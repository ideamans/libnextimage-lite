#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <png.h>
#include <jpeglib.h>

// スレッドローカルストレージ用
#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang拡張（macOS含む）
    static __thread char g_error_buffer[1024] = {0};
#elif defined(_MSC_VER)
    // MSVC拡張
    static __declspec(thread) char g_error_buffer[1024] = {0};
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__APPLE__)
    // C11 thread_local（macOS以外）
    #include <threads.h>
    static thread_local char g_error_buffer[1024] = {0};
#else
    // フォールバック: スレッドセーフではない
    #warning "Thread-local storage not supported, error messages may not be thread-safe"
    static char g_error_buffer[1024] = {0};
#endif

// デバッグビルド専用: メモリリークカウンター
#ifdef NEXTIMAGE_DEBUG
#include <stdatomic.h>
static atomic_int_least64_t g_allocation_counter = 0;

void nextimage_increment_alloc_counter(void) {
    atomic_fetch_add(&g_allocation_counter, 1);
}

void nextimage_decrement_alloc_counter(void) {
    atomic_fetch_sub(&g_allocation_counter, 1);
}

int64_t nextimage_allocation_counter(void) {
    return atomic_load(&g_allocation_counter);
}
#else
void nextimage_increment_alloc_counter(void) {}
void nextimage_decrement_alloc_counter(void) {}
#endif

// 内部用: エラーメッセージを設定
void nextimage_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(g_error_buffer, sizeof(g_error_buffer), format, args);
    va_end(args);
}

// エラーメッセージ取得
const char* nextimage_last_error_message(void) {
    if (g_error_buffer[0] == '\0') {
        return NULL;
    }
    return g_error_buffer;
}

// エラーメッセージのクリア
void nextimage_clear_error(void) {
    g_error_buffer[0] = '\0';
}

// バッファの解放
void nextimage_free_buffer(NextImageBuffer* buffer) {
    if (buffer && buffer->data) {
        free(buffer->data);
        nextimage_decrement_alloc_counter();
        buffer->data = NULL;
        buffer->size = 0;
    }
}

// デコードバッファの解放
void nextimage_free_decode_buffer(NextImageDecodeBuffer* buffer) {
    if (!buffer || !buffer->owns_data) {
        return;
    }

    if (buffer->data) {
        free(buffer->data);
        nextimage_decrement_alloc_counter();
        buffer->data = NULL;
    }

    if (buffer->u_plane) {
        free(buffer->u_plane);
        nextimage_decrement_alloc_counter();
        buffer->u_plane = NULL;
    }

    if (buffer->v_plane) {
        free(buffer->v_plane);
        nextimage_decrement_alloc_counter();
        buffer->v_plane = NULL;
    }

    buffer->data_capacity = 0;
    buffer->data_size = 0;
    buffer->u_capacity = 0;
    buffer->u_size = 0;
    buffer->v_capacity = 0;
    buffer->v_size = 0;
    buffer->owns_data = 0;
}

// バージョン取得
const char* nextimage_version(void) {
    static char version[64];
    snprintf(version, sizeof(version), "%d.%d.%d",
             NEXTIMAGE_VERSION_MAJOR,
             NEXTIMAGE_VERSION_MINOR,
             NEXTIMAGE_VERSION_PATCH);
    return version;
}

// 内部ヘルパー: メモリ割り当て（カウンター付き）
void* nextimage_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        nextimage_increment_alloc_counter();
    }
    return ptr;
}

void* nextimage_calloc(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (ptr) {
        nextimage_increment_alloc_counter();
    }
    return ptr;
}

void* nextimage_realloc(void* ptr, size_t size) {
    int was_allocated = (ptr != NULL);
    void* new_ptr = realloc(ptr, size);

    if (new_ptr && !was_allocated) {
        // 新規割り当て (ptr was NULL, now allocated)
        nextimage_increment_alloc_counter();
    }
    // Note: If realloc fails (new_ptr == NULL), the original ptr is still valid,
    // so we do NOT decrement the counter. The caller is responsible for
    // freeing the original pointer in this case.

    return new_ptr;
}

void nextimage_free(void* ptr) {
    if (ptr) {
        free(ptr);
        nextimage_decrement_alloc_counter();
    }
}

// ========================================
// libpng メモリライター
// ========================================

// libpng用のカスタムwrite関数: NextImageBufferに追記する
static void png_write_to_buffer(png_structp png_ptr, png_bytep data, png_size_t length) {
    NextImageBuffer* buf = (NextImageBuffer*)png_get_io_ptr(png_ptr);
    size_t new_size = buf->size + length;

    uint8_t* new_data = (uint8_t*)nextimage_realloc(buf->data, new_size);
    if (!new_data) {
        png_error(png_ptr, "Memory allocation failed");
        return;
    }

    memcpy(new_data + buf->size, data, length);
    buf->data = new_data;
    buf->size = new_size;
}

static void png_flush_buffer(png_structp png_ptr) {
    (void)png_ptr; // no-op for memory buffer
}

// libpngでPNGをメモリバッファに書き出す
// channels: 3=RGB → PNG_COLOR_TYPE_RGB, 4=RGBA → PNG_COLOR_TYPE_RGBA
// 設定: libwebp WebPWritePNG (imageio/image_enc.c) と同一
int nextimage_write_png_to_buffer(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride)
{
    if (!output || !pixels || width <= 0 || height <= 0 ||
        (channels != 3 && channels != 4)) {
        return -1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                   NULL, NULL, NULL);
    if (!png_ptr) {
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
    }

    // カスタムwrite関数を使用（メモリバッファ出力）
    png_set_write_fn(png_ptr, output, png_write_to_buffer, png_flush_buffer);

    // IHDR設定: libwebp WebPWritePNG と同一
    int color_type = (channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png_ptr, info_ptr, (png_uint_32)width, (png_uint_32)height,
                 8, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // 行ごとに書き込み
    for (int y = 0; y < height; y++) {
        png_write_row(png_ptr, (png_const_bytep)(pixels + y * stride));
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return 0;
}

// ========================================
// libjpeg メモリライター
// ========================================

// libjpegでJPEGをメモリバッファに書き出す
// 設定: avifdec CLI (avifjpeg.c) と同一
//   jpeg_set_defaults() + jpeg_set_quality(quality, TRUE)
int nextimage_write_jpeg_to_buffer(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride, int quality)
{
    if (!output || !pixels || width <= 0 || height <= 0 ||
        (channels != 1 && channels != 3)) {
        return -1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // jpeg_mem_dest でメモリバッファに出力
    unsigned char* jpeg_buf = NULL;
    unsigned long jpeg_size = 0;
    jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_size);

    cinfo.image_width = (JDIMENSION)width;
    cinfo.image_height = (JDIMENSION)height;
    cinfo.input_components = channels;
    cinfo.in_color_space = (channels == 1) ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    // 行ごとに書き込み
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = (JSAMPROW)(pixels + cinfo.next_scanline * stride);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    // jpeg_mem_dest の結果を NextImageBuffer にコピー
    output->data = (uint8_t*)nextimage_malloc(jpeg_size);
    if (!output->data) {
        free(jpeg_buf);
        return -1;
    }
    memcpy(output->data, jpeg_buf, jpeg_size);
    output->size = jpeg_size;

    // jpeg_mem_dest が割り当てたバッファを解放
    free(jpeg_buf);

    return 0;
}

// ========================================
// PNGメタデータ付き書き出し（avifdec CLI avifPNGWrite互換）
// ========================================

int nextimage_write_png_to_buffer_ex(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride,
    const NextImagePNGMetadata* metadata)
{
    if (!output || !pixels || width <= 0 || height <= 0 ||
        (channels != 3 && channels != 4)) {
        return -1;
    }

    // メタデータなしの場合は基本関数にフォールバック
    if (!metadata) {
        return nextimage_write_png_to_buffer(output, pixels, width, height, channels, stride);
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                   NULL, NULL, NULL);
    if (!png_ptr) {
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
    }

    png_set_write_fn(png_ptr, output, png_write_to_buffer, png_flush_buffer);

    // IHDR設定: avifPNGWrite と同一
    int color_type = (channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    int png_depth = (metadata->bit_depth == 16) ? 16 : 8;
    png_set_IHDR(png_ptr, info_ptr, (png_uint_32)width, (png_uint_32)height,
                 png_depth, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    // --- 色空間チャンク（avifPNGWrite:636-670 と同一ロジック） ---

    int has_icc = (metadata->icc_data && metadata->icc_size > 0);

    // ICC profile
    if (has_icc) {
        png_set_iCCP(png_ptr, info_ptr, "libavif", 0,
                     (png_const_bytep)metadata->icc_data,
                     (png_uint_32)metadata->icc_size);
    }

    // sRGB shortcut: png_set_sRGB_gAMA_and_cHRM で sRGB + gAMA + cHRM を一括書き込み
    if (metadata->is_srgb) {
        png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_PERCEPTUAL);
    } else {
        // cHRM chunk
        if (metadata->has_chrm) {
            png_set_cHRM(png_ptr, info_ptr,
                         metadata->chrm_white_x, metadata->chrm_white_y,
                         metadata->chrm_red_x, metadata->chrm_red_y,
                         metadata->chrm_green_x, metadata->chrm_green_y,
                         metadata->chrm_blue_x, metadata->chrm_blue_y);
        }
        // gAMA chunk
        if (metadata->has_gama) {
            png_set_gAMA(png_ptr, info_ptr, metadata->gama_value);
        }
    }

    // --- Exif (eXIf chunk, avifPNGWrite:675-680) ---
    if (metadata->exif_data && metadata->exif_size > 0) {
        png_set_eXIf_1(png_ptr, info_ptr,
                        (png_uint_32)metadata->exif_size,
                        (png_bytep)metadata->exif_data);
    }

    // --- XMP (iTXt chunk, avifPNGWrite:682-707) ---
    if (metadata->xmp_data && metadata->xmp_size > 0) {
        // XMP needs null termination for libpng
        uint8_t* xmp_copy = (uint8_t*)malloc(metadata->xmp_size + 1);
        if (xmp_copy) {
            memcpy(xmp_copy, metadata->xmp_data, metadata->xmp_size);
            xmp_copy[metadata->xmp_size] = '\0';

            png_text text;
            memset(&text, 0, sizeof(text));
            text.compression = PNG_ITXT_COMPRESSION_NONE;
            text.key = "XML:com.adobe.xmp";
            text.text = (char*)xmp_copy;
            text.itxt_length = metadata->xmp_size;
            png_set_text(png_ptr, info_ptr, &text, 1);
            free(xmp_copy);
        }
    }

    // png_write_info: 標準チャンクを書き込み
    png_write_info(png_ptr, info_ptr);

    // --- cICP custom chunk (avifPNGWrite:712-722) ---
    // ICCがない場合のみ、png_write_info の後に書き込む
    if (metadata->write_cicp && !has_icc) {
        const png_byte cicp_tag[5] = "cICP";
        const png_byte cicp_data[4] = {
            metadata->cicp_primaries,
            metadata->cicp_transfer,
            metadata->cicp_matrix,
            metadata->cicp_full_range
        };
        png_write_chunk(png_ptr, cicp_tag, cicp_data, 4);
    }

    // 16-bit: バイトスワップ (avifPNGWrite:764-766)
    if (png_depth > 8) {
        png_set_swap(png_ptr);
    }

    // ピクセルデータ書き込み
    for (int y = 0; y < height; y++) {
        png_write_row(png_ptr, (png_const_bytep)(pixels + y * stride));
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return 0;
}

// ========================================
// JPEGメタデータ付き書き出し（avifdec CLI avifJPEGWrite互換）
// ========================================

// ICC profile をAPP2マーカーとして書き込み（avifjpeg.c write_icc_profile互換）
static void nextimage_jpeg_write_icc(struct jpeg_compress_struct* cinfo,
                                      const uint8_t* icc_data, size_t icc_size) {
    // ICC_PROFILE marker header: "ICC_PROFILE\0" + seq_no + num_markers
    static const char icc_header[] = "ICC_PROFILE";
    const size_t header_len = 14; // "ICC_PROFILE\0" + 2 bytes
    const size_t max_data_per_marker = 65533 - header_len; // JPEG marker max = 65535 - 2

    size_t num_markers = (icc_size + max_data_per_marker - 1) / max_data_per_marker;
    if (num_markers == 0) num_markers = 1;

    size_t offset = 0;
    for (size_t i = 0; i < num_markers; i++) {
        size_t chunk_size = icc_size - offset;
        if (chunk_size > max_data_per_marker) chunk_size = max_data_per_marker;

        size_t marker_size = header_len + chunk_size;
        uint8_t* marker_data = (uint8_t*)malloc(marker_size);
        if (!marker_data) break;

        memcpy(marker_data, icc_header, 12); // "ICC_PROFILE\0"
        marker_data[12] = (uint8_t)(i + 1);           // seq_no (1-based)
        marker_data[13] = (uint8_t)num_markers;       // total markers
        memcpy(marker_data + header_len, icc_data + offset, chunk_size);

        jpeg_write_marker(cinfo, JPEG_APP0 + 2, marker_data, (unsigned int)marker_size);
        free(marker_data);
        offset += chunk_size;
    }
}

int nextimage_write_jpeg_to_buffer_ex(
    NextImageBuffer* output,
    const uint8_t* pixels, int width, int height,
    int channels, int stride, int quality,
    const NextImageJPEGMetadata* metadata)
{
    if (!output || !pixels || width <= 0 || height <= 0 ||
        (channels != 1 && channels != 3)) {
        return -1;
    }

    // メタデータなしの場合は基本関数にフォールバック
    if (!metadata) {
        return nextimage_write_jpeg_to_buffer(output, pixels, width, height,
                                              channels, stride, quality);
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* jpeg_buf = NULL;
    unsigned long jpeg_size = 0;
    jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_size);

    cinfo.image_width = (JDIMENSION)width;
    cinfo.image_height = (JDIMENSION)height;
    cinfo.input_components = channels;
    cinfo.in_color_space = (channels == 1) ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    // ICC profile (APP2 markers)
    if (metadata->icc_data && metadata->icc_size > 0) {
        nextimage_jpeg_write_icc(&cinfo, metadata->icc_data, metadata->icc_size);
    }

    // Exif (APP1 markers, avifjpeg.c:1334-1371)
    if (metadata->exif_data && metadata->exif_size > 0) {
        const size_t max_marker = 65533;
        const uint8_t* p = metadata->exif_data;
        size_t remaining = metadata->exif_size;
        while (remaining > max_marker) {
            jpeg_write_marker(&cinfo, JPEG_APP0 + 1, p, (unsigned int)max_marker);
            p += max_marker;
            remaining -= max_marker;
        }
        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, p, (unsigned int)remaining);
    }

    // XMP (APP1 marker, avifjpeg.c:1380-1404)
    if (metadata->xmp_data && metadata->xmp_size > 0) {
        static const char xmp_tag[] = "http://ns.adobe.com/xap/1.0/";
        const size_t tag_len = sizeof(xmp_tag); // includes null terminator
        size_t marker_size = tag_len + metadata->xmp_size;
        if (marker_size <= 65533) {
            uint8_t* marker = (uint8_t*)malloc(marker_size);
            if (marker) {
                memcpy(marker, xmp_tag, tag_len);
                memcpy(marker + tag_len, metadata->xmp_data, metadata->xmp_size);
                jpeg_write_marker(&cinfo, JPEG_APP0 + 1, marker, (unsigned int)marker_size);
                free(marker);
            }
        }
    }

    // ピクセルデータ書き込み
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = (JSAMPROW)(pixels + cinfo.next_scanline * stride);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    output->data = (uint8_t*)nextimage_malloc(jpeg_size);
    if (!output->data) {
        free(jpeg_buf);
        return -1;
    }
    memcpy(output->data, jpeg_buf, jpeg_size);
    output->size = jpeg_size;

    free(jpeg_buf);

    return 0;
}

// ========================================
// ICC profile extraction from memory buffers
// ========================================

// libpng用のカスタムread関数: メモリバッファから読み出す
typedef struct {
    const uint8_t* data;
    size_t size;
    size_t offset;
} PNGReadContext;

static void png_read_from_memory(png_structp png_ptr, png_bytep out, png_size_t length) {
    PNGReadContext* ctx = (PNGReadContext*)png_get_io_ptr(png_ptr);
    if (ctx->offset + length > ctx->size) {
        png_error(png_ptr, "Read past end of buffer");
        return;
    }
    memcpy(out, ctx->data + ctx->offset, length);
    ctx->offset += length;
}

int nextimage_extract_icc_from_png(
    const uint8_t* data, size_t size,
    uint8_t** out_icc, size_t* out_size)
{
    *out_icc = NULL;
    *out_size = 0;

    if (!data || size < 8) return 0;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return 0;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return 0;
    }

    PNGReadContext ctx = { data, size, 0 };
    png_set_read_fn(png_ptr, &ctx, png_read_from_memory);

    png_read_info(png_ptr, info_ptr);

    // Try to get iCCP chunk
    png_charp name;
    int compression_type;
    png_bytep profile;
    png_uint_32 proflen;

    if (png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &profile, &proflen) == PNG_INFO_iCCP) {
        if (proflen > 0 && profile) {
            *out_icc = (uint8_t*)malloc(proflen);
            if (*out_icc) {
                memcpy(*out_icc, profile, proflen);
                *out_size = proflen;
            }
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 0;
}

// JPEG ICC extraction: parse APP2 markers with "ICC_PROFILE\0" header
// Based on iccjpeg.c read_icc_profile() logic
int nextimage_extract_icc_from_jpeg(
    const uint8_t* data, size_t size,
    uint8_t** out_icc, size_t* out_size)
{
    *out_icc = NULL;
    *out_size = 0;

    if (!data || size < 2) return 0;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    // Request APP2 markers to be saved (ICC_PROFILE lives in APP2)
    jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    // Scan saved markers for ICC_PROFILE chunks
    // ICC_PROFILE header: "ICC_PROFILE\0" (12 bytes) + seq_no (1 byte) + num_markers (1 byte)
    static const char icc_signature[] = "ICC_PROFILE";
    const size_t icc_header_len = 14; // 12 (signature+null) + 2 (seq + count)

    // First pass: count markers and total size
    int num_markers = 0;
    jpeg_saved_marker_ptr marker;
    for (marker = cinfo.marker_list; marker; marker = marker->next) {
        if (marker->marker == (JPEG_APP0 + 2) &&
            marker->data_length >= icc_header_len &&
            memcmp(marker->data, icc_signature, 12) == 0) {
            int seq = marker->data[12];
            int count = marker->data[13];
            if (seq >= 1 && seq <= count) {
                if (count > num_markers) num_markers = count;
            }
        }
    }

    if (num_markers == 0) {
        jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    // Collect chunk pointers and sizes, indexed by sequence number
    const uint8_t** chunk_data = (const uint8_t**)calloc(num_markers, sizeof(const uint8_t*));
    size_t* chunk_sizes = (size_t*)calloc(num_markers, sizeof(size_t));
    if (!chunk_data || !chunk_sizes) {
        free(chunk_data);
        free(chunk_sizes);
        jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    size_t total_size = 0;
    for (marker = cinfo.marker_list; marker; marker = marker->next) {
        if (marker->marker == (JPEG_APP0 + 2) &&
            marker->data_length >= icc_header_len &&
            memcmp(marker->data, icc_signature, 12) == 0) {
            int seq = marker->data[12]; // 1-based
            int count = marker->data[13];
            if (seq >= 1 && seq <= count && seq <= num_markers) {
                size_t payload_size = marker->data_length - icc_header_len;
                chunk_data[seq - 1] = marker->data + icc_header_len;
                chunk_sizes[seq - 1] = payload_size;
                total_size += payload_size;
            }
        }
    }

    // Verify all chunks are present
    int complete = 1;
    for (int i = 0; i < num_markers; i++) {
        if (!chunk_data[i]) {
            complete = 0;
            break;
        }
    }

    if (complete && total_size > 0) {
        *out_icc = (uint8_t*)malloc(total_size);
        if (*out_icc) {
            size_t offset = 0;
            for (int i = 0; i < num_markers; i++) {
                memcpy(*out_icc + offset, chunk_data[i], chunk_sizes[i]);
                offset += chunk_sizes[i];
            }
            *out_size = total_size;
        }
    }

    free(chunk_data);
    free(chunk_sizes);
    jpeg_abort_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return 0;
}
