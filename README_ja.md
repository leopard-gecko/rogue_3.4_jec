# Rogue 3.4 JEC — macOS / Linux / Windows 移植版

**Rogue 3.4 JEC 003**

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

同梱の `rogue.6` と `rogue.cat` は4.1BSD版をもとに、JEC版の起動方法と
セーブファイルの扱いに合わせて最小限の修正を加えたものです。

## ビルド方法

macOS/Linuxではシステム標準のCコンパイラ（`cc`）を使用します。
展開したプロジェクトの `src/` で `make` を実行すると、`rogue` が生成されます。
`./configure` は不要です。Windowsは後述のMSYS2 UCRT64を使用します。

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

### Windows（MSYS2 UCRT64）

MSYS2 UCRT64とPDCursesMod VTを使用します。必要な開発環境を導入した後、
UCRT64シェルで次のようにビルドします。

```sh
cd src
make PROGRAM=rogue34
./rogue34.exe
```

Windows用の配布ZIPは、`zip` とbinutilsを導入して作成します。

```sh
pacman -S --needed zip mingw-w64-ucrt-x86_64-binutils
make dist.win
```

生成される `rogue34_jec_windows.zip` には、`Rogue 3.4/` 以下に
`rogue34.exe`、日英README、日英案内書（各TXT・HTML）、
`LICENSE.txt` が収録されます。配布時には実行ファイルのDLL依存を検査します。
Windows版はUTF-8のファイル名と80×24の表示に対応し、
ダブルクリック起動時は標準セーブが存在する場合に再開メニューを表示します。

### ビルド時のプログラム名・保存先の指定

`PROG` または `PROGRAM` で実行ファイル名を指定できます（同時指定時は `PROG` を優先）。
セーブ・スコアの既定のファイル名はそれぞれ `<プログラム名>.save`、`<プログラム名>.scr` です。

```sh
make PROGRAM=rogue34
make PROG=rogue34 SAVEFILE=custom.save SCOREFILE=custom.scr
make PROGRAM=rogue34 FILEDIR='~/Documents'
```

保存先の既定値はゲーム起動時のカレントディレクトリです。`FILEDIR` はセーブ・スコア共通の
保存先を指定し、`SAVEDIR` と `SCOREDIR` で個別に上書きできます。`~/` は実行ユーザーの
ホームを指します。指定するディレクトリは事前に作成してください。
セーブファイル名は実行時にも変更でき、セーブパス全体の上限は511バイトです。

### インストール（macOS / Linux / Raspberry Pi OS）

`src/` で以下を実行すると、本体・マニュアル・日英案内書をインストールできます。

```sh
sudo make install
man rogue34
```

通常の `make` は従来どおり `rogue` を作成しますが、`make install` の
既定のインストール名は `rogue34` です。`make install PROG=名前` または
`make install PROGRAM=名前` で変更できます。インストール先は `PREFIX`
（既定 `/usr/local`）、`BINDIR`、`MANDIR`、`DOCDIR`、`DESTDIR` で指定できます。
`rogue.6` はman6、整形済みの `rogue.cat` はcat6へ収録します。

アンインストールは `sudo make uninstall` です（インストール時に指定した変数は
同じ値を指定してください）。セーブ・スコアファイルは削除しません。
`FILEDIR`、`SAVEDIR`、`SCOREDIR` で絶対パスを指定した場合、そのディレクトリは
`make install` 時に作成します。相対パスと `~/` で始まるパスは作成しません。
Windows版は従来どおり `make dist.win` で配布ZIPを作成します。

ゲームの実行には、**80列×24行以上**の端末が必要です。
端末ウィンドウを大きくしても、内部のダンジョン画面は80×24文字のままです。
プレイ中に端末をそれより小さくすると、必要なサイズに戻るまでゲームが一時停止します。

## 操作方法・セーブ・スコア

詳しい遊び方については、日本語版の
[【運命の洞窟】への案内書（HTML）](src/rogue_ja.html)を参照してください。
[TXT](src/rogue_ja.txt)・[Markdown](src/rogue_ja.md)版も収録しています。
英語版は[HTML](src/rogue_en.html)・[TXT](src/rogue_en.txt)・
[Markdown](src/rogue_en.md)で収録しています。

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
- セーブ形式：`R34SAV02`
- スコアは専用のロックファイルを作らず、スコアファイル自体を排他制御します。
- 正常に再開すると、セーブファイル名と更新日時を表示し、使用したセーブファイルを削除します。
- スコアファイルがまだない場合、`-s` では順位表の見出しを表示しません。
- オプション画面では、日英とも項目名に `("terse")` のような設定キーを併記しません。

## 配布内容と関連文書

- `src/`：ゲームのソースコード、`Makefile`、JEC版に合わせたマニュアルページ、日英案内書（HTML・TXT・MD）
- `PORTING.md`：移植の技術的な経緯、原典とのゲームルールの相違、
  日本語化・Unicode・セーブ形式・保存先設定に関する保守資料
- `LICENSE`：再配布条件

`make clean` でオブジェクトファイルと、指定した `PROG` の実行ファイルを削除できます。
原典の配布アーカイブに含まれていた未使用の編集用スクリプト `:ctfix` は、
ビルドに不要なため、本移植版の配布から除外しています。

GitHub ActionsではmacOS/Linux/WindowsのビルドとWindows配布ZIPを検証し、
`rogue34-windows-ucrt64` のアーティファクトとして提供します（Releaseへの自動公開は行いません）。
