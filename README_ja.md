# libnextimage-light

GoとTypeScript/Node.js向けの簡略化された画像変換ライブラリ。

4つの統合変換関数をフォーマット自動検出で提供します。libnextimage Cコア（libwebp + libavif）の上に構築されています。

## 変換関数

| 関数 | 入力 | 出力 | 動作 |
|------|------|------|------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) |

## クイックスタート

### Go

```bash
go get github.com/ideamans/libnextimage/golang
```

```go
package main

import (
    "os"
    libnextimage "github.com/ideamans/libnextimage/golang"
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

ビルド済みバイナリは[GitHub Releases](https://github.com/ideamans/libnextimage/releases)で提供されています。

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

## ドキュメント

- [Go README](golang/README.md)
- [TypeScript README](typescript/README.md)
- [英語版README](README.md)

## ライセンス

BSD 3-Clause License

- libwebp: BSD License
- libavif: BSD License
- libaom: BSD License

## クレジット

開発: [株式会社アイデアマンズ](https://www.ideamans.com/)
