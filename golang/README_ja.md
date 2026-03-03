# libnextimage - Go バインディング

Go向けの簡略化された画像変換ライブラリ。4つの統合変換関数をフォーマット自動検出で提供します。

## インストール

```bash
go get github.com/ideamans/libnextimage-lite/golang
```

## 変換関数

| 関数 | 入力 | 出力 | 動作 |
|------|------|------|------|
| `LegacyToWebP` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `WebPToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `LegacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless **[Experimental]** |
| `AvifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) **[Experimental]** |

> **注意:** AVIF関連機能（`LegacyToAvif`、`AvifToLegacy`）は現在 **Experimental（実験的）** です。APIが変更される可能性があります。本番環境ではWebP変換の使用を推奨します。

## クイックスタート

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

## APIリファレンス

### 型

```go
// ConvertInput はすべての変換関数の入力を表します。
type ConvertInput struct {
    Data         []byte // 入力画像データ
    Quality      int    // -1=デフォルト使用、0-100=明示指定
    MinQuantizer int    // -1=デフォルト使用、0-63 (AVIF lossy最小量子化値)
    MaxQuantizer int    // -1=デフォルト使用、0-63 (AVIF lossy最大量子化値)
}

// ConvertOutput はすべての変換関数の出力を表します。
type ConvertOutput struct {
    Data     []byte // 出力画像データ
    MimeType string // "image/webp", "image/jpeg", "image/png", "image/gif"
    Error    error  // 成功時はnil
}
```

### 関数

```go
func LegacyToWebP(input ConvertInput) ConvertOutput  // JPEG/PNG/GIF → WebP
func WebPToLegacy(input ConvertInput) ConvertOutput   // WebP → JPEG/PNG/GIF (自動)
func LegacyToAvif(input ConvertInput) ConvertOutput   // JPEG/PNG → AVIF [Experimental]
func AvifToLegacy(input ConvertInput) ConvertOutput   // AVIF → JPEG/PNG (自動) [Experimental]

func Version() string  // ライブラリバージョンを返す
```

### メモリ管理

出力バッファは自動管理されます。明示的な `Close()` や `Free()` は不要です。

## プラットフォームサポート

| プラットフォーム | アーキテクチャ | 状態 |
|-----------------|---------------|------|
| macOS | ARM64 (M1/M2/M3) | 対応 |
| macOS | x64 | 対応 |
| Linux | x64 | 対応 |
| Linux | ARM64 | 対応 |
| Windows | x64 | 対応 |

## CGO要件

このパッケージはCGOを有効にする必要があります：

```bash
CGO_ENABLED=1 go build
```

ビルド済みの静的ライブラリが `shared/lib/{platform}/` に組み込まれています。外部ライブラリのインストールは不要です。

## テスト

```bash
cd golang && go test -v -timeout 120s
```

## ライセンス

BSD-3-Clause
