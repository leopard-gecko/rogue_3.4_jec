/* Windows-native storage for Rogue 3.4 JEC.  No independent .lock files.
 * File identity checks, wide-character paths, byte-range locks, and atomic
 * save replacement live here; game and save format logic stay in save.c/rip.c.
 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  /* FileDispositionInfo (Vista+) */
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include "windows/rogue_windows_files.h"

struct rogue_windows_save_file {
    FILE *fp;
    BY_HANDLE_FILE_INFORMATION original;
    size_t length;
    time_t mtime;
};

static wchar_t *
wide_path(const char *utf8)
{
    int count;
    wchar_t *out;
    if (utf8 == NULL || *utf8 == '\0') return NULL;
    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                utf8, -1, NULL, 0);
    if (count <= 0) return NULL;
    out = (wchar_t *)malloc((size_t)count * sizeof(*out));
    if (out == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            utf8, -1, out, count) != count) {
        free(out);
        return NULL;
    }
    return out;
}

static HANDLE
stream_handle(FILE *fp)
{
    intptr_t h;
    if (fp == NULL) return INVALID_HANDLE_VALUE;
    h = _get_osfhandle(_fileno(fp));
    return h == -1 ? INVALID_HANDLE_VALUE : (HANDLE)h;
}

static FILE *
stream_from_handle(HANDLE h, int writing)
{
    int fd = _open_osfhandle((intptr_t)h,
                            _O_BINARY | (writing ? _O_RDWR : _O_RDONLY));
    FILE *fp;
    /* _open_osfhandle takes ownership only if it succeeds. */
    if (fd < 0) { CloseHandle(h); return NULL; }
    fp = _fdopen(fd, writing ? "r+b" : "rb");
    if (fp == NULL) { _close(fd); return NULL; }
    return fp;
}

static int
regular_file(HANDLE h, BY_HANDLE_FILE_INFORMATION *info)
{
    if (!GetFileInformationByHandle(h, info)) return 0;
    return (info->dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
                                      FILE_ATTRIBUTE_REPARSE_POINT)) == 0 &&
           info->nNumberOfLinks == 1;
}

static int
same_identity(const BY_HANDLE_FILE_INFORMATION *a,
              const BY_HANDLE_FILE_INFORMATION *b)
{
    return a->dwVolumeSerialNumber == b->dwVolumeSerialNumber &&
           a->nFileIndexHigh == b->nFileIndexHigh &&
           a->nFileIndexLow == b->nFileIndexLow &&
           a->nFileSizeHigh == b->nFileSizeHigh &&
           a->nFileSizeLow == b->nFileSizeLow &&
           a->ftLastWriteTime.dwHighDateTime == b->ftLastWriteTime.dwHighDateTime &&
           a->ftLastWriteTime.dwLowDateTime == b->ftLastWriteTime.dwLowDateTime;
}

/* The file remains open and locked until the game has validated its
 * checksum and complete state, and has atomically consumed this exact file.
 * Do not remove the pathname just because a read succeeded. */
struct rogue_windows_save_file *
rogue_windows_open_save(const char *path)
{
    struct rogue_windows_save_file *save = NULL;
    BY_HANDLE_FILE_INFORMATION info;
    wchar_t *wide = wide_path(path);
    HANDLE h;
    OVERLAPPED lock;
    FILE *fp;
    uint64_t timestamp;
    if (wide == NULL) return NULL;
    h = CreateFileW(wide, GENERIC_READ | DELETE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(wide);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    memset(&lock, 0, sizeof lock);
    if (!LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0, 1, 0, &lock) || !regular_file(h, &info) ||
        info.nFileSizeHigh != 0) {
        CloseHandle(h);
        return NULL;
    }
    fp = stream_from_handle(h, 0);
    if (fp == NULL) return NULL;
    save = (struct rogue_windows_save_file *)calloc(1, sizeof *save);
    if (save == NULL) { fclose(fp); return NULL; }
    save->fp = fp;
    save->original = info;
    save->length = (size_t)info.nFileSizeLow;
    timestamp = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) |
                (uint64_t)info.ftLastWriteTime.dwLowDateTime;
    save->mtime = timestamp / UINT64_C(10000000) >= UINT64_C(11644473600)
        ? (time_t)(timestamp / UINT64_C(10000000) - UINT64_C(11644473600))
        : (time_t)0;
    return save;
}

FILE *rogue_windows_save_stream(struct rogue_windows_save_file *save)
{ return save != NULL ? save->fp : NULL; }
size_t rogue_windows_save_size(const struct rogue_windows_save_file *save)
{ return save != NULL ? save->length : 0; }
time_t rogue_windows_save_mtime(const struct rogue_windows_save_file *save)
{ return save != NULL ? save->mtime : (time_t)0; }

int
rogue_windows_save_unchanged(const struct rogue_windows_save_file *save,
                             const char *path)
{
    BY_HANDLE_FILE_INFORMATION opened, current;
    wchar_t *wide;
    HANDLE path_handle;
    int ok;
    if (save == NULL || !regular_file(stream_handle(save->fp), &opened) ||
        !same_identity(&opened, &save->original)) return 0;
    wide = wide_path(path);
    if (wide == NULL) return 0;
    path_handle = CreateFileW(wide, FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    free(wide);
    if (path_handle == INVALID_HANDLE_VALUE) return 0;
    ok = regular_file(path_handle, &current) &&
         same_identity(&current, &opened);
    CloseHandle(path_handle);
    return ok;
}

int
rogue_windows_consume_save(struct rogue_windows_save_file *save,
                           const char *path)
{
    FILE_DISPOSITION_INFO disposition;
    if (!rogue_windows_save_unchanged(save, path)) return -1;
    disposition.DeleteFile = TRUE;
    /* Delete by the locked HANDLE, not the pathname, so a concurrent
     * replacement cannot cause a different player's save to be removed. */
    return SetFileInformationByHandle(stream_handle(save->fp),
                   FileDispositionInfo, &disposition, sizeof disposition)
               ? 0 : -1;
}

void
rogue_windows_close_save(struct rogue_windows_save_file *save)
{
    if (save == NULL) return;
    if (save->fp != NULL) fclose(save->fp); /* closes and releases lock */
    free(save);
}

int
rogue_windows_write_save(const char *path, const unsigned char *data,
                         size_t length)
{
    wchar_t *wide = wide_path(path);
    wchar_t *temp = NULL;
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD written;
    DWORD error;
    size_t n;
    int ok = 0, i, attempt;
    if (wide == NULL || data == NULL || length == 0 || length > 0x7fffffffu)
        goto done;
    n = wcslen(wide) + 48;
    if (n < 48 || n > 32767) goto done;
    temp = (wchar_t *)calloc(n, sizeof *temp);
    if (temp == NULL) goto done;

    /* CREATE_NEW obtains an unused file in the destination's directory
     * without a create/close/reopen race.  The suffix is not part of the
     * save format; no temp file is ever loaded as a game. */
    for (attempt = 0; attempt < 32; ++attempt) {
        if (swprintf(temp, n, L"%ls.r34-%lu-%lu-%d.tmp", wide,
                     (unsigned long)GetCurrentProcessId(),
                     (unsigned long)GetTickCount(), attempt) < 0)
            goto done;
        h = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
            goto done;
    }
    if (h == INVALID_HANDLE_VALUE) goto done;
    for (i = 0; (size_t)i < length; i += (int)written) {
        if (!WriteFile(h, data + i, (DWORD)(length - (size_t)i),
                       &written, NULL) || written == 0)
            goto done;
    }
    if (!FlushFileBuffers(h)) goto done;
    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(temp, wide,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        goto done;
    ok = 1;
done:
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (!ok && temp != NULL && temp[0] != L'\0') DeleteFileW(temp);
    free(temp);
    free(wide);
    if (!ok) errno = EIO;
    return ok ? 0 : -1;
}

int
rogue_windows_score_open(const char *path, int writing,
                         FILE **fp, long long *length, int *exists)
{
    wchar_t *wide = wide_path(path);
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE h;
    OVERLAPPED lock;
    DWORD err;
    int attempt, locked = 0;
    if (fp == NULL || length == NULL || exists == NULL) return -1;
    *fp = NULL;
    *exists = 0;
    *length = 0;
    if (wide == NULL) return -1;
    h = CreateFileW(wide, writing ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    writing ? OPEN_ALWAYS : OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    err = GetLastError();
    free(wide);
    if (h == INVALID_HANDLE_VALUE) {
        if (!writing && (err == ERROR_FILE_NOT_FOUND ||
                         err == ERROR_PATH_NOT_FOUND)) return 0;
        return -1;
    }
    memset(&lock, 0, sizeof lock);
    for (attempt = 0; attempt < 50; attempt++) {
        if (LockFileEx(h, (writing ? LOCKFILE_EXCLUSIVE_LOCK : 0) |
                         LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &lock)) {
            locked = 1;
            break;
        }
        err = GetLastError();
        if (err != ERROR_LOCK_VIOLATION && err != ERROR_IO_PENDING)
            break;
        Sleep(100);
    }
    if (!locked || !regular_file(h, &info) ||
        info.nFileSizeHigh != 0) {
        CloseHandle(h);
        return -1;
    }
    *length = (long long)info.nFileSizeLow;
    *fp = stream_from_handle(h, writing);
    if (*fp == NULL) return -1;
    *exists = 1;
    return 0;
}

int
rogue_windows_score_truncate(FILE *fp)
{
    if (fp == NULL || fflush(fp) != 0 ||
        _chsize_s(_fileno(fp), 0) != 0 || fseek(fp, 0, SEEK_SET) != 0)
        return -1;
    return 0;
}

int
rogue_windows_score_flush(FILE *fp)
{
    if (fp == NULL || fflush(fp) != 0 || _commit(_fileno(fp)) != 0)
        return -1;
    return 0;
}

void
rogue_windows_score_close(FILE *fp)
{
    if (fp != NULL) fclose(fp); /* byte lock released with the OS handle */
}
#endif /* _WIN32 */
