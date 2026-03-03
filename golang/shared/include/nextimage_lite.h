#ifndef NEXTIMAGE_LITE_H
#define NEXTIMAGE_LITE_H

#include "nextimage.h"

#ifdef __cplusplus
extern "C" {
#endif

// ========================================
// libnextimage Light API v2
// ========================================
// 4 unified conversion functions with automatic format detection.
// Input format is detected from magic bytes; output format is determined
// by the conversion type and input characteristics.

// Input for all light conversions
typedef struct {
    const uint8_t* data;   // input image data
    size_t size;           // input data size in bytes
    int quality;           // -1=use default, 0-100=explicit quality
    int min_quantizer;     // -1=use default, 0-63 (AVIF lossy min quantizer, 0=best quality)
    int max_quantizer;     // -1=use default, 0-63 (AVIF lossy max quantizer, 63=worst quality)
} NextImageLiteInput;

// Output for all light conversions
typedef struct {
    NextImageStatus status; // NEXTIMAGE_OK on success
    uint8_t* data;          // output data (must free with nextimage_lite_free)
    size_t size;            // output data size in bytes
    char mime_type[32];     // output MIME type (e.g. "image/webp", "image/jpeg")
} NextImageLiteOutput;

// Free output buffer
void nextimage_lite_free(NextImageLiteOutput* output);

// ========================================
// Conversion functions
// ========================================

// Legacy (JPEG/PNG/GIF) -> WebP
// JPEG: lossy (quality default=75), PNG: lossless, GIF: gif2webp default
NextImageStatus nextimage_lite_legacy_to_webp(const NextImageLiteInput* input, NextImageLiteOutput* output);

// WebP -> Legacy (JPEG/PNG/GIF)
// animated -> GIF, lossless -> PNG, lossy -> JPEG (quality default=90)
NextImageStatus nextimage_lite_webp_to_legacy(const NextImageLiteInput* input, NextImageLiteOutput* output);

// Legacy (JPEG/PNG) -> AVIF
// JPEG: lossy (quality/quantizer params), PNG: lossless high-precision
NextImageStatus nextimage_lite_legacy_to_avif(const NextImageLiteInput* input, NextImageLiteOutput* output);

// AVIF -> Legacy (JPEG/PNG)
// lossless (matrixCoefficients==IDENTITY) -> PNG, lossy -> JPEG (quality default=90)
NextImageStatus nextimage_lite_avif_to_legacy(const NextImageLiteInput* input, NextImageLiteOutput* output);

#ifdef __cplusplus
}
#endif

#endif // NEXTIMAGE_LITE_H
