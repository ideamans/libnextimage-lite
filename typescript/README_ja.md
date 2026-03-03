# libnextimage

Node.js向けの簡略化された画像変換ライブラリ、TypeScriptサポート付き。4つの統合変換関数をフォーマット自動検出で提供します。

## インストール

```bash
npm install libnextimage
```

インストール時にプリビルドネイティブライブラリが自動的にダウンロードされます。コンパイル不要です。

## 変換関数

| 関数 | 入力 | 出力 | 動作 |
|------|------|------|------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless **[Experimental]** |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) **[Experimental]** |

> **注意:** AVIF関連機能（`legacyToAvif`、`avifToLegacy`）は現在 **Experimental（実験的）** です。APIが変更される可能性があります。本番環境ではWebP変換の使用を推奨します。

## クイックスタート

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

## APIリファレンス

### 型

```typescript
/** すべての変換関数の入力 */
interface ConvertInput {
  data: Buffer          // 入力画像データ
  quality?: number      // -1または省略でデフォルト、0-100で明示指定
  minQuantizer?: number // AVIF lossy最小量子化値 (0-63, 0=最高品質)
  maxQuantizer?: number // AVIF lossy最大量子化値 (0-63, 63=最低品質)
}

/** すべての変換関数の出力 */
interface ConvertOutput {
  data: Buffer    // 出力画像データ
  mimeType: string // "image/webp", "image/jpeg", "image/png", "image/gif"
}
```

### 関数

```typescript
function legacyToWebp(input: ConvertInput): ConvertOutput  // JPEG/PNG/GIF → WebP
function webpToLegacy(input: ConvertInput): ConvertOutput   // WebP → JPEG/PNG/GIF (自動)
function legacyToAvif(input: ConvertInput): ConvertOutput   // JPEG/PNG → AVIF [Experimental]
function avifToLegacy(input: ConvertInput): ConvertOutput   // AVIF → JPEG/PNG (自動) [Experimental]
```

すべての関数は失敗時に `NextImageError` をスローします。

### エラーハンドリング

```typescript
import { legacyToWebp, NextImageError } from 'libnextimage'

try {
  const result = legacyToWebp({ data: inputData })
  // result.data, result.mimeType を使用
} catch (e) {
  if (e instanceof NextImageError) {
    console.error(`変換失敗: ${e.message} (status: ${e.status})`)
  }
}
```

### ユーティリティ関数

```typescript
import { getLibraryVersion } from 'libnextimage'

console.log(getLibraryVersion()) // 例: "0.5.0"
```

### メモリ管理

出力バッファは自動管理されます。明示的な `close()` や `free()` は不要です。

## サポートプラットフォーム

- macOS: Apple Silicon (ARM64)、Intel (x64)
- Linux: x64、ARM64
- Windows: x64

## テスト

```bash
cd typescript && npm install && npm test
```

## ライセンス

BSD-3-Clause
