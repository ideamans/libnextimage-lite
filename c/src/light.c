#include "nextimage_light.h"
#include "nextimage/cwebp.h"
#include "nextimage/dwebp.h"
#include "nextimage/gif2webp.h"
#include "nextimage/webp2gif.h"
#include "nextimage/avifenc.h"
#include "nextimage/avifdec.h"
#include "webp/decode.h"
#include "avif/avif.h"
#include <string.h>

// ========================================
// Format detection helpers
// ========================================

// Image type detected from magic bytes
typedef enum {
    IMG_TYPE_UNKNOWN = 0,
    IMG_TYPE_JPEG,
    IMG_TYPE_PNG,
    IMG_TYPE_GIF,
    IMG_TYPE_WEBP,
    IMG_TYPE_AVIF
} ImageType;

static ImageType detect_image_type(const uint8_t* data, size_t size) {
    if (size < 4) return IMG_TYPE_UNKNOWN;

    // JPEG: FF D8 FF
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return IMG_TYPE_JPEG;
    }

    // PNG: 89 50 4E 47
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        return IMG_TYPE_PNG;
    }

    // GIF: GIF87a or GIF89a
    if (data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
        return IMG_TYPE_GIF;
    }

    // WebP: RIFF....WEBP
    if (size >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
        return IMG_TYPE_WEBP;
    }

    // AVIF: ftyp box at offset 4
    if (size >= 12 && data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p') {
        return IMG_TYPE_AVIF;
    }

    return IMG_TYPE_UNKNOWN;
}

// ========================================
// Free output buffer
// ========================================
void nextimage_light_free(NextImageLightOutput* output) {
    if (output && output->data) {
        NextImageBuffer buf;
        buf.data = output->data;
        buf.size = output->size;
        nextimage_free_buffer(&buf);
        output->data = NULL;
        output->size = 0;
    }
}

// ========================================
// Legacy (JPEG/PNG/GIF) -> WebP
// ========================================
NextImageStatus nextimage_light_legacy_to_webp(const NextImageLightInput* input, NextImageLightOutput* output) {
    if (!input || !input->data || input->size == 0 || !output) {
        return NEXTIMAGE_ERROR_INVALID_PARAM;
    }

    memset(output, 0, sizeof(*output));

    ImageType type = detect_image_type(input->data, input->size);

    if (type == IMG_TYPE_GIF) {
        // GIF -> WebP via gif2webp (default options)
        Gif2WebPOptions* opts = gif2webp_create_default_options();
        if (!opts) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        Gif2WebPCommand* cmd = gif2webp_new_command(opts);
        gif2webp_free_options(opts);
        if (!cmd) {
            output->status = NEXTIMAGE_ERROR_ENCODE_FAILED;
            return output->status;
        }

        NextImageBuffer buf;
        memset(&buf, 0, sizeof(buf));

        output->status = gif2webp_run_command(cmd, input->data, input->size, &buf);
        gif2webp_free_command(cmd);

        if (output->status == NEXTIMAGE_OK) {
            output->data = buf.data;
            output->size = buf.size;
            strncpy(output->mime_type, "image/webp", sizeof(output->mime_type) - 1);
        }
        return output->status;
    }

    if (type == IMG_TYPE_JPEG || type == IMG_TYPE_PNG) {
        CWebPOptions* opts = cwebp_create_default_options();
        if (!opts) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        if (type == IMG_TYPE_PNG) {
            // PNG -> lossless WebP
            opts->lossless = 1;
        } else {
            // JPEG -> lossy WebP with quality
            if (input->quality >= 0 && input->quality <= 100) {
                opts->quality = (float)input->quality;
            }
        }

        CWebPCommand* cmd = cwebp_new_command(opts);
        cwebp_free_options(opts);
        if (!cmd) {
            output->status = NEXTIMAGE_ERROR_ENCODE_FAILED;
            return output->status;
        }

        NextImageBuffer buf;
        memset(&buf, 0, sizeof(buf));

        output->status = cwebp_run_command(cmd, input->data, input->size, &buf);
        cwebp_free_command(cmd);

        if (output->status == NEXTIMAGE_OK) {
            output->data = buf.data;
            output->size = buf.size;
            strncpy(output->mime_type, "image/webp", sizeof(output->mime_type) - 1);
        }
        return output->status;
    }

    // Unsupported input format
    output->status = NEXTIMAGE_ERROR_UNSUPPORTED;
    return output->status;
}

// ========================================
// WebP -> Legacy (JPEG/PNG/GIF)
// ========================================
NextImageStatus nextimage_light_webp_to_legacy(const NextImageLightInput* input, NextImageLightOutput* output) {
    if (!input || !input->data || input->size == 0 || !output) {
        return NEXTIMAGE_ERROR_INVALID_PARAM;
    }

    memset(output, 0, sizeof(*output));

    // Detect WebP features
    WebPBitstreamFeatures features;
    VP8StatusCode vp8_status = WebPGetFeatures(input->data, input->size, &features);
    if (vp8_status != VP8_STATUS_OK) {
        output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
        return output->status;
    }

    if (features.has_animation) {
        // Animated WebP -> GIF
        WebP2GifOptions* opts = webp2gif_create_default_options();
        if (!opts) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        WebP2GifCommand* cmd = webp2gif_new_command(opts);
        webp2gif_free_options(opts);
        if (!cmd) {
            output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
            return output->status;
        }

        NextImageBuffer buf;
        memset(&buf, 0, sizeof(buf));

        output->status = webp2gif_run_command(cmd, input->data, input->size, &buf);
        webp2gif_free_command(cmd);

        if (output->status == NEXTIMAGE_OK) {
            output->data = buf.data;
            output->size = buf.size;
            strncpy(output->mime_type, "image/gif", sizeof(output->mime_type) - 1);
        }
        return output->status;
    }

    if (features.format == 2) {
        // Lossless WebP -> PNG
        DWebPOptions* opts = dwebp_create_default_options();
        if (!opts) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        opts->output_format = DWEBP_OUTPUT_PNG;

        DWebPCommand* cmd = dwebp_new_command(opts);
        dwebp_free_options(opts);
        if (!cmd) {
            output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
            return output->status;
        }

        NextImageBuffer buf;
        memset(&buf, 0, sizeof(buf));

        output->status = dwebp_run_command(cmd, input->data, input->size, &buf);
        dwebp_free_command(cmd);

        if (output->status == NEXTIMAGE_OK) {
            output->data = buf.data;
            output->size = buf.size;
            strncpy(output->mime_type, "image/png", sizeof(output->mime_type) - 1);
        }
        return output->status;
    }

    // Lossy WebP (format == 1 or 0/mixed) -> JPEG
    {
        DWebPOptions* opts = dwebp_create_default_options();
        if (!opts) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        opts->output_format = DWEBP_OUTPUT_JPEG;
        if (input->quality >= 0 && input->quality <= 100) {
            opts->jpeg_quality = input->quality;
        }

        DWebPCommand* cmd = dwebp_new_command(opts);
        dwebp_free_options(opts);
        if (!cmd) {
            output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
            return output->status;
        }

        NextImageBuffer buf;
        memset(&buf, 0, sizeof(buf));

        output->status = dwebp_run_command(cmd, input->data, input->size, &buf);
        dwebp_free_command(cmd);

        if (output->status == NEXTIMAGE_OK) {
            output->data = buf.data;
            output->size = buf.size;
            strncpy(output->mime_type, "image/jpeg", sizeof(output->mime_type) - 1);
        }
        return output->status;
    }
}

// ========================================
// Legacy (JPEG/PNG) -> AVIF
// ========================================
NextImageStatus nextimage_light_legacy_to_avif(const NextImageLightInput* input, NextImageLightOutput* output) {
    if (!input || !input->data || input->size == 0 || !output) {
        return NEXTIMAGE_ERROR_INVALID_PARAM;
    }

    memset(output, 0, sizeof(*output));

    ImageType type = detect_image_type(input->data, input->size);

    // Reject GIF, WebP, AVIF
    if (type != IMG_TYPE_JPEG && type != IMG_TYPE_PNG) {
        output->status = NEXTIMAGE_ERROR_UNSUPPORTED;
        return output->status;
    }

    AVIFEncOptions* opts = avifenc_create_default_options();
    if (!opts) {
        output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
        return output->status;
    }

    if (type == IMG_TYPE_PNG) {
        // PNG -> lossless AVIF (high precision)
        opts->quality = 100;             // AVIF_QUALITY_LOSSLESS
        opts->min_quantizer = 0;         // AVIF_QUANTIZER_LOSSLESS
        opts->max_quantizer = 0;         // AVIF_QUANTIZER_LOSSLESS
        opts->yuv_format = 0;            // YUV444
        opts->matrix_coefficients = 0;   // AVIF_MATRIX_COEFFICIENTS_IDENTITY
        opts->speed = 0;                 // Highest precision
    } else {
        // JPEG -> lossy AVIF
        if (input->quality >= 0 && input->quality <= 100) {
            opts->quality = input->quality;
        }
        if (input->min_quantizer >= 0 && input->min_quantizer <= 63) {
            opts->min_quantizer = input->min_quantizer;
        }
        if (input->max_quantizer >= 0 && input->max_quantizer <= 63) {
            opts->max_quantizer = input->max_quantizer;
        }
    }

    AVIFEncCommand* cmd = avifenc_new_command(opts);
    avifenc_free_options(opts);
    if (!cmd) {
        output->status = NEXTIMAGE_ERROR_ENCODE_FAILED;
        return output->status;
    }

    NextImageBuffer buf;
    memset(&buf, 0, sizeof(buf));

    output->status = avifenc_run_command(cmd, input->data, input->size, &buf);
    avifenc_free_command(cmd);

    if (output->status == NEXTIMAGE_OK) {
        output->data = buf.data;
        output->size = buf.size;
        strncpy(output->mime_type, "image/avif", sizeof(output->mime_type) - 1);
    }
    return output->status;
}

// ========================================
// AVIF -> Legacy (JPEG/PNG)
// ========================================
NextImageStatus nextimage_light_avif_to_legacy(const NextImageLightInput* input, NextImageLightOutput* output) {
    if (!input || !input->data || input->size == 0 || !output) {
        return NEXTIMAGE_ERROR_INVALID_PARAM;
    }

    memset(output, 0, sizeof(*output));

    // Parse AVIF header to detect lossless
    int is_lossless = 0;
    {
        avifDecoder* decoder = avifDecoderCreate();
        if (!decoder) {
            output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
            return output->status;
        }

        avifResult result = avifDecoderSetIOMemory(decoder, input->data, input->size);
        if (result == AVIF_RESULT_OK) {
            result = avifDecoderParse(decoder);
        }

        if (result == AVIF_RESULT_OK && decoder->image) {
            // matrixCoefficients == 0 (IDENTITY) indicates lossless encoding
            if (decoder->image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_IDENTITY) {
                is_lossless = 1;
            }
        }

        avifDecoderDestroy(decoder);

        if (result != AVIF_RESULT_OK) {
            output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
            return output->status;
        }
    }

    AVIFDecOptions* opts = avifdec_create_default_options();
    if (!opts) {
        output->status = NEXTIMAGE_ERROR_OUT_OF_MEMORY;
        return output->status;
    }

    if (is_lossless) {
        // Lossless AVIF -> PNG
        opts->output_format = AVIFDEC_OUTPUT_PNG;
    } else {
        // Lossy AVIF -> JPEG
        opts->output_format = AVIFDEC_OUTPUT_JPEG;
        if (input->quality >= 0 && input->quality <= 100) {
            opts->jpeg_quality = input->quality;
        }
    }

    AVIFDecCommand* cmd = avifdec_new_command(opts);
    avifdec_free_options(opts);
    if (!cmd) {
        output->status = NEXTIMAGE_ERROR_DECODE_FAILED;
        return output->status;
    }

    NextImageBuffer buf;
    memset(&buf, 0, sizeof(buf));

    output->status = avifdec_run_command(cmd, input->data, input->size, &buf);
    avifdec_free_command(cmd);

    if (output->status == NEXTIMAGE_OK) {
        output->data = buf.data;
        output->size = buf.size;
        if (is_lossless) {
            strncpy(output->mime_type, "image/png", sizeof(output->mime_type) - 1);
        } else {
            strncpy(output->mime_type, "image/jpeg", sizeof(output->mime_type) - 1);
        }
    }
    return output->status;
}
