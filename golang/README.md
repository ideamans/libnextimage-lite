# libnextimage - Go Bindings

Simplified image conversion library for Go. Provides 4 unified conversion functions with automatic format detection.

## Installation

```bash
go get github.com/ideamans/libnextimage/golang
```

## Conversion Functions

| Function | Input | Output | Behavior |
|----------|-------|--------|----------|
| `LegacyToWebP` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp |
| `WebPToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `LegacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless |
| `AvifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) |

## Quick Start

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

## API Reference

### Types

```go
// ConvertInput represents input for all conversion functions.
type ConvertInput struct {
    Data         []byte // input image data
    Quality      int    // -1=use default, 0-100=explicit quality
    MinQuantizer int    // -1=use default, 0-63 (AVIF lossy min quantizer)
    MaxQuantizer int    // -1=use default, 0-63 (AVIF lossy max quantizer)
}

// ConvertOutput represents output from all conversion functions.
type ConvertOutput struct {
    Data     []byte // output image data
    MimeType string // "image/webp", "image/jpeg", "image/png", "image/gif"
    Error    error  // nil on success
}
```

### Functions

```go
func LegacyToWebP(input ConvertInput) ConvertOutput  // JPEG/PNG/GIF → WebP
func WebPToLegacy(input ConvertInput) ConvertOutput   // WebP → JPEG/PNG/GIF (auto)
func LegacyToAvif(input ConvertInput) ConvertOutput   // JPEG/PNG → AVIF
func AvifToLegacy(input ConvertInput) ConvertOutput   // AVIF → JPEG/PNG (auto)

func Version() string  // Returns library version
```

### Memory Management

Output buffers are automatically managed. No explicit `Close()` or `Free()` is needed.

## Platform Support

| Platform | Architecture | Status |
|----------|-------------|--------|
| macOS | ARM64 (M1/M2/M3) | Supported |
| macOS | x64 | Supported |
| Linux | x64 | Supported |
| Linux | ARM64 | Supported |
| Windows | x64 | Supported |

## CGO Requirements

This package requires CGO to be enabled:

```bash
CGO_ENABLED=1 go build
```

Pre-built static libraries are embedded in `shared/lib/{platform}/`. No external library installation is needed.

## Testing

```bash
cd golang && go test -v -timeout 120s
```

## License

MIT License (c) 2025 Ideamans Inc.
