#!/bin/bash

# libwebp/libavif コマンドラインツールのビルド

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-cli-tools"
BIN_DIR="$PROJECT_ROOT/bin"

# CPU数の取得 (cross-platform)
if command -v sysctl >/dev/null 2>&1; then
    NCPU=$(sysctl -n hw.ncpu)
elif command -v nproc >/dev/null 2>&1; then
    NCPU=$(nproc)
else
    NCPU=4
fi

echo "=== コマンドラインツールビルド開始 ==="
echo "ビルドディレクトリ: $BUILD_DIR"
echo "出力ディレクトリ: $BIN_DIR"
echo "並列ジョブ数: $NCPU"
echo ""

mkdir -p "$BIN_DIR"

# ======================
# libwebp ツールのビルド
# ======================

echo "=== 1. libwebp コマンドのビルド ==="

LIBWEBP_DIR="$PROJECT_ROOT/deps/libwebp"
LIBWEBP_BUILD_DIR="$BUILD_DIR/libwebp"

mkdir -p "$LIBWEBP_BUILD_DIR"
cd "$LIBWEBP_BUILD_DIR"

cmake "$LIBWEBP_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DWEBP_BUILD_CWEBP=ON \
    -DWEBP_BUILD_DWEBP=ON \
    -DWEBP_BUILD_GIF2WEBP=ON \
    -DWEBP_BUILD_WEBPMUX=ON \
    -DWEBP_BUILD_IMG2WEBP=OFF \
    -DWEBP_BUILD_VWEBP=OFF \
    -DWEBP_BUILD_WEBPINFO=OFF \
    -DWEBP_BUILD_EXTRAS=OFF \
    -DWEBP_BUILD_ANIM_UTILS=ON

cmake --build . --config Release -j$NCPU

# バイナリをコピー
cp cwebp "$BIN_DIR/"
cp dwebp "$BIN_DIR/"
if [ -f gif2webp ]; then
    cp gif2webp "$BIN_DIR/"
fi

echo "  ✓ cwebp: $BIN_DIR/cwebp"
echo "  ✓ dwebp: $BIN_DIR/dwebp"
if [ -f "$BIN_DIR/gif2webp" ]; then
    echo "  ✓ gif2webp: $BIN_DIR/gif2webp"
fi

# バージョン確認
echo ""
echo "cwebp version:"
"$BIN_DIR/cwebp" -version 2>&1 | head -1 || true

echo ""

# ======================
# libavif ツールのビルド
# ======================

echo "=== 2. libavif コマンドのビルド ==="

LIBAVIF_DIR="$PROJECT_ROOT/deps/libavif"
LIBAVIF_BUILD_DIR="$BUILD_DIR/libavif"

# Check if aom source is cached in the main C build
MAIN_AOM_SRC="$PROJECT_ROOT/c/build/_deps/libaom-src"
FETCHCONTENT_BASE="$LIBAVIF_BUILD_DIR/_deps"

mkdir -p "$LIBAVIF_BUILD_DIR"

# If aom source exists in main build, symlink it to avoid re-download
if [ -d "$MAIN_AOM_SRC" ] && [ ! -d "$FETCHCONTENT_BASE/libaom-src" ]; then
    echo "  Using cached aom source from main build: $MAIN_AOM_SRC"
    mkdir -p "$FETCHCONTENT_BASE"
    cp -a "$MAIN_AOM_SRC" "$FETCHCONTENT_BASE/libaom-src"
fi

cd "$LIBAVIF_BUILD_DIR"

cmake "$LIBAVIF_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVIF_BUILD_APPS=ON \
    -DAVIF_CODEC_AOM=LOCAL \
    -DAVIF_CODEC_DAV1D=OFF \
    -DAVIF_LIBYUV=OFF \
    -DAVIF_BUILD_TESTS=OFF \
    -DAVIF_BUILD_EXAMPLES=OFF \
    -DAVIF_ENABLE_WERROR=OFF \
    -DFETCHCONTENT_SOURCE_DIR_LIBAOM="$MAIN_AOM_SRC"

cmake --build . --config Release -j$NCPU

# バイナリをコピー
cp avifenc "$BIN_DIR/" || echo "  ⚠ avifenc not found"
cp avifdec "$BIN_DIR/" || echo "  ⚠ avifdec not found"

if [ -f "$BIN_DIR/avifenc" ]; then
    echo "  ✓ avifenc: $BIN_DIR/avifenc"
fi
if [ -f "$BIN_DIR/avifdec" ]; then
    echo "  ✓ avifdec: $BIN_DIR/avifdec"
fi

# バージョン確認
echo ""
if [ -f "$BIN_DIR/avifenc" ]; then
    echo "avifenc version:"
    "$BIN_DIR/avifenc" --version 2>&1 | head -1 || true
fi

echo ""
echo "=== ビルド完了 ==="
echo ""
echo "生成されたコマンド:"
ls -lh "$BIN_DIR"
