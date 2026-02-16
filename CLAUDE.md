# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

**libnextimage** is a multi-language image processing library providing WebP and AVIF encoding/decoding. A C FFI core wraps libwebp and libavif, with bindings for Go (CGO), TypeScript/Node.js (Koffi FFI), Bun, and Deno.

Core design principle: **byte-for-byte identical output** to official CLI tools (`cwebp`, `dwebp`, `avifenc`, `avifdec`, `gif2webp`).

## Build & Test Commands

### C Library (CMake)

```bash
# Build static (.a) and shared (.dylib/.so/.dll) libraries
make build-c

# Run C tests
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
cd golang && go test -v -timeout 30s

# Run a single test
cd golang && go test -v -run TestWebPEncode -timeout 30s
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

### Build from Source (full pipeline)

```bash
# Prerequisites (macOS):
brew install cmake jpeg libpng giflib

# Prerequisites (Linux):
sudo apt-get install cmake build-essential libjpeg-dev libpng-dev libgif-dev

# Build C library and install to lib/
bash scripts/build-c-library.sh

# Or use Makefile (also copies to golang/typescript dirs)
make install-c
```

## Architecture

```
┌─────────────────────────────────────────────────┐
│  Go API (CGO)      │  TypeScript API (Koffi FFI) │
│  golang/            │  typescript/src/             │
├─────────────────────┴───────────────────────────┤
│  C FFI Layer: c/src/{webp.c, avif.c, common.c}  │
│  Headers: c/include/{nextimage.h, webp.h, avif.h}│
├──────────────────────────────────────────────────┤
│  deps/libwebp    │  deps/libavif (+ libaom)      │
└──────────────────┴───────────────────────────────┘
```

- **c/** — Core C library. CMake builds static and shared libraries. All codec options are exposed as C structs/functions.
- **golang/** — Go bindings via CGO. Pre-built static libraries embedded in `golang/shared/lib/{platform}/`. No external Go dependencies.
- **typescript/** — Node.js bindings via [koffi](https://koffi.dev/) FFI. Pre-built shared libraries in `typescript/lib/{platform}/`. Postinstall auto-downloads binaries.
- **deps/** — Git submodules: libwebp, libavif, stb.
- **testdata/** — Test images (JPEG, PNG, GIF, WebP, AVIF).
- **scripts/** — Build automation, binary installer, test image generation.

### Platform Identifiers

Used throughout for library paths: `darwin-arm64`, `darwin-amd64`, `linux-amd64`, `linux-arm64`, `windows-amd64`.

### Memory Management

- **C**: Caller must free via `nextimage_buffer_free()`.
- **Go**: `runtime.SetFinalizer` handles cleanup automatically.
- **TypeScript**: `FinalizationRegistry` for GC + explicit `close()` method. Always call `close()` when done.

## Key Files

| Path | Purpose |
|------|---------|
| `c/src/webp.c` | WebP encode/decode implementation (~57KB) |
| `c/src/avif.c` | AVIF encode/decode implementation (~38KB) |
| `c/include/nextimage.h` | Common types: status codes, pixel formats, buffers |
| `c/include/webp.h` | WebP FFI option structs and function declarations |
| `c/include/avif.h` | AVIF FFI option structs and function declarations |
| `golang/download.go` | Auto-download logic for pre-built binaries |
| `typescript/src/ffi.ts` | Koffi struct/function definitions (~16KB) |
| `typescript/src/library.ts` | Platform detection, library path resolution |

## Release Process

1. Push tag `v0.5.0` → CI builds all platforms → GitHub Release created
2. Automated PR adds binaries to golang/typescript modules
3. Merge PR → Tag Go modules (`golang/v0.5.0`, `golang/cwebp/v0.5.0`, etc.) → `npm publish` in typescript/

See `RELEASE.md` for full details.

## CI/CD

GitHub Actions test matrix: Ubuntu x64, Ubuntu ARM64, macOS ARM64, Windows x64. Workflows in `.github/workflows/`: `test.yml` (build + test), `release.yml` (binary builds), `update-binaries.yml` (automated PR with binaries).

## Reference Documentation

- `SPEC.md` — Detailed technical specification for the C API and CLI compatibility
- `COMPAT.md` — Platform/version compatibility notes
- `OPTION-REVIEW.md` — All encoding/decoding options documented
- `TYPESCRIPT.md` — TypeScript implementation details
- `TEST-SPEC.md` — Comprehensive test specification
