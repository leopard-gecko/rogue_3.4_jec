#ifndef ROGUE34_WINDOWS_FILES_H
#define ROGUE34_WINDOWS_FILES_H
/* Windows-only UTF-8 paths, transactional saves and locked score access.
 * The on-disk R34SAV02 and ROGUE34-SCORES-1 formats remain platform-neutral.
 */
#ifdef _WIN32
#include <stddef.h>
#include <stdio.h>
#include <time.h>

struct rogue_windows_save_file;

int rogue_windows_write_save(const char *path, const unsigned char *data,
                             size_t length);
struct rogue_windows_save_file *rogue_windows_open_save(const char *path);
FILE *rogue_windows_save_stream(struct rogue_windows_save_file *save);
size_t rogue_windows_save_size(const struct rogue_windows_save_file *save);
time_t rogue_windows_save_mtime(const struct rogue_windows_save_file *save);
int rogue_windows_save_unchanged(const struct rogue_windows_save_file *save,
                                 const char *path);
int rogue_windows_consume_save(struct rogue_windows_save_file *save,
                               const char *path);
void rogue_windows_close_save(struct rogue_windows_save_file *save);

/* Returns 0 on success, including missing read-only score files, otherwise -1.
 * An open stream holds a byte-range lock until rogue_windows_score_close(). */
int rogue_windows_score_open(const char *path, int writing,
                             FILE **fp, long long *length, int *exists);
int rogue_windows_score_truncate(FILE *fp);
int rogue_windows_score_flush(FILE *fp);
void rogue_windows_score_close(FILE *fp);
#endif
#endif
