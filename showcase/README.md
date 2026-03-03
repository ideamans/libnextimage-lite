# libnextimage-lite Showcase

libnextimage-lite の変換結果を目視で確認するためのWebビューアです。テスト画像をライブラリで変換し、元画像と変換後画像をサイドバイサイドで比較できます。

## 前提条件

- Node.js 20+
- libnextimage の C ライブラリがビルド・インストール済みであること
- `testdata/` にテスト画像が存在すること

## セットアップ

### 1. C ライブラリのビルド（未実施の場合）

```bash
# プロジェクトルートで実行
make install-c
```

### 2. TypeScript バインディングのビルド

```bash
cd ../typescript
npm install
npm run build
cd ../showcase
```

### 3. showcase の依存関係インストール

```bash
npm install
```

## 使い方

### データ生成

`testdata/` のテスト画像を変換し、`public/` にメタデータと画像ファイルを出力します。

```bash
npm run generate
```

出力先:
- `public/manifest.json` — 変換結果のメタデータ（ファイルサイズ、圧縮率、処理時間など）
- `public/images/` — 元画像のコピーと変換後の画像

### ビューアの起動

```bash
npm run dev
```

ブラウザが自動で開き、変換結果の一覧が表示されます。

## 対象テスト画像

| カテゴリ | ファイル | 確認ポイント |
|---------|---------|-------------|
| JPEG | `jpeg-source/photo-like.jpg` | 写真的コンテンツの圧縮効率 |
| JPEG | `jpeg-source/text.jpg` | テキスト含む画像の品質 |
| JPEG | `jpeg-source/gradient-radial.jpg` | グラデーションの再現性 |
| PNG | `png-source/photo-like.png` | PNG→WebP/AVIF の圧縮効率 |
| PNG | `png-source/checkerboard.png` | パターン画像のロスレス変換 |
| GIF | `gif-source/static-512x512.gif` | 静的GIF→WebP |
| GIF | `gif-source/animated-3frames.gif` | アニメーションGIF→WebP |
| WebP | `webp-samples/lossy-q90.webp` | 非可逆WebP→JPEG |
| WebP | `webp-samples/lossless.webp` | 可逆WebP→PNG |
| AVIF | `avif/red.avif` | AVIF→JPEG |
| ICC | `icc/srgb.jpg`, `icc/srgb.png` | sRGBプロファイル付き画像の色再現 |
| ICC | `icc/display-p3.jpg`, `icc/display-p3.png` | Display P3プロファイルの保持・変換 |
| ICC | `icc/no-icc.jpg`, `icc/no-icc.png` | ICCなし画像との比較 |
| Orientation | `orientation/orientation-{1,3,6,8}.jpg` | EXIF回転情報の正しい反映 |

## 変換マトリックス

| 入力形式 | 変換先 | 使用関数 |
|---------|--------|---------|
| JPEG | WebP, AVIF | `legacyToWebp`, `legacyToAvif` [Experimental] |
| PNG | WebP, AVIF | `legacyToWebp`, `legacyToAvif` [Experimental] |
| GIF | WebP | `legacyToWebp` |
| WebP | JPEG or PNG | `webpToLegacy` |
| AVIF | JPEG or PNG | `avifToLegacy` [Experimental] |

## フィルタ機能

ビューア上部のボタンでカテゴリ別に絞り込めます:

- **All** — 全画像を表示
- **JPEG / PNG / GIF / WebP / AVIF** — 入力形式別
- **ICC** — ICCカラープロファイル付き画像のみ
- **EXIF Orientation** — EXIF回転テスト画像のみ

## テスト画像の追加

`src/generate.ts` の `SOURCES` 配列にエントリを追加してください:

```typescript
{ file: 'testdata内の相対パス', mime: 'image/jpeg' },
```

追加後、`npm run generate` を再実行するとビューアに反映されます。

## ディレクトリ構成

```
showcase/
├── package.json
├── tsconfig.json
├── vite.config.ts
├── .gitignore
├── index.html            # ビューアのエントリポイント
├── style.css             # スタイリング
├── viewer.ts             # ブラウザ側ロジック
├── src/
│   ├── generate.ts       # データ生成スクリプト
│   └── types.ts          # 共通型定義
└── public/               # (gitignored) 生成データ
    ├── manifest.json
    └── images/
```
