#ifndef NEXTIMAGE_INTERNAL_H
#define NEXTIMAGE_INTERNAL_H

#include "nextimage.h"
#include <stdarg.h>

// 内部用メモリ管理関数（カウンター付き）
void* nextimage_malloc(size_t size);
void* nextimage_calloc(size_t nmemb, size_t size);
void* nextimage_realloc(void* ptr, size_t size);
void nextimage_free(void* ptr);

// 内部用エラーメッセージ設定
void nextimage_set_error(const char* format, ...);

// libpngでPNGをメモリバッファに書き出す
// channels: 3=RGB, 4=RGBA
int nextimage_write_png_to_buffer(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride);

// libjpegでJPEGをメモリバッファに書き出す
// channels: 1=grayscale, 3=RGB
int nextimage_write_jpeg_to_buffer(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride, int quality);

// ========================================
// PNGメタデータ付き書き出し（avifdec CLI互換）
// ========================================

// avifPNGWrite と同じチャンク構造を再現するためのメタデータ
typedef struct {
    // ビット深度: 8 or 16 (0はデフォルト8)
    int bit_depth;

    // cICP chunk (ICCがない場合、png_write_infoの後に書き込む)
    int write_cicp;              // 0 or 1
    uint8_t cicp_primaries;
    uint8_t cicp_transfer;
    uint8_t cicp_matrix;         // 常に 0 (IDENTITY)
    uint8_t cicp_full_range;     // 常に 1

    // sRGB (primaries==BT709 && transfer==sRGB の場合)
    // trueの場合、png_set_sRGB_gAMA_and_cHRM() で sRGB+gAMA+cHRM を一括設定
    int is_srgb;

    // cHRM chunk (色度座標、sRGBでない場合に使用)
    int has_chrm;
    double chrm_white_x, chrm_white_y;
    double chrm_red_x, chrm_red_y;
    double chrm_green_x, chrm_green_y;
    double chrm_blue_x, chrm_blue_y;

    // gAMA chunk (sRGBでない場合に使用)
    int has_gama;
    double gama_value;           // 1.0/gamma (png_set_gAMA に渡す値)

    // ICC profile
    const uint8_t* icc_data;
    size_t icc_size;

    // Exif (eXIf chunk)
    const uint8_t* exif_data;
    size_t exif_size;

    // XMP (iTXt chunk)
    const uint8_t* xmp_data;
    size_t xmp_size;
} NextImagePNGMetadata;

// PNGメタデータ付き書き出し
int nextimage_write_png_to_buffer_ex(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride,
    const NextImagePNGMetadata* metadata);

// ========================================
// JPEGメタデータ付き書き出し（avifdec CLI互換）
// ========================================

typedef struct {
    // ICC profile
    const uint8_t* icc_data;
    size_t icc_size;

    // Exif (APP1 marker)
    const uint8_t* exif_data;
    size_t exif_size;

    // XMP (APP1 marker)
    const uint8_t* xmp_data;
    size_t xmp_size;
} NextImageJPEGMetadata;

// JPEGメタデータ付き書き出し
int nextimage_write_jpeg_to_buffer_ex(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride, int quality,
    const NextImageJPEGMetadata* metadata);

// ========================================
// ICC profile extraction from image buffers
// ========================================

// PNG (メモリバッファ) からICCプロファイルを抽出
// 成功: *out_icc に malloc データ、*out_size にサイズ。呼び出し元が free()
// ICC無し or 失敗: *out_icc = NULL, *out_size = 0, return 0
int nextimage_extract_icc_from_png(
    const uint8_t* data, size_t size,
    uint8_t** out_icc, size_t* out_size);

// JPEG (メモリバッファ) からICCプロファイルを抽出
int nextimage_extract_icc_from_jpeg(
    const uint8_t* data, size_t size,
    uint8_t** out_icc, size_t* out_size);

// ========================================
// Exif orientation extraction and auto-rotation
// ========================================

// JPEG (メモリバッファ) からExif Orientationタグを抽出
// 戻り値: 1-8 (Exif orientation値), 0 (orientationなし/エラー)
int nextimage_extract_exif_orientation(const uint8_t* data, size_t size);

// JPEG画像をExif Orientationに従って自動回転
// 4:4:4サブサンプリング、品質100%で中間JPEGを生成
// orientation==1 または orientation不明の場合は入力をそのままコピー
// output は nextimage_free_buffer() で解放すること
// 戻り値: 0=成功, -1=エラー
int nextimage_jpeg_auto_orient(
    const uint8_t* data, size_t size,
    NextImageBuffer* output);

// デバッグビルド専用
#ifdef NEXTIMAGE_DEBUG
void nextimage_increment_alloc_counter(void);
void nextimage_decrement_alloc_counter(void);
#endif

#endif // NEXTIMAGE_INTERNAL_H
