# Rogue 3.4 JEC — 移植・保守ノート

この文書は、macOS / Linux への移植記録と、日本語化・追加修正の段階別記録を整理した**現行仕様の技術資料**です。一般的なビルド・起動・操作は [README_ja.md](README_ja.md) または [README.md](README.md) を参照してください。前半は現在同梱しているソースの仕様、後半の `Windows port, stage N` 節は各移植段階の作業履歴です。

## 1. 基準と方針

- 基準は [rogueforge/rogue3.4](https://github.com/rogueforge/rogue3.4) の、1981年のRogue 3.4実行ファイルから**再構成された**ソースです。当時のオリジナルソースそのものではありません。
- 他バージョンからは、主に [Rogue 3.6 JEC](https://github.com/leopard-gecko/rogue_3.6_jec) の日本語訳・Unicode表示・現代的な端末処理を参考にしました。5.2・5.4 JECは主に巻き物名の長さ制限や画面表示の照合先です。
- **ゲームルールは3.4を基準とする**方針です。後のバージョンの品物・怪物・罠・ゲームルールを移植するものではありません。原典由来であっても、明白な未定義動作や破綻する処理は安全性のため修正しています。
- 英語の通常ゲームメッセージは原典の文言を基本とし、明らかな誤字・不完全な文章やOS対応上必要な案内を修正します。日本語は3.6 JECの訳語・文体を可能な範囲で共通化します。
- `src/rogue.6` と `src/rogue.cat` は歴史的資料として変更せず保存しています。案内書は `src/rogue_ja.*` と `src/rogue_en.*` のTXT・HTML・Markdown版を収録しています。

## 2. 配布ファイルの構成

| 場所 | 用途 |
| --- | --- |
| `src/*.c`, `src/rogue.h` | 原典由来のゲーム処理、OS移植、セーブ／スコアおよび表示処理 |
| `src/compat.c` | POSIX端末、表示遅延、保存パスの共通補助関数 |
| `src/globals.c` | グローバル変数の定義整理 |
| `src/jec/language.[ch]` | `ROGUEOPTS`・OSロケール・ゲーム中の言語切り替え |
| `src/jec/unicode_text.[ch]` | UTF-8と端末セル幅を考慮した入力／表示 |
| `src/jec/game_text.[ch]` | 日本語訳語、カタカナの巻き物名と色名 |
| `src/Makefile`・`src/Makefile.win` | macOS / Linux・Windows用ビルド、削除、Windows配布ZIP
| `src/Makefile.sources` | 全OS共通のゲーム本体ソース一覧（オブジェクト構成の重複を排除） |
| `.github/workflows/build.yml` | macOS / Linux のビルド成果物とWindowsネイティブビルド・配布ZIPの確認 |
| `README.md`・`README_ja.md` | 独立した日英の利用者向け説明 |
| `PORTING.md` | この技術資料（旧段階別文書を統合） |
| `LICENSE` | ライセンス |
| `src/windows/binary/README_ja.txt`・`README_en.txt` | Windowsバイナリ配布用文書 |

開発時に作成した `tests/` 以下の単体・操作テストファイルと、一時的な検証用curses宣言ヘッダーは**ソース配布に含めません**。テスト専用の関数はゲーム本体には残していません。`#define debug if (wizard) msg` などのウィザード関連処理は原典由来のゲーム機能であり、テスト用の残骸ではありません。`_Static_assert` は保存形式の前提をコンパイル時に確認する本体の安全策なので維持しています。

## 3. ビルドとファイル名

`src/` で `make` を実行します。macOSではシステムcurses、Linuxではncurseswとlibcryptにリンクします。Debian/Ubuntu/Raspberry Pi OSでは `build-essential libncurses-dev libcrypt-dev` を導入してください。追加の `./configure` は不要です。

| Make変数 | 意味 | 既定値 |
| --- | --- | --- |
| `PROG` / `PROGRAM` | 実行ファイル名（両方指定した場合 `PROG` 優先） | `rogue` |
| `SAVEFILE` | セーブファイル名 | `$(PROG).save` |
| `SCOREFILE` | スコアファイル名 | `$(PROG).scr` |
| `FILEDIR` | セーブ・スコア共通のディレクトリ | 未指定（カレントディレクトリ） |
| `SAVEDIR` / `SCOREDIR` | 個別の保存先（`FILEDIR` より優先） | `FILEDIR` または `.` |
| `DEFAULT_LANGUAGE` | 初期言語：`0` 自動、`1` 日本語、`2` 英語 | `0` |

例：

```sh
cd src
make PROGRAM=rogue34
# -> rogue34, ./rogue34.save, ./rogue34.scr

make PROG=rogue34 FILEDIR='~/Documents' \
  SAVEFILE=rogue34.save SCOREFILE=.rogue34.scr
# -> 保存は ~/Documents。スコアは先頭がドットの隠しファイル

make PROGRAM=rogue34 original     # 英語既定
make PROGRAM=rogue34 japanese     # 日本語既定
make PROGRAM=rogue34 clean
```

パスの `~` は**実行時のユーザー**のホームとして展開し、ビルド実施者のホーム絶対パスは埋め込みません。`SCOREFILE` は、先頭の `.` の有無で通常ファイル／隠しファイルを指定できます。保存先ディレクトリは自動作成されません。長すぎるパスを別の場所へ暗黙にフォールバックさせることはしません。ビルド設定は `.build-options` に記録し、指定が変わると必要なオブジェクトを再コンパイルします。

原典のスコア先は `/usr/public/.rogue_roll` ですが、現行版の既定値は実行時のカレントディレクトリにある `./rogue.scr` です。原典のファイル名をそのまま使う機能ではありません。

**保存先の既定値の変更（2026-11-23）：** `FILEDIR` 未指定時のセーブ・スコア保存先をホームディレクトリからカレントディレクトリ（`.`）へ統一しました。`SAVEDIR`・`SCOREDIR` を明示した場合は従来どおり優先し、`FILEDIR='~'` や `FILEDIR='~/Documents'` でホーム以下も指定できます。Windows第1段階時点ではセーブ・スコア機能は未実装でしたが、第4段階で対応しました。

## 4. 端末・C言語・OS移植

初期の移植段階から段階的に実施した主な変更を、機能別にまとめます。

- K&R形式の関数定義と古い宣言をC11互換のプロトタイプへ変更。`rogue.h`、`globals.c`、各コールバック型を整理し、廃止されたcurses内部構造や `crmode` 等への依存を公開APIへ置換しました。
- PDP-11時代のビルドルール、歴史的な共有計算機の負荷制限、動作に不要な互換処理を除去しました。`crypt()` を使うウィザード認証は維持しています。
- 内部ダンジョンを **80列×24行** に固定。端末が小さい場合は拡大されるまで待ち、サイズ変更・シェル復帰・端末停止／再開時には画面状態を復元します。
- `SIGWINCH` 等の非同期シグナル処理では、原則としてフラグを立てて安全な位置で反映します。`Ctrl-Z`、`!`、終了時の端末復帰を現代OSに合わせて処理します。
- `msg()`、`--More--`、所持品・オプション表示、墓標の文字列は、バッファ長と端末セル幅を守るよう整理しました。文字列を誤って `printf` の書式として解釈する危険も除去しています。
- 投擲物の見える移動と、`jump` が無効な連続移動の中間フレームには **30 ms** の表示遅延を使います。通常の1歩移動や戦闘計算・乱数には遅延を加えません。
- 起動直前の挨拶と、正常なセーブのファイル名案内は **1,000 ms** 表示します。通常のテキストメッセージには一律の遅延を入れず、従来の `--More--` 入力待ちを維持します。

### 表示遅延の考え方

怪物探知・魔法探知・地図など、入力待ちまたは継続表示を伴う効果には新たな一時停止を足していません。演出としてのウェイトは表示処理だけに限定し、ゲーム内ターンや乱数の進行には影響させません。

## 5. 原典のゲームルールと意図した安全修正

3.4の怪物26種類・武器12種類・よろい8種類、巻き物と水薬の出現設定、基本攻撃の命中文言4種類など、対応するデータを保持しています。**3.4の `w` では武器以外の品物も手に持てる**ことが、3.6以降との重要な差です。日本語表示だけをそろえるために、この動作は変えていません。`w` の完了メッセージには日本語・英語とも所持品番号を追加しません。

安全性や明白な不具合に対する修正は、ゲームの再調整と区別します。

| 分野 | 現行版での対策と意図 |
| --- | --- |
| 乱数 | 32ビット演算の未定義の符号付きオーバーフローを避け、原典の下位ビットの動作を維持 |
| ダンジョン配置 | 原典の再抽選を乱数周期の限界まで続け、その後に限り決定的な空きマス探索を使って無限ループを防止 |
| 通路走行 | `look()` の通路判定用カウンターを毎回初期化。途中の段階で導入された累積不具合を修正 |
| 放浪怪物 | すでに怪物がいるマスへ重ねて生成しない |
| イベントとリスト | `do_daemons()` の呼び出し中に自身の要素が削除される場合の解放済み領域参照を修正 |
| 品物・魔法 | 装備品や識別情報の境界、怪物が保持する品物の寿命、呪われた品物の扱い等を検証し修正 |
| 混乱の水薬 | すでに混乱しているときは効果時間を延長するよう、到達不能な原典の分岐を修正 |
| 怪物作成の巻き物 | 個数カウンターを `_Bool` ではなく `int` として扱い、抽選の意図を維持 |
| 投射方向 | 混乱時の零方向を再抽選して停止を防止 |
| 数値入力 | 極端に大きい移動回数や範囲外の配列参照を安全に扱う |
| `w` と巻き物 | 武器ではない品物を持ったまま武器強化の巻き物を使う際にも配列外参照をしない |

すべての種・行動で原典と完全に同じ乱数列になることを保証するものではありません。特に配置の最終フォールバック、既存の不具合修正、次節の巻き物題名制限は動作差になり得ます。後期Rogueの新ルールは追加していません。

## 6. 日本語化・Unicode・英語表示

### 言語の決定

初期表示はOSロケールを使い、`make original` は英語、`make japanese` は日本語を既定にします。`ROGUEOPTS='language=ja'`／`language=en`／`language=auto`、またはゲーム中の `o` で切り替えます。`ROGUE_LANG` は使用しません。言語は表示状態であり、アイテムの識別情報など原典由来のゲームデータとは分離しています。

### 名称とメッセージ

- 怪物・武器・よろい・水薬・巻き物、戦闘・探索・ヘルプの訳語を、3.6 JECと可能な範囲で統一しました（例：**菫の大キノコ、小悪魔、褐色の大あご虫、投げ矢**）。武器名の `dart` と罠による死因「毒投げ矢」を区別します。
- `i` の空の持ちものは「何も持っていない。」、`I` の通常案内は「調べる持ちものの種類は？」、簡潔案内は「調べる持ちものは？」です。無効キーに対する案内やオプション入力案内も表示できるようにしています。
- `w` は「武器を%sに持ちかえた。」（簡潔表示は「%sに持ちかえた。」）を使いますが、**所持品番号は追加しません**。英語は原典の `You are now wielding %s`／`Wielding %s` を維持します。
- `T` で着用中のよろいがないときは原典の未完英文 `You aren't wearing any` を `You aren't wearing any armor` と修正し、`terse` では `Not wearing armor` にします。
- 罠発見は「とらばさみのわな」などを3.6と統一し、英語の `posion dart trap` という原典由来の誤記のみ `poison dart trap` に訂正しました。
- `v` は日英ともに `Rogue version 3.4. (mctesq was here)` を表示します。原典の英語ステータスは強さ `18/7` の後の桁合わせも維持します。3.4には空腹状態の常時ステータス欄はなく、上部メッセージで警告します。
- 3.4の英語で操作別に異なる呪いのメッセージは維持します。英語の通常表示は原典に近づけていますが、セーブの安全対策、墓標年号、異常系の案内など必要な変更は維持します。

### 文字幅と巻き物

名前・好きな果物・品物の呼び名・ファイル名はUTF-8の入力／表示を考慮し、Backspace・左右移動・Deleteでは文字の途中を壊さないようにします。端末標準で正しく表示できない特殊絵文字は対象外です。墓標は日本語の「安らかに」「眠れ」と死因を中央寄せで表示し、英語の `REST / IN / PEACE` を維持します。

未識別巻き物の題名は原典の**英語の音節列**を保存し、画面表示時に日本語のカタカナへ近似変換します。3.6の音節ID方式には変更していません。題名は生成時に、日英どちらもUTF-8で**40バイト以内**となるよう制限します。表示時の旧セーブ用の強制省略はありません。この上限制御により、新しい巻き物題名生成時の乱数消費は改修前と異なる場合があります。拾得メッセージの持ちもの記号は「あなたは短弓 (o)を手に入れた。」の位置で表示します。

### 原典のメッセージ・操作に合わせた調整（2026-11-23）

- `S` の確認は原典どおり英語で `Save file ("...")?` と表示します。`y` は既定名で保存、`n` は別名入力に進み、ファイル名入力中のEscで中止できます。保存先でI/Oエラーが起きた場合は、原典と同様に別の名前を再入力できます。データの直列化（`R34SAV02`）、チェックサム、一時ファイル＋`rename()` の安全対策は維持します。
- 再開後は `Game restored.` の代わりにファイル名と保存ファイルの更新日時を表示します。**英語は原典と同じ `Mon Nov 23 10:30:00 2026` 形式、日本語は `2026年11月23日（月）10:30:00` 形式**です。長いUTF-8パスは**表示だけ**短縮し、日時を表示する余地を残します。
- `o` の一覧から `("terse")` などの設定キーの併記を日英とも取り除き、入力位置も見出しのセル幅に合わせます。`ROGUEOPTS`での指定キー自体は引き続き有効です。
- 英語の継続表示は原典どおり `--More--` とし、存在しないスコアファイルを `-s` で参照したときは見出しを出力しません（ファイルも作成しません）。シェル起動の `exec` 失敗には原典の `No shelly` を使用します。OS依存の別種のエラーは引き続き区別します。
- 【履歴】Windows第2段階時点では、保存・再開とスコアのファイル操作は無効でした。その後の**第4段階で実装し、Windows実機での正常動作が確認**されています。

## 7. セーブとスコア

- セーブ形式は**`R34SAV02`**。当時の生メモリ保存形式、開発中の `R34SAV01`、他バージョンの形式とは互換性がありません。未公開段階の旧ファイルを救済する処理はありません。
- プレイヤー、怪物、所持品、識別情報、階層・罠・マップ、乱数の状態、デーモン／フューズなどを独立したデータとして保存し、ネイティブのポインター値や構造体配置に依存しません。巻き物名は日英40バイトの条件も検証します。`look()` の毎回初期化されるカウンターは保存しません。
- セーブは同一ディレクトリの一時ファイルに書き、成功時に置換する方式です。ファイル名は `ROGUEOPTS` の `file=` またはゲーム内 `o` で変更できます。正常に再開すると使用したセーブファイルは削除します（ウィザードモードは例外）。破損・不適合ファイルは勝手に修正・削除しません。
- スコアはポータブルな専用形式を採用し、**上位10件**の記録を保持します。`./rogue -s` は読み取り専用で、新規ファイルを作成しません。誤った形式や破損を自動で上書きしません。
- セーブ・スコアの排他制御は対象ファイル自身のロックを使い、**別の `.lock` ファイルは作成しません**。Unixは `flock()`、Windowsは `LockFileEx()` を用い、初回作成時の競合にも配慮します。
- 既定の保存先は実行時のカレントディレクトリです。ビルド時の `SAVEFILE`・`SCOREFILE`・各ディレクトリは第3節のとおりです。通常のスコア名は隠しファイルではありません。

## 8. 検証済み事項・今後の注意

移植各段階で、C11ビルド、AddressSanitizer/UndefinedBehaviorSanitizer、Linux 80×24疑似端末での起動、操作、セーブ／再開、スコア表示、言語切り替え、文字幅・巻き物名などを検証してきました。過去のLinux検証では、開発用cursesヘッダーがない環境に限って一時的な宣言ヘッダーとシステム実行ライブラリを使用した回もあります。**そのヘッダーや計測用コードは配布していません**。GitHub Actionsは現在、複数環境での**ビルド**を担当し、個別の開発用テストスクリプトは同梱しません。

- すべての原典ゲーム行動に対するビット単位の比較や、全端末での視覚効果の同一性までは保証していません。
- 現行の日本語版を含む最新の状態について、Mac実機とRaspberry Pi実機での全操作の網羅的な再検証は別途必要です。
- 原典からのゲーム動作差を調査するときは、英語の文言差、日本語化による表示差、意図した安全修正、意図しないルール差を区別してください。
- 今後の修正でも、ゲームの本体関数やウィザード機能を、単なるテスト用コードと誤認して削除しないでください。

## 9. この整理で行ったこと

2026-11-23に、分散していた段階別README、個別修正レポート、旧版の技術履歴をこの1文書に統合しました。旧セーブ形式や旧スコアファイル名の説明が「現行仕様」と混在しないよう、現在の `R34SAV02`、`PROG`／`PROGRAM`、`$(PROG).scr` を基準にしました。`tests/` の専用スクリプトとCテストファイルを除去し、CIはビルドと成果物作成に整理しました。原典の歴史的資料やゲーム本体の関数は保持しています。

## Windows port, stage 1 (2026-11-23)

**Historical status:** this was the build-foundation stage, before Windows save, Unicode, and launch features were implemented.
The macOS/Linux game and its storage implementations are retained.

- The parent `src/Makefile` selects `src/Makefile.win` in MSYS2 UCRT64;
  it keeps the Unix recipe unchanged otherwise. Windows generates `.win.o`
  objects and a `.exe` binary. `PROG` and `PROGRAM` select the same name;
  build-time save/score file names remain derived from that name.
- `compat.c` retains the portable path construction and glyph decoding.
  Unix terminal attributes, signals, terminal-size checks, curses redraw and
  suspend/resume code remain within the Unix branch. Windows-specific
  platform functions are in `src/windows/rogue_windows.c`.
- Windows obtains `USERPROFILE`, `ROGUEOPTS`, and the account name using
  wide-character Win32 APIs and converts these values to UTF-8. The current
  process ID is obtained using the Windows API, without POSIX `getpid()`.
- To avoid unsafe/unlocked storage, Windows stage 1 **does not write saves or
  score files and does not restore saves**. These operations return an error
  instead of silently dropping or corrupting data. The in-memory serializer
  and the on-disk save/score formats remain unchanged. Windows file locking,
  safe replacement and Unicode paths are scheduled for stage 4.
- Windows wizard-password authentication remains disabled, to be enabled in
  stage 5 with proper console input. The POSIX crypt/getpass flow is retained
  unchanged on Unix. Windows console resizing and Unicode editing are NOT
  complete in this stage; the temporary `wgetch()` path is for build bring-up
  only, not the final keyboard implementation.
- The Windows job in GitHub Actions only compiles and links; it deliberately
  does not publish an executable for download or run tests. No dedicated
  tests or mock system headers are included in the repository ZIP.
- Windows build-warning cleanup (2026-11-23): `save.c` excludes the
  R34SAV02 codec from stage-1 Windows compilation because both Windows
  save/restore entry points are deliberately disabled. All codec code is
  preserved for reuse in stage 4, and the Unix save path is unchanged.
  `windows/rogue_windows.c` uses `NOMINMAX` for Win32/Rogue `max` and
  releases Win32's `MOUSE_MOVED` macro before PDCurses declares its own.
  This changes header hygiene only; mouse and gameplay behavior do not
  change. Native Windows validation is still required.

### Windows build prerequisites and commands

Open **MSYS2 UCRT64** and install:

```sh
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-pdcurses
```

From the `src/` directory, use:

```sh
make PROGRAM=rogue34
make PROGRAM=rogue34 original
make PROG=rogue34 japanese
```

These examples create `rogue34.exe` using PDCursesMod's VT backend. Both
`PROG` and `PROGRAM` derive default `rogue34.save` / `rogue34.scr` names.
The default data path is the process working directory (`.`), so an
explicit `FILEDIR=.` is unnecessary. The build intentionally
has no compiled-in developer HOME directory. No Windows release should be
made until stage 2-5 functionality and native Windows testing pass.

### Stage 4 / 5 plan (historical; both stages implemented below)

1. **Stage 4:** Windows-safe score/save file creation, lock and atomic
   write/restore, Unicode filename I/O, score display after direct launch.
2. **Stage 5:** Wizard password (including one-character input and Ctrl-C),
   shell escape, Explorer launch menu and score screen return-key waiting.

Stage 2 adds basic direct-launch sizing, 80x24 screen handling and ASCII
input. It does not complete the above later stages.

## Windows port, stage 2 (2026-11-23)

- `src/windows/rogue_windows_console.c` owns direct-launch recognition, VT
  80x24 size requests, Console Host buffer/viewport resizing and plain-screen
  clearing. It uses the established Windows 3.6 JEC approach. A shell parent
  (Command Prompt, PowerShell, MSYS2/mintty, WSL etc.) is never resized.
  `WT_SESSION` is intentionally **not** used for frontend detection; Windows
  Terminal ConPTY can lack it. Resize is retried after the greeting, because
  the GUI frontend may initialize asynchronously.
- `src/windows/rogue_windows.c` keeps `cw`, `mw`, `hw` at 80x24 and records the
  most recently presented curses window. While a help/inventory/options
  overlay is shown, `readchar()` must read from that window instead of
  refreshing `cw` over it. The Windows input wait uses PDCurses `wgetch()`
  with a 100 ms timeout so interrupts and window-size changes are processed.
  Unrecognised special keys are ignored instead of being truncated into
  original Rogue commands. The original `h/j/k/l/y/u/b/n` keys are retained.
- For a viewport smaller than 80x24 during play, the Windows game exits curses
  and waits without taking a turn; it repaints the visible window after the
  viewport is enlarged. Original Unix signal/resize handling is untouched.
- Windows code calls `SetConsoleCP(CP_UTF8)` / `SetConsoleOutputCP(CP_UTF8)`
  when attached to a Win32 console. Full Japanese/emoji input editing,
  surrogate-pair handling and supplementary-plane repaint are **stage 3**;
  stage 2 guarantees only the basic PDCurses key path and frame geometry.
- Saving, score recording, restoring, wizard authentication and Explorer
  launch menus remain **disabled/pending** until their previously planned
  stages. The Windows job in GitHub Actions remains compile-and-link only;
  it does not publish a Windows executable as a release.
- After building on Windows, check Explorer direct launch separately under
  Windows Terminal and Console Host, shell launch (window must not resize),
  basic movement, `?`/`i`/`o` overlays, Ctrl-C and narrow terminal recovery.
  Real Windows execution and visual behavior must be confirmed on Windows.


## Windows port, stage 3 (2026-11-23)

- Unicode input for option strings uses the design implemented in Rogue 3.6
  JEC. `rogue_windows_edit_key()` reads UTF-16 `KEY_EVENT_RECORD`s from
  Console Host/Windows Terminal and combines surrogate pairs into a Unicode
  scalar. When stdin is a mintty pipe, UTF-8 bytes are decoded directly;
  ESC/key sequences are consumed without treating them as text. The ordinary
  game command reader remains PDCurses `wgetch()`, preserving stage 2
  movement and shell/overlay behavior.
- `options.c` converts the scalar into UTF-8 for the original grapheme-aware
  string editor. Name, fruit, call-name and save-path fields all use the same
  editor. Left/right, Delete, Backspace and Esc act on the intended editing
  unit; characters already unsupported by the terminal remain out of scope.
  The Windows file APIs themselves are still planned for stage 4.
- `windows/unicode_output.c` adapts the Rogue 3.6 VT rendering workaround:
  non-BMP Unicode scalars reserve two cells in PDCursesMod and are repainted
  with their original UTF-8 sequences after a normal curses refresh. The
  wrappers cover `waddstr`, `mvwaddstr`, formatted window text and the normal
  screen-clear paths. The Unix code uses its existing curses functions.
- Stage 3 also adopts Rogue 3.6 JEC's cursor flicker fix. VT cursor hide is
  sent **immediately before** the Windows refresh, and VT cursor show is
  sent **immediately after** it (including refresh failures), with stdio
  flushing around Win32 writes. We deliberately do not call `curs_set(0)`:
  the player's cursor must remain visible between movement updates.
- Current stage-3 checks: GCC/Clang `-Werror` syntax validation of the
  modified Windows sources with temporary Win32/curses declaration stubs;
  mock VT-renderer execution for a BMP Japanese string plus a non-BMP emoji,
  including cursor restoration and screen erasure. Stubs/tests are kept
  outside the distributed source tree. Native MSYS2 compilation and Windows
  Terminal/Console Host/mintty display tests must be performed on Windows.
- No changes to game rules, English original messages, RNG, on-disk save
  `R34SAV02` or score layout. Windows save/score storage and wizard login
  remain disabled until their respective stages. Stage 3 introduces no
  additional public binary distribution targets.


## Windows stage 3: UTF-8 locale correction (2026-11-23)

- The first Windows 3.4 stage-3 build used `setlocale(LC_CTYPE, "")`, which
  chooses the Windows system ANSI locale on Japanese installations.  This
  conflicts with the UTF-8 bytes passed to PDCursesMod and causes garbled
  Japanese status labels and stray visible cursor-motion sequences such as
  `[24;3H`.  `SetConsoleOutputCP(CP_UTF8)` by itself does **not** select the
  multibyte locale used by the C runtime and curses library.
- As in Rogue 3.6 JEC, startup now selects the Windows UCRT `.UTF8` locale
  before curses initialization and regardless of the selected game language.
  A `C.UTF-8` fallback is allowed; when neither locale is available the game
  exits with an explicit diagnostic instead of drawing a corrupted dungeon.
- Unix `LC_CTYPE` initialization, the saved game format, the Unicode editor,
  console input routes and the VT cursor-flicker workaround are unchanged.
- The fix should be validated on Japanese-language Windows Terminal and
  Console Host with both `make original` and normal Japanese startup, including
  the single 24th-row status line, no stray `[24;...H`, and `i`/`o` overlays.

## Windows port, stage 4: native save/restore and scores (2026-11-23)

Stage 3 Japanese/emoji display and cursor behavior were confirmed on
Windows by the maintainer. The stage-4 changes restore the common R34SAV02
serializer in Windows builds and add only the OS-specific file operations.

- `src/windows/rogue_windows_files.c` handles UTF-8 to UTF-16 pathname
  conversion, `CreateFileW`/`FILE_FLAG_OPEN_REPARSE_POINT`, native byte-range
  locks (`LockFileEx`) and file identity checks. It does not rely on POSIX
  `flock()`, `mkstemp()`, `unlink()` or narrow-charset `fopen()`.
- Windows writes the existing `R34SAV02` bytes to a `CREATE_NEW` temporary
  file in the destination directory, flushes them using `FlushFileBuffers`,
  closes the handle, then replaces the destination using `MoveFileExW` and
  `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`. Failed replacements
  never truncate the existing save. Temporary files are removed on failure.
- Restoring opens and holds a nonblocking exclusive byte-range lock on
  the save, rejects reparse points, directories and hard-linked files, and
  requires the on-disk size and checksum to match the validated decoder.
  Before consumption it rechecks the opened handle's identity against the
  current pathname and uses `SetFileInformationByHandle(FileDispositionInfo)`
  on the locked handle, not `DeleteFileW` on an unchecked pathname. Invalid
  or replaced files remain untouched; wizard restores retain saves as on Unix.
- Score files retain `ROGUE34-SCORES-1`, the same 10-entry ordering, name
  hex encoding and read-only `-s` behavior on all operating systems. Windows
  scores acquire shared (read) or exclusive (update) locks directly on the
  score file, keep the lock through parse/update/flush and commit via
  `_chsize_s` and `_commit`. No `.lock` files are created.
- Windows drive-absolute and UNC paths override the configured save directory;
  `SAVEDIR`, `SCOREDIR` and `FILEDIR` remain runtime directory selections.
  The default for both data files is still the working directory (`.`).
- No change to gameplay, RNG, original English messages, Unicode editing,
  VT repaint, cursor flicker handling or save/score magic strings. Native
  Windows compilation/link and file operation verification must be performed
  in MSYS2 UCRT64 or GitHub Actions; Unix builds and synthetic Windows
  declaration-based syntax checks alone do not prove OS-level behavior.
- **Historical stage-4 status:** Wizard authentication and Explorer-specific
  start/end behavior were deferred to stage 5, documented below.
- For command-line saves and Explorer drag-and-drop, the Windows runtime
  derives the restore argument from `GetCommandLineW`/`CommandLineToArgvW`
  and converts it to UTF-8. Narrow UCRT `argv` might otherwise have been
  decoded in the machine's ANSI code page before `.UTF8` locale selection.

## Windows port, stage 5 (2026-11-23)

The user confirmed stage 4's Windows save/restore/score behavior. This stage
continues without changing `R34SAV02` or `ROGUE34-SCORES-1`.

- Common `save.c` now formats restore **file modification times** as
  `YYYY年M月D日（曜）HH:MM:SS` when Japanese is selected. English retains
  the original ctime order, without the trailing newline. The formatter
  uses `localtime()` and an explicit weekday table, so a Japanese system
  locale is not required. It abbreviates a long file path only for display,
  keeping the datetime within the 80-column message area.
- Windows `main.c` now shows a menu only for directly launched Explorer
  sessions with an existing configured default save. `R`/Enter resumes,
  `N` starts new, `F` chooses a Unicode filename via `GetOpenFileNameW`,
  and `Q` quits. Explicit `-r`/filename arguments and existing shells
  bypass the menu; no default save skips the menu and starts new.
- Windows game launch and in-game Ctrl-P wizard authentication use the
  **original Rogue 3.4 hash**. Shared `wizard_password_ok()` uses the
  host's `crypt()` on Unix and the FreeSec implementation on Windows.
  `src/windows/xcrypt.c` is taken from the working Rogue 3.6 JEC port and
  retains David Burren's 1994 license. Password input never echoes; at
  runtime it stays on the current curses input path instead of mixing
  buffered CRT stdin and curses keyboard input.
- After curses ends, Windows console Return input uses the platform
  Unicode key reader. A direct-launched game waits after the final score
  display and on a restore failure; normal command-shell launches do not.
  The same platform input reader handles intermediate non-curses Return
  prompts in the victory/shell paths.
- The previous Windows VT Unicode and cursor-flicker code, 80x24 fixed
  dungeon, score lock/safe file replacement, language settings and working
  directory defaults are preserved.
- Source distribution includes no test-only files. Native Windows
  compile/link and interactive behavior still require verification with
  MSYS2 UCRT64/PDCursesMod; the available compilation checks use mock
  Windows API declarations. The user has already confirmed stage 4.


## Windows port, stage 6-A: final verification (2026-11-23)

**Status:** The maintainer confirmed the stage-5 features on real Windows,
including the Japanese restore timestamp, direct-launch save menu, wizard
password entry, and direct-launch score-screen pause. This stage focuses
on reproducible verification; no game rules, English/Japanese strings,
`R34SAV02` fields, score format, or OS file APIs were changed.

- Built all Unix game sources with GCC and the installed ncurses runtime,
  using temporary curses declarations because this verification environment
  lacks the development headers. The same sources passed stage-5 regression
  checks in the 80x24 terminal.
- Compiled **32 Windows-path C translation units**, including the Windows
  file/console/Unicode layer, against temporary Win32/PDCurses declarations
  with GCC C11 `-Wall -Wextra -Wpedantic -Werror`. This is a source-level
  check, **not a substitute for a native MinGW-w64 link or field test**.
- In Unix 80x24 pseudo-terminal tests, verified: no creation on score-only
  `-s` when the file is missing; UTF-8 and emoji names in score records;
  preserving damaged score and save files; writing `R34SAV02` in the current
  working directory; verifying the recorded payload size and FNV-1a checksum;
  Japanese-to-English and English-to-Japanese save/restore, with deletion of
  a successfully consumed save; detection of a deliberately altered save;
  and 18 simultaneous read-only score processes, with file bytes unchanged.
- Confirmed `make PROGRAM=rogueqa original`, configurable save/score
  filenames and directory, and full object recompilation on changed
  `SAVEFILE`/`SCOREFILE`. Windows Makefile expansion also passes those
  parameters into the compiler; new native CI assertions verify the score
  path and the two explicitly selected default languages.
- Removed `src/rogue_windows_files.plist`: a leftover compiler static-analysis
  result with no role in the game, build, or distribution. Verification-only
  scripts, mock system headers, and object files remain outside the ZIP.

**Unverified in this environment:** transfer of an actual `R34SAV02` save
between Windows and macOS/Linux; concurrent Windows writers and restore
attempts using real `LockFileEx`; lengthy interactive Windows Terminal / 
Console Host testing. Both platforms use the same fixed-width save codec
and textual score format by source inspection, but actual cross-platform
interchange cannot be claimed without real files or a Windows runtime.
The stage-6-B binary distribution procedure has not yet been implemented.


## Windows cursor shape restoration after PDCursesMod endwin (2026-11-23)

PDCursesMod's VT backend (`vt/pdcscrn.c: PDC_scr_close`) invokes
`PDC_curs_set(2)` at shutdown, sending a blinking-block cursor shape.
This is independent of the Rogue 3.6 JEC-derived flicker fix, which only
uses `CSI ?25l`/`CSI ?25h` to hide/show the cursor during a refresh.

The Windows-only `rogue_windows_endwin()` wrapper now calls the genuine
PDCurses `endwin()` first, then sends `CSI 0 SP q` (DECSCUSR: use terminal
configured default shape).  The PDCurses VT shutdown may restore console
modes that disable VT parsing; the wrapper temporarily enables processed
VT output when necessary and restores the previous console mode afterward.
It refuses to send these terminal bytes to redirected output or pipes.
`rogue.h` redirects Windows game calls to the wrapper; Unix calls to
`endwin()` are unchanged.  The same path also covers quit, death, victory,
save-and-exit, failed restore after curses initialization, Windows resize
suspension, signals that close curses, and the `!` temporary shell command.
The routine doesn't change the cursor shape during normal in-game drawing.

`CSI 0 SP q` requests the configured default, not an arbitrary user-supplied
custom shape saved separately by a shell program.  This is the intended
Windows Terminal/Console Host behavior; as with other VT functionality,
actual user-visible behavior needs Windows Terminal and Console Host checks.


## MSYS2 UCRT64 build warning cleanup (2026-11-23)

The user's native PDCursesMod headers define `endwin` as an ABI-specific
function alias (for example `endwin_u64_4400`).  The Windows cursor-shape
wrapper originally defined `endwin()` again without undefining that alias,
causing `warning: 'endwin' redefined` in every game translation unit.
`src/rogue.h` now `#undef`s the PDCurses macro *after* including curses.h
and then defines the game-only wrapper; the wrapper implementation in
`windows/rogue_windows_console.c` continues to call the real PDCurses
`endwin()` through its own `<curses.h>` alias, before resetting the cursor.
This only affects Windows builds, and does not change terminal behavior.

The Windows `.windows-build-options` stamp no longer invokes `cmp`, which
may be absent in a minimal MSYS2 installation; it uses POSIX shell `test`
and `cat` (from coreutils, already needed for the existing `rm` and `mv`
commands).  Identical build options still keep the stamp unchanged; changed
options still force recompilation.  Unix build rules are unchanged.


## Windows port, stage 6-B: binary distribution (2026-11-24)

The MSYS2 UCRT64 `make dist.win` target now produces one archive,
`src/rogue34_jec_windows.zip`, with top-level folder `Rogue 3.4/`.
Its five files are: `rogue34.exe` (normal, language auto),
`rogue34_original.exe` (English startup language), `README_ja.txt`,
`README_en.txt` and `LICENSE.txt`. These binaries deliberately use the
same `rogue34.save` and `rogue34.scr` names, and the original variant does
not change the game mechanics. They are compiled sequentially through the
existing option-stamp mechanism, with the normal EXE built last so the
`src/` build product is not left in original mode. Temporary staging files
are removed on successful and failed builds. No game source, save codec,
score layout, or Unix build rule was changed in this stage.

This distribution target requires the MSYS2 `zip` and MinGW `objdump`
commands. It inspects imports of **both** EXEs and refuses to create an
archive if a non-system DLL import is seen, rather than assuming the
user already has PDCurses or MinGW DLLs installed. The PDCursesMod
package provides `libpdcurses_vt.a`, used for the normal static link.
The root `LICENSE` is copied without modification, covering FreeSec.

At this stage the distribution workflow can be inspected and dry-run from
Unix, but it has **not** been built and executed on a real Windows host.
Stage 6-C adds Windows CI build, packaging and import/ZIP
validation; stage 6-D is reserved for final source and documentation consolidation.

## Windows port, stage 6-C: GitHub Actions distribution verification (2026-11-24)

既存の `.github/workflows/build.yml` のWindows UCRT64ジョブに、
第6-Aのビルド・設定変更・セーブ／スコア検査を保持したまま、
第6-Bの `make dist.win` を呼び出す処理を追加しました。
PDCursesMod VTの静的ライブラリの存在を確認し、ZIPを検査した後で
Windows用アーティファクト `rogue34-windows-ucrt64` としてアップロードします。

CIで新たに確認する項目は以下のとおりです。

- ZIP自体の破損がないこと、トップレベルが `Rogue 3.4/` であること、
  内容が2個のEXE・日英README・LICENSEの計5ファイルに一致すること。
- 展開した両EXEのPE形式が64-bit Windows用で、直接インポートに
  PDCursesやMSYS2由来のランタイムDLLが含まれていないこと。
  全DLLの許可判定は第6-Bの `make dist.win` 側でも実行する。
- 既定の言語について、通常版は日本語ロケールに従い日本語、
  `rogue34_original.exe` は日本語ロケール下でも英語で表示すること。
- `ROGUEOPTS` で両EXEの言語を切り替えられ、
  どちらも `rogue34.scr` を参照すること。
- `LICENSE.txt` がルートの `LICENSE` と同一であること。

`actions/checkout@v7`、`actions/upload-artifact@v7`、
`msys2/setup-msys2@v2` を維持し、新たに `zip`・`unzip` と、
検証に使う `cmp` のための `diffutils` をCIのパッケージ一覧に追加しました。CI成果物はGitHub Actionsの
Artifactsからダウンロードでき、Releaseへの公開は自動で行いません。
**この作業環境ではGitHub Actionsの実行までは行っていません。**
対話式の画面表示、カーソル復元、端末サイズ変更、セーブの実操作は
引き続きWindows実機での確認が必要です。ゲーム本体・セーブ形式・
配布ZIPの作成処理には変更を加えていません。

## Windows port, stage 6-D: source and documentation consolidation (2026-11-24)

- `src/Makefile.sources` に共通ゲームソース一覧を集約し、macOS/Linuxと
  Windowsの双方が参照するようにしました。Windows専用ソースは従来どおり
  `Makefile.win` に限定し、`make -f Makefile.win` でも動作します。
- GitHub Actionsのテスト内容・成果物名は維持し、段階番号に依存したジョブ名と
  一時フォルダ名を一般的な名称に整理しました。
- 日英READMEのWindows節は現行仕様を中心にまとめ、段階ごとの詳細は
  本書の移植記録へ残しました。Windows側のソースコメントも現状に合わせています。
- **ゲームルール・乱数・セーブ／スコア形式・カーソル制御の処理自体は変更していません。**
  第6-C段階のWindows実機・GitHub Actionsでの動作確認を維持するため、
  リファクタリングはビルドの重複解消とコメント・文書の整理に限定しました。


## Windows distribution guides (2026-11-25)

`make dist.win` now includes `rogue_ja.txt`, `rogue_ja.html`,
`rogue_en.txt` and `rogue_en.html` alongside the two EXEs, two READMEs
and `LICENSE.txt` under `Rogue 3.4/`. Markdown editions remain in the
source distribution. GitHub Actions checks the exact nine-file payload
and compares the four included guides against their source files.
No game logic, build flags or save/score formats were changed.


## Windows distribution: single-executable package (2026-11-25)

`make dist.win` now includes only `rogue34.exe` (automatic startup
language), two Windows READMEs, four TXT/HTML guides adapted for Windows,
and `LICENSE.txt` under `Rogue 3.4/` (eight files total).
`rogue34_original.exe` is no longer built or included by `dist.win`.
The earlier stage 6-B/6-C descriptions above are historical records.

The standalone `make original`/`make english` target remains available to
verify original-style English startup without shipping a separate EXE.
GitHub Actions checks the eight-file ZIP and tests both language settings
using the one packaged executable. No gameplay or save-format changes.
