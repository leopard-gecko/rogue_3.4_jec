# Rogue 3.4 JEC — macOS / Linux / Windows 移植版

本プロジェクトは、1981年のBerkeley版Rogue 3.4を復元したソースコードを、現代のC11コンパイラと端末環境で動作するように移植したものです。

[English README](README.md)

## 基準としたソースコード

本移植版は、Michael Toy、Kenneth Arnold、Glenn Wichmanの3氏が
1981年に開発したRogue 3.4を再構成したソースコード
（[rogueforge/rogue3.4](https://github.com/rogueforge/rogue3.4)）を
基準としています。

この再構成ソースは、当時の実行ファイルなどを基に復元されたもので、
1981年当時のオリジナルソースそのものではありません。復元プロジェクトの
READMEによると、修正した2.8BSDのツールチェーンでビルドすると、
CSRG Archive CD-ROM Disc 1の `2.8/usr/bin.v7/ucb/rogue` に
収録された実行ファイルとバイト単位で一致します
（MD5: `88f791623b2a684d2555891c0f5b6735`）。

歴史的資料の `rogue.doc` は `2.8/usr/doc/rogue.doc`
（MD5: `e7113c0e9aedca6edf47f2127cfad4f7`）に対応し、
マニュアルページ `rogue.6` は4.1BSD由来です
（MD5: `f84eef587bc38bc0823f2cd029df030b`）。

これらの歴史的資料は、参照用として**変更せずに同梱**しています。
特に `rogue.6` に記載されている古いセーブ・スコアファイル名は
当時の環境のものであり、本移植版とは異なります。
現在の保存先は後述の説明を参照してください。

## ビルド方法

既定ではシステム標準のCコンパイラ（`cc`）を使用します。
展開したプロジェクトの `src/` ディレクトリで `make` を実行すると、
実行ファイル `rogue` が生成されます。
`./configure` や `CC=clang` の明示的な指定は不要です。

### macOS

AppleのCommand Line Toolsが未導入の場合は、次のコマンドでインストールします。

```sh
xcode-select --install
```

続いて、ビルド・実行します。

```sh
cd src
make clean
make
./rogue
```

### Linux（Debian / Ubuntu）

コンパイラ、make、必要な開発用ライブラリをインストールします。

```sh
sudo apt update
sudo apt install build-essential libncurses-dev libcrypt-dev
```

展開したプロジェクトのディレクトリから、次のようにビルド・実行します。

```sh
cd src
make clean
make
./rogue
```

`libncurses-dev` はcursesのヘッダーとncursesのリンク用ライブラリを、
`libcrypt-dev` はゲーム内で使用する `crypt()` と `-lcrypt` に必要な
開発用ライブラリを提供します。

### Raspberry Pi OS

Raspberry Pi OSはDebian系のため、同じ開発用パッケージを利用します。

```sh
sudo apt update
sudo apt install build-essential libncurses-dev libcrypt-dev
```

ビルド・実行手順も同じです。

```sh
cd src
make clean
make
./rogue
```

### その他のLinuxディストリビューション

ディストリビューションのパッケージ管理ツールで、C11対応Cコンパイラ、
`make`、ncursesの開発用ヘッダー・ライブラリ、およびlibcryptの
開発用ライブラリを導入し、`src/` で `make` を実行してください。
MakefileはOSを判別し、macOSでは `-lcurses`、Linuxでは
`-lncursesw -lcrypt` を使用します。必要なら `make CC=clang` や
`make CC=gcc` でコンパイラを指定できます。

ゲームの実行には、**80列×24行以上**の端末が必要です。
端末ウィンドウを大きくしても、内部のダンジョン画面は80×24文字のままです。
プレイ中に端末をそれより小さくすると、必要なサイズに戻るまでゲームが一時停止します。

## 日本語・英語の切り替え

日本語表示・英語表示の切り替えに対応しています。初期言語はOSの設定から選ばれ、
`ROGUEOPTS` で明示的に指定できます。

```sh
ROGUEOPTS='language=ja' ./rogue
ROGUEOPTS='language=en' ./rogue
ROGUEOPTS='language=auto' ./rogue
```

ゲーム中は `o` のオプション画面から「表示する言語」を選択し、
`J`（日本語）か `E`（英語）を入力します。言語項目は「セーブファイル名」の
**ひとつ上**にあります。言語を切り替えるとオプション画面を再描画します。
`make original` は英語、`make japanese` は日本語での初期表示を指定して
クリーンビルドします。`ROGUE_LANG` は使用しません。

日本語化では、**3.6 JECと同じオプションの訳語・並び順**と
「階／金塊／体力／強さ／守備／経験」のステータス表示を採用しました。
さらに名前、果物、品物の呼び名のUTF-8入力、絵文字や日本語の文字幅による
折り返し対策、Backspace・左右矢印・Deleteによる文字編集、
UTF-8名のスコア保存を整備しています。

また、Rogue 3.6 JECの訳語を基準に、怪物名・品物名・戦闘・探索・
巻き物や水薬の説明・ヘルプ・スコア関連の日本語表示を追加しました。3.4で
異なる品物やゲーム規則は3.4の実装を維持します。言語の変更後は表示時に
訳語が切り替わり、内部データの英語名は保存されたままです。未識別の
巻き物の題名は、保存されている英語の音節列を3.6のカタカナ音節へ表示時に
置き換える近似方式を採用しています（セーブには音節IDを保存しません）。
ゲームルール、ターンや表示遅延は変更していません。

巻き物の題名は、Rogue 3.6/5.4と同様に日英双方のUTF-8表記が
40バイト以内になるよう生成時に制限しています。このため、初期化時に
消費する乱数の回数は制限導入前の3.4 JECと異なる場合があります。
セーブ形式は `R34SAV02` で、開発中の旧形式の読み込みには対応しません。
拾得メッセージの持ちもの記号は、
「あなたは短弓 (o)を手に入れた。」のように動詞の前に表示します。

移植の技術的な経緯と日本語訳の方針は [PORTING.md](PORTING.md) にまとめています。

**開発中の旧セーブデータ（`R34SAV01`）は読み込めません。**

## 操作方法・セーブ・スコア

詳しい遊び方については、原典に付属する英語の案内書
[A Guide to the Dungeons of Doom（rogue.doc）](src/rogue.doc)を参照してください。
ゲームの目的、画面の見方、操作コマンド、アイテムや戦闘などの基本が説明されています。

| 操作 | コマンド |
| --- | --- |
| 新規ゲームを開始 | `./rogue` |
| ゲーム内ヘルプを表示 | `?` |
| 移動 | `h`、`j`、`k`、`l`（斜め移動：`y`、`u`、`b`、`n`） |
| 持ち物を表示 | `i` |
| オプションを開く | `o` |
| セーブして終了 | `S` を押し、`y` で表示中のファイル名を使用、`n` で別名を入力（Escで中止） |
| 標準のセーブファイルから再開 | `./rogue -r` |
| セーブファイルを指定して再開 | `./rogue /path/to/savefile` |
| スコアを表示 | `./rogue -s` |
| 通常終了 | `Q` |
| 一時停止と再開 | `Ctrl-Z`、続いてシェルで `fg` |
| 対話シェルを開く | `!`、`exit` でゲームに戻る |

既定の保存先と形式は以下のとおりです。

- セーブファイル：`./rogue.save`（起動時のカレントディレクトリ）
- スコアファイル：`./rogue.scr`（起動時のカレントディレクトリ、隠しファイルではありません）
- セーブ形式：バージョン付きの `R34SAV02`（原典のプロセスメモリ保存形式とは互換性がありません）
- スコアは専用のロックファイルを作らず、スコアファイル自体を排他制御します。
- 正常に再開すると、セーブファイル名とファイルの更新日時を表示します。
- スコアファイルがまだない場合、`-s` では順位表の見出しを表示しません。
- オプション画面では、日英とも項目名に `("terse")` のような設定キーを併記しません。


### ビルド時のプログラム名・保存先の指定

3.6 JECと同様に、`src/` で実行する `make` に設定を渡せます。
`PROG` と `PROGRAM` はどちらも実行ファイル名を指定します。
両方を同時に指定した場合は `PROG` が優先されます。

```sh
make PROGRAM=rogue34
# rogue34 を生成。既定のセーブは ./rogue34.save、スコアは ./rogue34.scr

make PROG=rogue34 SAVEFILE=custom.save SCOREFILE=.custom.scr
# セーブは ./custom.save、スコアは ./.custom.scr（隠しファイル）

make PROGRAM=rogue34 FILEDIR='~/Documents' \
    SAVEFILE=rogue34.save SCOREFILE=rogue34.scr
# 保存先は実行時ユーザーの ~/Documents（ディレクトリは事前に作成）
```

| `make` の変数 | 意味 | 既定値 |
| --- | --- | --- |
| `PROG` / `PROGRAM` | 実行ファイル名（`PROG` 優先） | `rogue` |
| `SAVEFILE` | セーブファイル名 | `$(PROG).save` |
| `SCOREFILE` | スコアファイル名 | `$(PROG).scr` |
| `FILEDIR` | セーブ・スコア共通の保存先 | 未指定（カレントディレクトリ） |
| `SAVEDIR` | セーブ先（`FILEDIR` より優先） | `FILEDIR` または `.` |
| `SCOREDIR` | スコア先（`FILEDIR` より優先） | `FILEDIR` または `.` |

`SCOREFILE=.rogue34.scr` のように先頭を `.` にするとmacOS/Linuxでは
隠しファイルになり、`SCOREFILE=rogue34.scr` なら通常のファイルです。
既定の保存先は**ゲーム起動時のカレントディレクトリ**です。実行ファイルが
置かれたディレクトリとは限りません。ホームへ保存したいときは
`FILEDIR='~'` や `FILEDIR='~/Documents'` を明示します。
`FILEDIR` や各保存先の `~/` は**実行するユーザー**のホームを指し、
ビルドしたユーザーのホームパスは埋め込みません。
`/` で始まる絶対パスも指定できます。通常の相対ディレクトリは実行時の
カレントディレクトリを基準とします。保存先ディレクトリは自動作成されません。

ビルド設定を変更すると、Makefileが差異を検出して必要なオブジェクトを
再コンパイルします。`make PROGRAM=rogue34 original` などの
言語別ターゲットでも同じ名前・保存先指定を使えます。
`SAVEFILE` は起動時の既定値で、`ROGUEOPTS` や `o` コマンドで変更できます。
スコアファイル名はビルド時にのみ指定します。

セーブファイル名やHOMEディレクトリには、日本語などのUTF-8文字列や空白を使用でき、
原典の80バイトのバッファより長いパスにも対応しています。
ただし、本移植版の**セーブパス全体の上限は511バイト**（ディレクトリ名と
ファイル名を含む）です。

例えば、次のように `ROGUEOPTS` でHOME以下のセーブファイル名を指定できます。

```sh
ROGUEOPTS='file=~/保存.save' ./rogue
```

正常に再開すると、使用したセーブファイルは削除されます。

## 配布内容と関連文書

- `src/`：ゲームのソースコード、`Makefile`、原典由来の案内書・マニュアル
- `PORTING.md`：移植の技術的な経緯、原典とのゲームルールの相違、
  日本語化・Unicode・セーブ形式・保存先設定に関する保守資料
- `LICENSE`：再配布条件

`make clean` でオブジェクトファイルと、指定した `PROG` の実行ファイルを削除できます。
原典の配布アーカイブに含まれていた未使用の編集用スクリプト `:ctfix` は、
ビルドに不要なため、本移植版の配布から除外しています。


## Windows移植（第6-C段階・CI配布検証）

Windows版は **MSYS2 UCRT64 + PDCursesMod VT** でビルドします。`src/` で
次のコマンドを実行してください。

```sh
make PROGRAM=rogue34 clean
make PROGRAM=rogue34
```

生成される実行ファイルは `rogue34.exe` です。既定の保存先は**実行時の
カレントディレクトリ**で、セーブは `rogue34.save`、スコアは
`rogue34.scr` です。`PROG`／`PROGRAM`、`SAVEFILE`、`SCOREFILE`、
`FILEDIR`、`SAVEDIR`、`SCOREDIR` の指定方法はMac/Linuxと共通です。

第4段階でWindowsのセーブ・再開・スコア記録を実装し、Windows実機での
動作確認が済んでいます。セーブは `R34SAV02`、スコアは
`ROGUE34-SCORES-1` で、Mac/Linuxと同じ形式を使用します。
WindowsではUTF-8のパスをUTF-16に変換してファイル操作し、セーブと
スコアの排他制御は対象ファイル自体に行います。

**第5段階では次の機能を追加しました。**

- 日本語モードの再開通知は、保存ファイルの更新日時を
  `2026年11月23日（月）10:30:00` の形式で表示します。
  英語モードは原典の英語形式（`Mon Nov 23 10:30:00 2026`）を維持します。
  長いパスは表示幅に合わせて省略しますが、実際の保存先は変更しません。
  日時の日本語化はmacOS/Linuxにも共通で反映されます。
- Windowsの**ダブルクリック起動**で、既定のセーブファイルが存在する
  場合だけ起動メニューを出します。`R`／Enter＝標準セーブを再開、
  `N`＝新しい冒険、`F`＝ファイル選択ダイアログから再開、`Q`＝終了です。
  セーブがなければ新しい冒険を自動開始します。
- `-r` やセーブファイル名を指定したコマンドライン起動は、メニューを
  表示せず従来どおり再開します。既存のシェルから起動した場合も
  ダブルクリック用のメニューや強制的な端末サイズ変更は行いません。
- Windowsでも、ゲーム開始時とゲーム中（Ctrl-P）のウィザード認証に
  対応しました。原典のパスワード照合値を維持し、Windowsでは
  FreeSec版DES `crypt(3)` を使用します。新しいライセンス条件は
  `LICENSE` に記載しています。
- ダブルクリック起動時は、ゲーム終了後のスコア画面でReturnの入力を
  待ちます。既存のターミナルから起動した場合は、余分な終了待機を
  追加しません。

第2・3段階で導入した80×24自動調整、Unicode・絵文字の入出力、
3.6 JEC由来のカーソルちらつき対策も維持しています。

**第5段階までのWindows実機動作はメンテナーが確認済みです。**
第6-A段階では全機能を横断して最終検証を行いました。Linux上で
日英相互のセーブ・再開、チェックサム、破損セーブ・スコアの保持、
UTF-8名のスコア表示、スコアの同時参照を確認しました。Windows用の
全ソースは検証用Win32宣言で警告なしコンパイルを確認し、GitHub
ActionsにはWindowsネイティブでのビルド設定切り替えとスコア名の
検証を追加しています。

**未完了の実機検証**：Mac/LinuxとWindows間で実際に交換した
セーブファイルの相互再開、Windows上での同時スコア書き込み・
同時セーブ再開、Windows TerminalとConsole Hostでの長時間動作。
第6-AではWindowsのゲーム処理や保存形式に変更を加えていません。

**第6-B段階では**、MSYS2 UCRT64の `src/` で `make dist.win` を
実行するだけで通常版・英語初期設定版を含むZIPを作成できるようにしました。
必要なビルドツールに加え、MSYS2の `zip` と `objdump` が必要です。

```sh
pacman -S --needed zip mingw-w64-ucrt-x86_64-binutils
cd src
make dist.win
```

生成される `src/rogue34_jec_windows.zip` は、展開すると
`Rogue 3.4/` フォルダとなり、以下の5ファイルを含みます。

- `rogue34.exe`：通常版（OS設定に応じて初期言語を選択）
- `rogue34_original.exe`：英語初期設定版（ゲームルールは共通）
- `README_ja.txt`／`README_en.txt`：Windowsバイナリ用の説明書
- `LICENSE.txt`：ライセンス全文（FreeSecのライセンスを含む）

両EXEの既定のセーブ名は `rogue34.save`、スコア名は
`rogue34.scr` で共通です。`make dist.win` はその配布用設定で
再ビルドするため、通常の `PROGRAM=` などの個別ビルド設定は
配布用ZIPには引き継ぎません。終了時とシェル復帰時のWindows
カーソル形状復元処理も含みます。DLLのインポートを調べ、Windows
標準以外のDLLが見つかる場合はZIP作成を中止します。

**第6-C段階では**、GitHub ActionsのWindows UCRT64ジョブに
`make dist.win` とZIP内容の検査を追加しました。ZIP内のファイルが指定の
5種類だけであること、両方のEXEが64ビットWindows用であること、
外部のMSYS2/PDCurses DLLへの直接依存がないことを確認します。
さらにZIPを展開した両EXEの `-s` を実行し、初期言語設定・
`ROGUEOPTS` による上書き・共通スコアファイル名を検証します。

生成した `rogue34_jec_windows.zip` は、GitHubの
**Actions → Build Rogue 3.4 → 実行結果 → Artifacts** に
`rogue34-windows-ucrt64` という名前で保存する設定です。
GitHubからダウンロードしたアーティファクトのZIP内に、配布用の
`rogue34_jec_windows.zip` が入ります。Releaseの自動公開は行いません。

**GitHub Actions上での実際の成功は、変更を反映した後に確認が必要です。**
Windows Terminal／Console Hostでの対話操作はCIから検証できないため、
引き続き実機での確認が必要です。

詳細は [`PORTING.md`](PORTING.md) を参照してください。
