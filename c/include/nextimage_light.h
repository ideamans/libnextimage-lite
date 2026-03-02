#ifndef NEXTIMAGE_LIGHT_H
#define NEXTIMAGE_LIGHT_H

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
} NextImageLightInput;

// Output for all light conversions
typedef struct {
    NextImageStatus status; // NEXTIMAGE_OK on success
    uint8_t* data;          // output data (must free with nextimage_light_free)
    size_t size;            // output data size in bytes
    char mime_type[32];     // output MIME type (e.g. "image/webp", "image/jpeg")
} NextImageLightOutput;

// Free output buffer
void nextimage_light_free(NextImageLightOutput* output);

// ========================================
// Conversion functions
// ========================================

// Legacy (JPEG/PNG/GIF) -> WebP
// JPEG: lossy (quality default=75), PNG: lossless, GIF: gif2webp default
NextImageStatus nextimage_light_legacy_to_webp(const NextImageLightInput* input, NextImageLightOutput* output);

// WebP -> Legacy (JPEG/PNG/GIF)
// animated -> GIF, lossless -> PNG, lossy -> JPEG (quality default=90)
NextImageStatus nextimage_light_webp_to_legacy(const NextImageLightInput* input, NextImageLightOutput* output);

// Legacy (JPEG/PNG) -> AVIF
// JPEG: lossy (quality/quantizer params), PNG: lossless high-precision
NextImageStatus nextimage_light_legacy_to_avif(const NextImageLightInput* input, NextImageLightOutput* output);

// AVIF -> Legacy (JPEG/PNG)
// lossless (matrixCoefficients==IDENTITY) -> PNG, lossy -> JPEG (quality default=90)
NextImageStatus nextimage_light_avif_to_legacy(const NextImageLightInput* input, NextImageLightOutput* output);

#ifdef __cplusplus
}
#endif

#endif // NEXTIMAGE_LIGHT_H
