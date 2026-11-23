# Rogue 3.4 — macOS / Linux 移植版

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
`-lncurses -lcrypt` を使用します。必要なら `make CC=clang` や
`make CC=gcc` でコンパイラを指定できます。

ゲームの実行には、**80列×24行以上**の端末が必要です。
端末ウィンドウを大きくしても、内部のダンジョン画面は80×24文字のままです。
プレイ中に端末をそれより小さくすると、必要なサイズに戻るまでゲームが一時停止します。

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
| セーブして終了 | `S`：`y` は表示された名前で保存、`n` は別名を入力 |
| 標準のセーブファイルから再開 | `./rogue -r` |
| セーブファイルを指定して再開 | `./rogue /path/to/savefile` |
| スコアを表示 | `./rogue -s` |
| 通常終了 | `Q` |
| 一時停止と再開 | `Ctrl-Z`、続いてシェルで `fg` |
| 対話シェルを開く | `!`、`exit` でゲームに戻る |

保存時には、書き込み開始前に `File name: ...` が表示され、保存処理にかかった時間を含めて最低200ミリ秒間表示されます。保存に失敗した場合は別のファイル名を入力して再試行できます。ファイル名の入力中にEscapeキーを押すと、保存せずゲームに戻ります。既定のファイル名がない場合も、名前の入力を求めます。

既定の保存先と形式は以下のとおりです。

- セーブファイル：`./rogue.save`（`rogue` を起動したカレントディレクトリ）
- スコアファイル：`~/.rogue_roll`（ユーザーごと。共有ランキングは未実装）
- セーブ形式：バージョン付きの `R34SAV01`（原典のプロセスメモリ保存形式とは互換性がありません）

セーブファイル名やHOMEディレクトリには、日本語などのUTF-8文字列や空白を使用でき、
原典の80バイトのバッファより長いパスにも対応しています。
ただし、本移植版の**セーブパス全体の上限は511バイト**（ディレクトリ名と
ファイル名を含む）です。

例えば、次のように `ROGUEOPTS` でHOME以下のセーブファイル名を指定できます。

```sh
ROGUEOPTS='file=~/保存.save' ./rogue
```

原典と同様、オプション名は先頭の数文字に省略できます。例えば
`ROGUEOPTS='ter,noflu'` は `terse,noflush` と同じ指定です。

正常に再開すると、使用したセーブファイルは削除されます。

## 配布内容と関連文書

- `src/`：ゲームのソースコード、`Makefile`、原典由来の案内書・マニュアル
- `PORTING.md`：移植の技術的な履歴、[原典とのゲームルール比較](PORTING.md#original-rules-audit)、移植性に関する注記
- `LICENSE`：再配布条件

`make clean` で生成されたオブジェクトファイルと `src/rogue` を削除できます。
原典の配布アーカイブに含まれていた未使用の編集用スクリプト `:ctfix` は、
ビルドに不要なため、本移植版の配布から除外しています。
