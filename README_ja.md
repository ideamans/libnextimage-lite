# libnextimage-lite

GoとTypeScript/Node.js向けの簡略化された画像変換ライブラリ。

4つの統合変換関数をフォーマット自動検出で提供します。libnextimage Cコア（libwebp + libavif）の上に構築されています。

## 変換関数

| 関数 | 入力 | 出力 | 動作 |
|------|------|------|------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless **[Experimental]** |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) **[Experimental]** |

> **注意:** AVIF関連機能（`legacyToAvif`、`avifToLegacy`）は現在 **Experimental（実験的）** です。APIが変更される可能性があり、一部のエッジケースが完全に対応されていない場合があります。本番環境ではWebP変換の使用を推奨します。

## クイックスタート

### Go

```bash
go get github.com/ideamans/libnextimage-lite/golang
```

```go
package main

import (
    "os"
    libnextimage "github.com/ideamans/libnextimage-lite/golang"
)

func main() {
    jpeg, _ := os.ReadFile("input.jpg")

    // JPEG → WebP (自動検出でlossy、品質75)
    out := libnextimage.LegacyToWebP(libnextimage.ConvertInput{
        Data:         jpeg,
        Quality:      -1, // デフォルト使用
        MinQuantizer: -1,
        MaxQuantizer: -1,
    })
    if out.Error != nil {
        panic(out.Error)
    }
    // out.MimeType == "image/webp"
    os.WriteFile("output.webp", out.Data, 0644)

    // JPEG → AVIF (lossy、品質80)
    out = libnextimage.LegacyToAvif(libnextimage.ConvertInput{
        Data:         jpeg,
        Quality:      80,
        MinQuantizer: -1,
        MaxQuantizer: -1,
    })
    if out.Error != nil {
        panic(out.Error)
    }
    // out.MimeType == "image/avif"
    os.WriteFile("output.avif", out.Data, 0644)
}
```

### TypeScript / Node.js

```bash
npm install libnextimage
```

```typescript
import { legacyToWebp, legacyToAvif } from 'libnextimage'
import { readFileSync, writeFileSync } from 'fs'

const jpeg = readFileSync('input.jpg')

// JPEG → WebP (自動検出でlossy、品質75)
const webp = legacyToWebp({ data: jpeg })
// webp.mimeType === 'image/webp'
writeFileSync('output.webp', webp.data)

// JPEG → AVIF (lossy、品質80)
const avif = legacyToAvif({ data: jpeg, quality: 80 })
// avif.mimeType === 'image/avif'
writeFileSync('output.avif', avif.data)
```

## API設計

すべての変換関数は統一された構造体ベースのパターンに従います：

**入力**: `{ data, quality?, minQuantizer?, maxQuantizer? }` — `quality`は-1/省略でデフォルト、0-100で明示指定。`minQuantizer`/`maxQuantizer`はAVIF微調整用(0-63)。

**出力**: `{ data, mimeType }` (Goには`error`フィールドも) — 出力画像データと検出されたMIMEタイプ。

**メモリ**: 出力バッファは自動管理されます。明示的な`close()`や`free()`は不要です。

## プラットフォーム

- macOS: Intel (x64)、Apple Silicon (ARM64)
- Linux: x64、ARM64
- Windows: x64

ビルド済みバイナリは[GitHub Releases](https://github.com/ideamans/libnextimage-lite/releases)で提供されています。

## ソースからのビルド

```bash
# 前提条件 (macOS)
brew install cmake jpeg libpng giflib

# 前提条件 (Linux)
sudo apt-get install cmake build-essential libjpeg-dev libpng-dev libgif-dev

# ビルドとインストール
make install-c
```

## テスト

```bash
# Go
cd golang && go test -v -timeout 120s

# TypeScript
cd typescript && npm install && npm test
```

## Showcase

変換結果を目視確認するためのWebビューアが [`showcase/`](showcase/) に含まれています。テスト画像をライブラリで変換し、元画像と変換後画像をサイドバイサイドで比較できます。

```bash
cd showcase
npm install
npm run generate   # テスト画像を変換 → public/
npm run dev        # ブラウザでビューアを開く
```

詳細は [showcase/README.md](showcase/README.md) を参照してください。

## ドキュメント

- [C ライブラリ README](c/README.md)
- [Go README](golang/README.md)
- [TypeScript README](typescript/README.md)
- [リリースプロセス](RELEASE.md)
- [英語版README](README.md)

## ライセンス

MIT License (c) 2025 Ideamans Inc.

### 依存ライブラリ

| ライブラリ | ライセンス | 用途 |
|-----------|-----------|------|
| [libwebp](https://github.com/webmproject/libwebp) | BSD-3-Clause | WebPエンコード/デコード |
| [libavif](https://github.com/AOMediaCodec/libavif) | BSD-2-Clause | AVIFエンコード/デコード |
| [libaom](https://aomedia.googlesource.com/aom/) | BSD-2-Clause + Patent | AV1コーデック（AVIFバックエンド） |
| [stb](https://github.com/nothings/stb) | MIT / Public Domain | C層のJPEG/PNG入出力 |

すべての依存ライブラリはスタティックリンクされ、ビルド済みバイナリに同梱されています。

## クレジット

開発: [株式会社アイデアマンズ](https://www.ideamans.com/)
