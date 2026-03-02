# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**libnextimage-light** is a simplified image conversion library with bindings for Go and TypeScript/Node.js. It provides 4 unified conversion functions with automatic format detection. Built on top of a C FFI core wrapping libwebp and libavif.

## Build & Test Commands

### C Library (CMake)

```bash
# Build static (.a) and shared (.dylib/.so/.dll) libraries
make build-c

# Run C tests (internal cwebp/dwebp/avifenc/avifdec/gif2webp/webp2gif tests)
make test-c

# Build, install to lib/, and copy to golang/typescript for local dev
make install-c

# Clean build artifacts
make clean-c
```

### Go (CGO)

```bash
# Run all Go tests
make test-go
# Or directly:
cd golang && go test -v -timeout 120s
```

### TypeScript/Node.js

```bash
# Run TypeScript tests (builds first)
make test-typescript
# Or directly:
cd typescript && npm install && npm test

# Build only
cd typescript && npm run build
```

### All Tests

```bash
make test-all
```

### Build from Source

```bash
# Prerequisites (macOS):
brew install cmake jpeg libpng giflib

# Prerequisites (Linux):
sudo apt-get install cmake build-essential libjpeg-dev libpng-dev libgif-dev

# Build and install
make install-c
```

## Architecture

```
┌──────────────────────────────────────────────────┐
│  Go API (CGO)       │  TypeScript API (Koffi FFI) │
│  golang/light.go    │  typescript/src/light.ts     │
├─────────────────────┴────────────────────────────┤
│  C Light Layer: c/src/light.c                     │
│  Header: c/include/nextimage_light.h              │
│  (format detection + dispatch to internal cmds)   │
├───────────────────────────────────────────────────┤
│  Internal Commands:                               │
│  cwebp, dwebp, gif2webp, webp2gif, avifenc,      │
│  avifdec (c/src/webp.c, c/src/avif.c)            │
├───────────────────────────────────────────────────┤
│  C Core: c/src/common.c                           │
├───────────────────────────────────────────────────┤
│  deps/libwebp    │  deps/libavif (+ libaom)       │
└──────────────────┴────────────────────────────────┘
```

### Key Directories

- **c/** — Core C library. Internal command implementations in `webp.c` and `avif.c`. `light.c` provides 4 thin wrapper functions with format detection that delegate to internal commands.
- **golang/** — Go bindings via CGO. Single file `light.go` with struct-based API.
- **typescript/** — Node.js bindings via [koffi](https://koffi.dev/) FFI. `light.ts` + `ffi.ts`.
- **deps/** — Git submodules: libwebp, libavif, stb.
- **testdata/** — Test images (JPEG, PNG, GIF, WebP, AVIF).

### Platform Identifiers

Used for library paths: `darwin-arm64`, `darwin-amd64`, `linux-amd64`, `linux-arm64`, `windows-amd64`.

## API Design

4 unified functions with automatic format detection:

| Function | Input | Output | Behavior |
|----------|-------|--------|----------|
| `legacyToWebp` | JPEG/PNG/GIF | WebP | JPEG→lossy(q=75), PNG→lossless, GIF→gif2webp default |
| `webpToLegacy` | WebP | JPEG/PNG/GIF | animated→GIF, lossless→PNG, lossy→JPEG(q=90) |
| `legacyToAvif` | JPEG/PNG | AVIF | JPEG→lossy(q=60), PNG→lossless high-precision |
| `avifToLegacy` | AVIF | JPEG/PNG | lossless→PNG, lossy→JPEG(q=90) |

### C Layer

```c
typedef struct {
    const uint8_t* data; size_t size;
    int quality;        // -1=default, 0-100
    int min_quantizer;  // -1=default, 0-63 (AVIF)
    int max_quantizer;  // -1=default, 0-63 (AVIF)
} NextImageLightInput;

typedef struct {
    NextImageStatus status; uint8_t* data; size_t size;
    char mime_type[32]; // "image/webp", "image/jpeg", etc.
} NextImageLightOutput;

NextImageStatus nextimage_light_legacy_to_webp(input, output);
NextImageStatus nextimage_light_webp_to_legacy(input, output);
NextImageStatus nextimage_light_legacy_to_avif(input, output);
NextImageStatus nextimage_light_avif_to_legacy(input, output);
void nextimage_light_free(NextImageLightOutput*);
```

### Go Layer

```go
type ConvertInput struct {
    Data []byte; Quality int; MinQuantizer int; MaxQuantizer int
}
type ConvertOutput struct { Data []byte; MimeType string; Error error }

func LegacyToWebP(input ConvertInput) ConvertOutput
func WebPToLegacy(input ConvertInput) ConvertOutput
func LegacyToAvif(input ConvertInput) ConvertOutput
func AvifToLegacy(input ConvertInput) ConvertOutput
```

### TypeScript Layer

```typescript
interface ConvertInput { data: Buffer; quality?: number; minQuantizer?: number; maxQuantizer?: number }
interface ConvertOutput { data: Buffer; mimeType: string }

function legacyToWebp(input: ConvertInput): ConvertOutput
function webpToLegacy(input: ConvertInput): ConvertOutput
function legacyToAvif(input: ConvertInput): ConvertOutput
function avifToLegacy(input: ConvertInput): ConvertOutput
```

## Internal Command Layer

The light API delegates to these internal command implementations (tested independently via `make test-c`):

| Command | Source | Purpose |
|---------|--------|---------|
| cwebp | `c/src/webp.c` | Encode images to WebP (lossy/lossless) |
| dwebp | `c/src/webp.c` | Decode WebP to JPEG/PNG |
| gif2webp | `c/src/webp.c` | Convert GIF to WebP |
| webp2gif | `c/src/webp.c` | Convert static WebP to GIF |
| avifenc | `c/src/avif.c` | Encode images to AVIF |
| avifdec | `c/src/avif.c` | Decode AVIF to JPEG/PNG |

## Key Files

| Path | Purpose |
|------|---------|
| `c/include/nextimage_light.h` | Light API C header (4 functions) |
| `c/src/light.c` | Light API C implementation (format detection + dispatch) |
| `c/src/webp.c` | Internal cwebp/dwebp/gif2webp/webp2gif implementations |
| `c/src/avif.c` | Internal avifenc/avifdec implementations |
| `c/include/nextimage/cwebp.h` | cwebp command interface |
| `c/include/nextimage/dwebp.h` | dwebp command interface |
| `c/include/nextimage/gif2webp.h` | gif2webp command interface |
| `c/include/nextimage/webp2gif.h` | webp2gif command interface |
| `c/include/nextimage/avifenc.h` | avifenc command interface |
| `c/include/nextimage/avifdec.h` | avifdec command interface |
| `golang/light.go` | Go bindings (CGO, struct-based) |
| `golang/light_test.go` | Go tests |
| `typescript/src/light.ts` | TypeScript conversion functions |
| `typescript/src/ffi.ts` | Koffi FFI struct/function definitions |
| `typescript/src/types.ts` | Status codes, NextImageError class |
| `typescript/src/index.ts` | Public exports |
| `typescript/src/test/light.test.ts` | TypeScript tests |
| `typescript/src/library.ts` | Platform detection, library path resolution |
| `golang/download.go` | Auto-download logic for pre-built Go binaries |

## Memory Management

- **C**: Caller must free via `nextimage_light_free()`.
- **Go**: Output data is copied to Go slices; C buffer freed immediately. No manual cleanup needed.
- **TypeScript**: Output data is copied to Buffer; C buffer freed immediately. No manual cleanup needed.

## Release Process

1. Push tag `v0.5.0` → CI builds all platforms → GitHub Release created
2. Automated PR adds binaries to golang/typescript modules
3. Merge PR → Tag Go module (`golang/v0.5.0`) → `npm publish` in typescript/

## CI/CD

GitHub Actions test matrix: Ubuntu x64, Ubuntu ARM64, macOS ARM64, Windows x64. Workflows in `.github/workflows/`.
