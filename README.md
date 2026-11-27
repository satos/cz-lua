# CZ Lua

**CZ Lua is a self-contained Lua game environment for CardputerZero —
write, run and draw, all on the device.**

CardputerZero 上で「書く→実行→描く」が完結する、LÖVE（love2d）風の小型ゲーム環境です。
TIC-80 のように端末内で開発が完結するツールを、ネイティブ実装で目指しています。
日本語入力は対象外（コードは英語のみ）。公開（CardputerZero ストア審査制）も視野に入れています。

- Device: CardputerZero (320×170, 46-key keyboard, Cortex-A53 / 512 MB RAM)
- Language: Lua 5.4.8 (bundled from source, `third_party/lua/`)
- UI: LVGL v9.5.0, C++ (official Template based)
- Repo: https://github.com/satos/cz-lua

## About this project

**さとー（@satos73）と、その Muse 上のエージェント「クマ」による個人プロジェクトです。**

- さとー: 企画・方針決定・実機テスト担当。人間。
- クマ: 設計・実装・ビルド担当。Meta のパーソナル AI「Muse」上で動くエージェントで、
  クラウド VM 上でコードを書き、クロスコンパイルして .deb を作ります。

このため本文には「さとー報告」「さとー決定」のような表記が頻出しますが、
上記の2名（1名と1体）のことです。開発のやりとりは主にスマホのチャットと
CardputerZero 実機だけで行われています。

## Status: running on real hardware

2026-11-26 の昼休み、実機テストで確認済み（さとー報告）:

- `sudo dpkg -i CzLua_0.1.0_rev1_arm64.deb` でインストール成功、ランチャーに「CzLua」表示
- サンプルのライフゲームが実機で動作、さとー評「かなり高速」
- 色も正常（濃紺の背景に緑のセル）
- キーボードで終了できた ＝ キー入力が画面に届くことを実証（未検証だった最大の関門を通過）

Fixed in rev2 (2026-11-27, さとーとクマで同日中に原因特定→修正→再ビルド):

- タイトルバーとナビバーのアイコンが「豆腐」（□）表示だった件。実機ログに
  `font asset not found: Phosphor-Fill.ttf` が2回出ていたのが決め手で、原因は
  `/usr/share/cz_lua/` のディレクトリ権限が `770` だったこと（VM の umask 0007 が
  パッケージに混入）。`sudo chmod 755 /usr/share/cz_lua` でその場は直り、
  さとーの写真でタイトルバーの電池アイコン＋下部ナビ5アイコン全描画・豆腐ゼロを確認。
  rev2 では .deb 内の全ディレクトリ権限を 755 に修正済み（パッケージ内で検証済み）で、
  上書きインストールだけで chmod 不要。なお rev1 で動いていたライフゲームは
  プロジェクトファイルが読めていなかったための内蔵フォールバック版だった（中身は同じ物）。
- タイトルバーが Template の残り物「Template App」表示だった件。rev2 で「CZ Lua」に修正済み
  （`src/model/base_model.cpp` の `app_title()` が `"CZ Lua"` を返す。GitHub にも push 済み）。

rev2 の実機への上書きインストールは、さとーが手が空いたときで OK（`sudo dpkg -i CzLua_0.1.0_rev2_arm64.deb`）。

## Quick start (on the device)

```sh
sudo dpkg -i CzLua_0.1.0_rev2_arm64.deb
# ランチャーから「CzLua」を開く。ライフゲームが動き出します。
# スペースキー: 模様をリシード（最初からやり直し）
# 終了: キーボード操作で抜けられます（ナビバーの戻る操作）
sudo apt remove czlua   # 削除するとき
```

## The `czlua` API (v1 scope, decided 2026-11-25)

LÖVE のコールバックモデル（`load` / `update(dt)` / `draw()`）を踏襲し、
名前は本家との混同を避けて `czlua.*` に置いています。

| API | LÖVE equivalent | Notes |
|---|---|---|
| `function load()` | love.load | 起動時に1回 |
| `function update(dt)` | love.update | dt は秒 |
| `function draw()` | love.draw | 毎フレーム |
| `czlua.graphics.setColor(r, g, b)` | love.graphics.setColor | 0–1 の float |
| `czlua.graphics.rectangle(mode, x, y, w, h)` | 同名 | mode は "fill"/"line" |
| `czlua.graphics.circle(mode, x, y, r)` | 同名 | |
| `czlua.graphics.line(x1, y1, x2, y2)` | 同名 | |
| `czlua.graphics.points(x, y, ...)` | 同名 | |
| `czlua.graphics.print(text, x, y)` | 同名 | ASCII のみ |
| `czlua.graphics.clear(r, g, b)` | 同名 | |
| `czlua.keyboard.isDown(key)` | love.keyboard.isDown | |
| `function keypressed(key)` | love.keypressed | コールバック |
| `czlua.system.getSize()` → 320, 170 | love.graphics.getDimensions | 固定値 |
| `czlua.system.quit()` | love.event.quit | エディタへ戻る |
| `czlua.files.readText / writeText` | — | プロジェクトフォルダ内のみ（v1 に入れる方向） |

Decided for v1 (2026-11-25, さとー決定):

- **名前: CZ Lua**（LÖVE のコードを直接派生させたものではないため LÖVE を含む名前は避ける。被りなし確認済み）
- **名前空間: `czlua`**
- スコア保存: v1 に入れる
- ファイルの読み書き: want（プロジェクトフォルダ内限定で v1 方向）
- スプライト・画像・サウンド: 後回し（v1.1/v2 候補）
- サンプルゲーム: ライフゲーム（さとーの個人的趣味で選定）

Editor (v1): vi 風モーダル編集（`hjkl` 移動、`i` で入力、`:w` 保存・`:q` 終了）。
46 キーではカーソルキーが Fn 層の奥にあるため、修飾キー和音は必須にしません。

## Sandbox (safety guards)

コードを事前に読んで弾くのではなく、**危険な能力を最初から渡さない**設計です。

- 読み込むライブラリは base / string / math / table / coroutine のみ。
  `os` / `io` / `package` / `debug` は存在しないため `os.execute` は `os` 自体が nil で失敗します。
  `load` / `require` / `dofile` も nil にしてあり、動的なコード生成によるすり抜けも塞いでいます。
- 命令数フック: 1 万命令ごとに数え、1 回の呼び出しで 1 億命令を超えたらエラーで強制終了
  （`while true do end` でも固まらないことを VM で検証済み）。
- メモリ上限: 専用アロケータで 16 MB まで。超過はエラーで停止します。
- ファイル confinement: `czlua.files` は相対パスのみ、`..` と絶対パスは拒否、プロジェクトフォルダの外には出られません。
- すべて `pcall`（保護呼び出し）の中で回すため、Lua 側のエラーはアプリを落とさず画面表示で終わります。

Future candidate (2026-11-26, さとー発案): LÖVE の GUI 部品ライブラリのような純 Lua ライブラリを、
使ってみて必要になったら「許可制」で追加できるようにする。審査済みライブラリをアプリ本体に同梱し、
プロジェクト側は許可リストにある物だけ `require` できる（任意ダウンロードやネイティブモジュールは不可）。
純 Lua なら命令数・メモリの上限がそのまま効くので、サンドボックスの前提は崩れません。

## Build (cross-compile from a Linux host)

WSL2 + Ubuntu 24.04 が素直です（Windows ネイティブのクロス環境は非推奨）。
詳しい手順メモ: `wsl_build_setup_2026-11-29.md`（週末用に別途用意済み）。

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu ninja-build git pkg-config
# 落とし穴: CMake は 3.31 以上が必須。Ubuntu 24.04 の apt 版は 3.28 で足りない。
# Kitware のバイナリ tarball か `pip install cmake` で新しいものを。
git clone https://github.com/satos/cz-lua   # WSL 内の ~/ に clone（/mnt/c 配下は遅い）
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel
cpack --preset cp0-cross-deb
# または cmake --workflow --preset cp0-cross-package
```

Measured costs (VM 実測): 初回 configure 約 10 分、キャッシュ込み約 2.3 GB、2 回目以降のビルドは 2 分弱。
対象アーキテクチャは **aarch64**（armhf ではない）。

On-device build (romance option): Template には `cp0-native` プリセットがあり、実機の Debian 13 の
cmake は 3.31 系で要件を満たします。ただし RAM 512 MB のため `-j1` + zram、フルビルドは
30 分〜1 時間コースで USB 給電が前提。フルビルドは初回だけの儀式、あとは増分、という落としどころです。

## Roadmap

- **v0 (done)**: Lua 実行＋描画のみ。VM 上でライフゲーム 90 フレーム・ボール 60 フレームを検証済み。
- **v0.1 (done, 2026-11-25)**: 公式 Template 組み込み＋ aarch64 クロスコンパイル＋ .deb 化（`CzLua_0.1.0_rev1_arm64.deb`）、
  GitHub 初回 push（テキスト 148 ファイル、commit `ab175e12`）。
- **rev2 (done, 2026-11-27)**: 豆腐アイコン修正（原因は `/usr/share/cz_lua/` の権限 770、VM の umask 0007 の混入。
  .deb 内の全ディレクトリを 755 に修正、パッケージ内で検証済み）＋タイトル「CZ Lua」化。GitHub に push 済み。
  実機への上書きインストールは `sudo dpkg -i CzLua_0.1.0_rev2_arm64.deb`（chmod 不要）。
- **v1**: エディタ＋ファイル管理＋保存＋エラー表示＋サンプル同梱。ここで「端末内で完結」を名乗れる。
- **v1.1 候補**: 簡易サウンド（ES8389 で beep / 矩形波）。
- **v2 候補**: ドット絵エディタ＋スプライト API、複数ファイル対応、シンタックスハイライト、許可制パッケージ。

## License

- CZ Lua / Template base: MIT (see `LICENSE` / assets folder for third-party asset notes)
- Lua 5.4.8: MIT (bundled from source in `third_party/lua/`)
