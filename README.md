# libnextimage-light

Simplified image conversion library with bindings for Go and TypeScript/Node.js.

Provides 4 unified conversion functions with automatic format detection. Built on top of the libnextimage C core (libwebp + libavif).

## Conversion Functions

| Function | Input | Output | Behavior |
|----------|-------|--------|----------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) |

## Quick Start

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

    // JPEG → WebP (auto-detected as lossy, quality 75)
    out := libnextimage.LegacyToWebP(libnextimage.ConvertInput{
        Data:         jpeg,
        Quality:      -1, // use default
        MinQuantizer: -1,
        MaxQuantizer: -1,
    })
    if out.Error != nil {
        panic(out.Error)
    }
    // out.MimeType == "image/webp"
    os.WriteFile("output.webp", out.Data, 0644)

    // JPEG → AVIF (lossy, quality 80)
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

// JPEG → WebP (auto-detected as lossy, quality 75)
const webp = legacyToWebp({ data: jpeg })
// webp.mimeType === 'image/webp'
writeFileSync('output.webp', webp.data)

// JPEG → AVIF (lossy, quality 80)
const avif = legacyToAvif({ data: jpeg, quality: 80 })
// avif.mimeType === 'image/avif'
writeFileSync('output.avif', avif.data)
```

## API Design

All conversion functions follow a uniform struct-based pattern:

**Input**: `{ data, quality?, minQuantizer?, maxQuantizer? }` — `quality` is -1/omitted for default, 0-100 for explicit. `minQuantizer`/`maxQuantizer` for AVIF fine-tuning (0-63).

**Output**: `{ data, mimeType }` (Go also has `error` field) — output image data and detected MIME type.

**Memory**: Output buffers are automatically managed. No explicit `close()` or `free()` needed.

## Platforms

- macOS: Intel (x64), Apple Silicon (ARM64)
- Linux: x64, ARM64
- Windows: x64

Pre-built binaries are available via [GitHub Releases](https://github.com/ideamans/libnextimage/releases).

## Building from Source

```bash
# Prerequisites (macOS)
brew install cmake jpeg libpng giflib

# Prerequisites (Linux)
sudo apt-get install cmake build-essential libjpeg-dev libpng-dev libgif-dev

# Build and install
make install-c
```

## Testing

```bash
# Go
cd golang && go test -v -timeout 120s

# TypeScript
cd typescript && npm install && npm test
```

## Documentation

- [Go README](golang/README.md)
- [TypeScript README](typescript/README.md)

## License

BSD 3-Clause License

- libwebp: BSD License
- libavif: BSD License
- libaom: BSD License
