# libnextimage

Simplified image conversion library for Node.js with TypeScript support. Provides 4 unified conversion functions with automatic format detection.

## Installation

```bash
npm install libnextimage
```

Pre-built native libraries are downloaded automatically during installation. No compilation required.

## Conversion Functions

| Function | Input | Output | Behavior |
|----------|-------|--------|----------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless **[Experimental]** |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) **[Experimental]** |

> **Note:** AVIF support (`legacyToAvif`, `avifToLegacy`) is currently **experimental**. The API may change. WebP conversion is stable and recommended for production use.

## Quick Start

```typescript
import { legacyToWebp, legacyToAvif } from 'libnextimage'
import { readFileSync, writeFileSync } from 'fs'

const jpeg = readFileSync('input.jpg')

// JPEG → WebP (auto-detected as lossy, quality 75)
const webp = legacyToWebp({ data: jpeg })
// webp.mimeType === 'image/webp'
writeFileSync('output.webp', webp.data)

// JPEG → AVIF (lossy, quality 80)
const avif = legacyToAvif({ data: jpeg, quality: 80 })
// avif.mimeType === 'image/avif'
writeFileSync('output.avif', avif.data)
```

## API Reference

### Types

```typescript
/** Input for all conversion functions */
interface ConvertInput {
  data: Buffer          // Input image data
  quality?: number      // -1 or omit for default, 0-100 for explicit
  minQuantizer?: number // AVIF lossy min quantizer (0-63, 0=best quality)
  maxQuantizer?: number // AVIF lossy max quantizer (0-63, 63=worst quality)
}

/** Output from all conversion functions */
interface ConvertOutput {
  data: Buffer    // Output image data
  mimeType: string // "image/webp", "image/jpeg", "image/png", "image/gif"
}
```

### Functions

```typescript
function legacyToWebp(input: ConvertInput): ConvertOutput  // JPEG/PNG/GIF → WebP
function webpToLegacy(input: ConvertInput): ConvertOutput   // WebP → JPEG/PNG/GIF (auto)
function legacyToAvif(input: ConvertInput): ConvertOutput   // JPEG/PNG → AVIF [Experimental]
function avifToLegacy(input: ConvertInput): ConvertOutput   // AVIF → JPEG/PNG (auto) [Experimental]
```

All functions throw `NextImageError` on failure.

### Error Handling

```typescript
import { legacyToWebp, NextImageError } from 'libnextimage'

try {
  const result = legacyToWebp({ data: inputData })
  // result.data, result.mimeType
} catch (e) {
  if (e instanceof NextImageError) {
    console.error(`Conversion failed: ${e.message} (status: ${e.status})`)
  }
}
```

### Utility Functions

```typescript
import { getLibraryVersion } from 'libnextimage'

console.log(getLibraryVersion()) // e.g., "0.5.0"
```

### Memory Management

Output buffers are automatically managed. No explicit `close()` or `free()` is needed.

## Supported Platforms

- macOS: Apple Silicon (ARM64), Intel (x64)
- Linux: x64, ARM64
- Windows: x64

## Testing

```bash
cd typescript && npm install && npm test
```

## License

MIT License (c) 2025 Ideamans Inc.
