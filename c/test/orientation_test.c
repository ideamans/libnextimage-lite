/**
 * Exif Orientation Test
 *
 * Verifies:
 * 1. Exif orientation extraction from JPEG
 * 2. Auto-orient (rotation) of JPEG images
 * 3. Output has no rotation needed (orientation absent or 1)
 * 4. Dimensions are correct after rotation
 * 5. Pixel content reflects correct rotation (quadrant colors)
 *
 * Test images (testdata/orientation/):
 * - orientation-1.jpg: 80x60, orientation=1 (normal)
 * - orientation-3.jpg: 80x60, orientation=3 (180°)
 * - orientation-6.jpg: 60x80, orientation=6 (90° CW needed)
 * - orientation-8.jpg: 60x80, orientation=8 (270° CW needed)
 *
 * All images should render as 80x60 with quadrants:
 *   Top-left=RED, Top-right=GREEN, Bottom-left=BLUE, Bottom-right=WHITE
 */

#include "nextimage.h"
#include "nextimage_lite.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <jpeglib.h>

static uint8_t* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = malloc(*size);
    if (!data) { fclose(f); return NULL; }
    size_t read = fread(data, 1, *size, f);
    fclose(f);
    if (read != *size) { free(data); return NULL; }
    return data;
}

// Decode JPEG to RGB pixels
typedef struct {
    uint8_t* pixels;
    int width;
    int height;
    int channels;
} DecodedImage;

static DecodedImage decode_jpeg(const uint8_t* data, size_t size) {
    DecodedImage img = {0};

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, (unsigned long)size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return img;
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    img.width = (int)cinfo.output_width;
    img.height = (int)cinfo.output_height;
    img.channels = (int)cinfo.output_components;

    int stride = img.width * img.channels;
    img.pixels = (uint8_t*)malloc(stride * img.height);
    if (!img.pixels) {
        jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        img.width = img.height = img.channels = 0;
        return img;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = img.pixels + cinfo.output_scanline * stride;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return img;
}

// Check if a pixel is "close to" a reference color (JPEG lossy tolerance)
static int color_matches(const uint8_t* pixel, int r, int g, int b, int tolerance) {
    return (abs(pixel[0] - r) <= tolerance &&
            abs(pixel[1] - g) <= tolerance &&
            abs(pixel[2] - b) <= tolerance);
}

// ========================================
// Test: Exif orientation extraction
// ========================================
void test_extract_orientation(void) {
    printf("\nTesting Exif orientation extraction...\n");

    struct {
        const char* filename;
        int expected;
    } cases[] = {
        {"../../testdata/orientation/orientation-1.jpg", 1},
        {"../../testdata/orientation/orientation-3.jpg", 3},
        {"../../testdata/orientation/orientation-6.jpg", 6},
        {"../../testdata/orientation/orientation-8.jpg", 8},
    };

    for (int i = 0; i < 4; i++) {
        size_t size;
        uint8_t* data = read_file(cases[i].filename, &size);
        assert(data != NULL);

        int orientation = nextimage_extract_exif_orientation(data, size);
        printf("  %s: orientation=%d (expected %d)\n",
               cases[i].filename, orientation, cases[i].expected);
        assert(orientation == cases[i].expected);

        free(data);
    }

    // Test with non-JPEG data
    {
        uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03};
        int orientation = nextimage_extract_exif_orientation(garbage, sizeof(garbage));
        assert(orientation == 0);
        printf("  Non-JPEG data: orientation=%d (expected 0)\n", orientation);
    }

    // Test with NULL
    {
        int orientation = nextimage_extract_exif_orientation(NULL, 0);
        assert(orientation == 0);
        printf("  NULL data: orientation=%d (expected 0)\n", orientation);
    }

    printf("  ✓ Orientation extraction tests passed\n");
}

// ========================================
// Test: Auto-orient with orientation 1 (no-op)
// ========================================
void test_auto_orient_1(void) {
    printf("\nTesting auto-orient for orientation=1 (no-op)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-1.jpg", &size);
    assert(data != NULL);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);
    assert(output.size > 0);

    // Output should be a copy of input (same size)
    assert(output.size == size);
    printf("  Input: %zu bytes, Output: %zu bytes\n", size, output.size);

    // Verify output has no rotation needed
    int out_orient = nextimage_extract_exif_orientation(output.data, output.size);
    printf("  Output orientation: %d\n", out_orient);
    assert(out_orient == 0 || out_orient == 1);

    // Decode and verify dimensions and pixel colors
    DecodedImage img = decode_jpeg(output.data, output.size);
    assert(img.pixels != NULL);
    printf("  Output dimensions: %dx%d\n", img.width, img.height);
    assert(img.width == 80);
    assert(img.height == 60);

    // Check quadrant colors (with JPEG lossy tolerance of 30)
    int tolerance = 30;
    // Top-left (10, 10) should be RED
    uint8_t* tl = img.pixels + (10 * img.width + 10) * 3;
    assert(color_matches(tl, 255, 0, 0, tolerance));
    printf("  Top-left pixel (10,10): R=%d G=%d B=%d (expected ~RED)\n", tl[0], tl[1], tl[2]);

    // Top-right (70, 10) should be GREEN
    uint8_t* tr = img.pixels + (10 * img.width + 70) * 3;
    assert(color_matches(tr, 0, 255, 0, tolerance));
    printf("  Top-right pixel (70,10): R=%d G=%d B=%d (expected ~GREEN)\n", tr[0], tr[1], tr[2]);

    // Bottom-left (10, 50) should be BLUE
    uint8_t* bl = img.pixels + (50 * img.width + 10) * 3;
    assert(color_matches(bl, 0, 0, 255, tolerance));
    printf("  Bottom-left pixel (10,50): R=%d G=%d B=%d (expected ~BLUE)\n", bl[0], bl[1], bl[2]);

    // Bottom-right (70, 50) should be WHITE
    uint8_t* br = img.pixels + (50 * img.width + 70) * 3;
    assert(color_matches(br, 255, 255, 255, tolerance));
    printf("  Bottom-right pixel (70,50): R=%d G=%d B=%d (expected ~WHITE)\n", br[0], br[1], br[2]);

    free(img.pixels);
    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ Auto-orient orientation=1 test passed\n");
}

// ========================================
// Test: Auto-orient with orientation 3 (180°)
// ========================================
void test_auto_orient_3(void) {
    printf("\nTesting auto-orient for orientation=3 (180° rotation)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-3.jpg", &size);
    assert(data != NULL);

    // Input should have orientation 3
    assert(nextimage_extract_exif_orientation(data, size) == 3);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);
    assert(output.size > 0);

    // Output should have no rotation needed
    int out_orient = nextimage_extract_exif_orientation(output.data, output.size);
    printf("  Output orientation: %d (should be 0 or 1)\n", out_orient);
    assert(out_orient == 0 || out_orient == 1);

    // Decode and verify
    DecodedImage img = decode_jpeg(output.data, output.size);
    assert(img.pixels != NULL);
    printf("  Output dimensions: %dx%d (expected 80x60)\n", img.width, img.height);
    assert(img.width == 80);
    assert(img.height == 60);

    // After 180° rotation, quadrants should be in canonical order
    int tolerance = 30;
    uint8_t* tl = img.pixels + (10 * img.width + 10) * 3;
    assert(color_matches(tl, 255, 0, 0, tolerance));
    printf("  Top-left: R=%d G=%d B=%d (expected ~RED)\n", tl[0], tl[1], tl[2]);

    uint8_t* tr = img.pixels + (10 * img.width + 70) * 3;
    assert(color_matches(tr, 0, 255, 0, tolerance));
    printf("  Top-right: R=%d G=%d B=%d (expected ~GREEN)\n", tr[0], tr[1], tr[2]);

    uint8_t* bl = img.pixels + (50 * img.width + 10) * 3;
    assert(color_matches(bl, 0, 0, 255, tolerance));
    printf("  Bottom-left: R=%d G=%d B=%d (expected ~BLUE)\n", bl[0], bl[1], bl[2]);

    uint8_t* br = img.pixels + (50 * img.width + 70) * 3;
    assert(color_matches(br, 255, 255, 255, tolerance));
    printf("  Bottom-right: R=%d G=%d B=%d (expected ~WHITE)\n", br[0], br[1], br[2]);

    free(img.pixels);
    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ Auto-orient orientation=3 test passed\n");
}

// ========================================
// Test: Auto-orient with orientation 6 (90° CW)
// ========================================
void test_auto_orient_6(void) {
    printf("\nTesting auto-orient for orientation=6 (90° CW rotation)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-6.jpg", &size);
    assert(data != NULL);

    // Input should have orientation 6
    assert(nextimage_extract_exif_orientation(data, size) == 6);

    // Input image is 60x80 (stored portrait)
    DecodedImage input_img = decode_jpeg(data, size);
    assert(input_img.pixels != NULL);
    printf("  Input dimensions: %dx%d\n", input_img.width, input_img.height);
    assert(input_img.width == 60);
    assert(input_img.height == 80);
    free(input_img.pixels);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);
    assert(output.size > 0);

    // Output should have no rotation needed
    int out_orient = nextimage_extract_exif_orientation(output.data, output.size);
    printf("  Output orientation: %d (should be 0 or 1)\n", out_orient);
    assert(out_orient == 0 || out_orient == 1);

    // After 90° CW rotation, 60x80 becomes 80x60
    DecodedImage img = decode_jpeg(output.data, output.size);
    assert(img.pixels != NULL);
    printf("  Output dimensions: %dx%d (expected 80x60)\n", img.width, img.height);
    assert(img.width == 80);
    assert(img.height == 60);

    // After rotation, quadrants should be in canonical order
    int tolerance = 30;
    uint8_t* tl = img.pixels + (10 * img.width + 10) * 3;
    assert(color_matches(tl, 255, 0, 0, tolerance));
    printf("  Top-left: R=%d G=%d B=%d (expected ~RED)\n", tl[0], tl[1], tl[2]);

    uint8_t* tr = img.pixels + (10 * img.width + 70) * 3;
    assert(color_matches(tr, 0, 255, 0, tolerance));
    printf("  Top-right: R=%d G=%d B=%d (expected ~GREEN)\n", tr[0], tr[1], tr[2]);

    uint8_t* bl = img.pixels + (50 * img.width + 10) * 3;
    assert(color_matches(bl, 0, 0, 255, tolerance));
    printf("  Bottom-left: R=%d G=%d B=%d (expected ~BLUE)\n", bl[0], bl[1], bl[2]);

    uint8_t* br = img.pixels + (50 * img.width + 70) * 3;
    assert(color_matches(br, 255, 255, 255, tolerance));
    printf("  Bottom-right: R=%d G=%d B=%d (expected ~WHITE)\n", br[0], br[1], br[2]);

    free(img.pixels);
    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ Auto-orient orientation=6 test passed\n");
}

// ========================================
// Test: Auto-orient with orientation 8 (270° CW)
// ========================================
void test_auto_orient_8(void) {
    printf("\nTesting auto-orient for orientation=8 (270° CW rotation)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-8.jpg", &size);
    assert(data != NULL);

    // Input should have orientation 8
    assert(nextimage_extract_exif_orientation(data, size) == 8);

    // Input image is 60x80 (stored portrait)
    DecodedImage input_img = decode_jpeg(data, size);
    assert(input_img.pixels != NULL);
    printf("  Input dimensions: %dx%d\n", input_img.width, input_img.height);
    assert(input_img.width == 60);
    assert(input_img.height == 80);
    free(input_img.pixels);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);
    assert(output.size > 0);

    // Output should have no rotation needed
    int out_orient = nextimage_extract_exif_orientation(output.data, output.size);
    printf("  Output orientation: %d (should be 0 or 1)\n", out_orient);
    assert(out_orient == 0 || out_orient == 1);

    // After 270° CW rotation, 60x80 becomes 80x60
    DecodedImage img = decode_jpeg(output.data, output.size);
    assert(img.pixels != NULL);
    printf("  Output dimensions: %dx%d (expected 80x60)\n", img.width, img.height);
    assert(img.width == 80);
    assert(img.height == 60);

    // After rotation, quadrants should be in canonical order
    int tolerance = 30;
    uint8_t* tl = img.pixels + (10 * img.width + 10) * 3;
    assert(color_matches(tl, 255, 0, 0, tolerance));
    printf("  Top-left: R=%d G=%d B=%d (expected ~RED)\n", tl[0], tl[1], tl[2]);

    uint8_t* tr = img.pixels + (10 * img.width + 70) * 3;
    assert(color_matches(tr, 0, 255, 0, tolerance));
    printf("  Top-right: R=%d G=%d B=%d (expected ~GREEN)\n", tr[0], tr[1], tr[2]);

    uint8_t* bl = img.pixels + (50 * img.width + 10) * 3;
    assert(color_matches(bl, 0, 0, 255, tolerance));
    printf("  Bottom-left: R=%d G=%d B=%d (expected ~BLUE)\n", bl[0], bl[1], bl[2]);

    uint8_t* br = img.pixels + (50 * img.width + 70) * 3;
    assert(color_matches(br, 255, 255, 255, tolerance));
    printf("  Bottom-right: R=%d G=%d B=%d (expected ~WHITE)\n", br[0], br[1], br[2]);

    free(img.pixels);
    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ Auto-orient orientation=8 test passed\n");
}

// ========================================
// Test: Auto-orient JPEG without Exif (no-op)
// ========================================
void test_auto_orient_no_exif(void) {
    printf("\nTesting auto-orient for JPEG without Exif (no-op)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/jpeg-source/small-128x128.jpg", &size);
    assert(data != NULL);

    int orientation = nextimage_extract_exif_orientation(data, size);
    printf("  Input orientation: %d (expected 0)\n", orientation);
    assert(orientation == 0);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);
    // Should be a copy of input
    assert(output.size == size);

    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ No-Exif auto-orient test passed\n");
}

// ========================================
// Test: Output JPEG is 4:4:4 subsampling
// ========================================
void test_output_subsampling(void) {
    printf("\nTesting output JPEG subsampling (4:4:4)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-3.jpg", &size);
    assert(data != NULL);

    NextImageBuffer output = {0};
    int ret = nextimage_jpeg_auto_orient(data, size, &output);
    assert(ret == 0);
    assert(output.data != NULL);

    // Decode output and check subsampling
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, output.data, (unsigned long)output.size);

    int header_ok = jpeg_read_header(&cinfo, TRUE);
    assert(header_ok == JPEG_HEADER_OK);

    // 4:4:4 means all components have h_samp_factor=1 and v_samp_factor=1
    printf("  Components: %d\n", cinfo.num_components);
    for (int i = 0; i < cinfo.num_components; i++) {
        printf("  Component %d: h_samp=%d, v_samp=%d\n",
               i, cinfo.comp_info[i].h_samp_factor, cinfo.comp_info[i].v_samp_factor);
        assert(cinfo.comp_info[i].h_samp_factor == 1);
        assert(cinfo.comp_info[i].v_samp_factor == 1);
    }

    jpeg_destroy_decompress(&cinfo);
    nextimage_free_buffer(&output);
    free(data);
    printf("  ✓ Output subsampling test passed\n");
}

// ========================================
// Helper: Decode WebP to get dimensions
// ========================================
static void get_webp_dimensions(const uint8_t* data, size_t size, int* width, int* height) {
    // WebP VP8/VP8L header: RIFF....WEBP
    // Use simple approach: look for VP8 header
    *width = 0;
    *height = 0;
    if (size < 30 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) return;

    // VP8 lossy: "VP8 " at offset 12
    if (data[12] == 'V' && data[13] == 'P' && data[14] == '8' && data[15] == ' ') {
        // VP8 bitstream starts at offset 20 (after chunk header)
        // Frame tag at offset 20: 3 bytes, then 3-byte start code 9D 01 2A
        size_t off = 20;
        // Skip frame tag (3 bytes)
        // Find start code 9D 01 2A
        for (size_t i = off; i + 5 < size && i < off + 10; i++) {
            if (data[i] == 0x9D && data[i+1] == 0x01 && data[i+2] == 0x2A) {
                *width = (data[i+3] | (data[i+4] << 8)) & 0x3FFF;
                *height = (data[i+5] | (data[i+6] << 8)) & 0x3FFF;
                return;
            }
        }
    }

    // VP8L lossless: "VP8L" at offset 12
    if (data[12] == 'V' && data[13] == 'P' && data[14] == '8' && data[15] == 'L') {
        // VP8L signature at offset 21: 0x2F
        size_t off = 21;
        if (off + 4 < size && data[off] == 0x2F) {
            uint32_t bits = data[off+1] | (data[off+2] << 8) | (data[off+3] << 16) | ((uint32_t)data[off+4] << 24);
            *width = (bits & 0x3FFF) + 1;
            *height = ((bits >> 14) & 0x3FFF) + 1;
            return;
        }
    }

    // VP8X extended: chunk at offset 12
    if (data[12] == 'V' && data[13] == 'P' && data[14] == '8' && data[15] == 'X') {
        // Canvas size at offset 24-29: width-1 (24 bits), height-1 (24 bits)
        if (size >= 30) {
            *width = (data[24] | (data[25] << 8) | (data[26] << 16)) + 1;
            *height = (data[27] | (data[28] << 8) | (data[29] << 16)) + 1;
        }
    }
}

// ========================================
// Test: Light API - Oriented JPEG -> WebP
// ========================================
void test_light_oriented_jpeg_to_webp(void) {
    printf("\nTesting Light API: oriented JPEG -> WebP...\n");

    struct {
        const char* filename;
        int orientation;
        int expected_w;
        int expected_h;
    } cases[] = {
        {"../../testdata/orientation/orientation-1.jpg", 1, 80, 60},
        {"../../testdata/orientation/orientation-3.jpg", 3, 80, 60},
        {"../../testdata/orientation/orientation-6.jpg", 6, 80, 60},
        {"../../testdata/orientation/orientation-8.jpg", 8, 80, 60},
    };

    for (int i = 0; i < 4; i++) {
        size_t size;
        uint8_t* data = read_file(cases[i].filename, &size);
        assert(data != NULL);

        NextImageLiteInput input = {0};
        input.data = data;
        input.size = size;
        input.quality = 75;
        input.min_quantizer = -1;
        input.max_quantizer = -1;

        NextImageLiteOutput output = {0};
        NextImageStatus status = nextimage_lite_legacy_to_webp(&input, &output);
        assert(status == NEXTIMAGE_OK);
        assert(output.data != NULL);
        assert(output.size > 0);
        assert(strcmp(output.mime_type, "image/webp") == 0);

        // Verify WebP dimensions (should be 80x60 after auto-orient)
        int webp_w = 0, webp_h = 0;
        get_webp_dimensions(output.data, output.size, &webp_w, &webp_h);
        printf("  orientation=%d: %zu bytes -> %zu bytes WebP (%dx%d, expected %dx%d)\n",
               cases[i].orientation, size, output.size,
               webp_w, webp_h, cases[i].expected_w, cases[i].expected_h);
        assert(webp_w == cases[i].expected_w);
        assert(webp_h == cases[i].expected_h);

        nextimage_lite_free(&output);
        free(data);
    }

    printf("  ✓ Light API oriented JPEG -> WebP test passed\n");
}

// ========================================
// Test: Light API - Oriented JPEG -> AVIF
// ========================================
void test_light_oriented_jpeg_to_avif(void) {
    printf("\nTesting Light API: oriented JPEG -> AVIF...\n");

    struct {
        const char* filename;
        int orientation;
    } cases[] = {
        {"../../testdata/orientation/orientation-1.jpg", 1},
        {"../../testdata/orientation/orientation-3.jpg", 3},
        {"../../testdata/orientation/orientation-6.jpg", 6},
        {"../../testdata/orientation/orientation-8.jpg", 8},
    };

    for (int i = 0; i < 4; i++) {
        size_t size;
        uint8_t* data = read_file(cases[i].filename, &size);
        assert(data != NULL);

        NextImageLiteInput input = {0};
        input.data = data;
        input.size = size;
        input.quality = 60;
        input.min_quantizer = -1;
        input.max_quantizer = -1;

        NextImageLiteOutput avif_output = {0};
        NextImageStatus status = nextimage_lite_legacy_to_avif(&input, &avif_output);
        assert(status == NEXTIMAGE_OK);
        assert(avif_output.data != NULL);
        assert(avif_output.size > 0);
        assert(strcmp(avif_output.mime_type, "image/avif") == 0);

        // Decode AVIF back to JPEG to verify dimensions
        NextImageLiteInput avif_input = {0};
        avif_input.data = avif_output.data;
        avif_input.size = avif_output.size;
        avif_input.quality = -1;
        avif_input.min_quantizer = -1;
        avif_input.max_quantizer = -1;

        NextImageLiteOutput jpeg_output = {0};
        status = nextimage_lite_avif_to_legacy(&avif_input, &jpeg_output);
        assert(status == NEXTIMAGE_OK);
        assert(jpeg_output.data != NULL);

        // Decode JPEG to verify dimensions
        DecodedImage img = decode_jpeg(jpeg_output.data, jpeg_output.size);
        assert(img.pixels != NULL);
        printf("  orientation=%d: %zu bytes -> %zu bytes AVIF -> %zu bytes JPEG (%dx%d, expected 80x60)\n",
               cases[i].orientation, size, avif_output.size, jpeg_output.size,
               img.width, img.height);
        assert(img.width == 80);
        assert(img.height == 60);

        // Verify pixel colors at quadrant centers (lossy tolerance of 50 for double-encode)
        int tolerance = 50;
        uint8_t* tl = img.pixels + (10 * img.width + 10) * 3;
        assert(color_matches(tl, 255, 0, 0, tolerance));

        uint8_t* tr = img.pixels + (10 * img.width + 70) * 3;
        assert(color_matches(tr, 0, 255, 0, tolerance));

        uint8_t* bl = img.pixels + (50 * img.width + 10) * 3;
        assert(color_matches(bl, 0, 0, 255, tolerance));

        uint8_t* br = img.pixels + (50 * img.width + 70) * 3;
        assert(color_matches(br, 255, 255, 255, tolerance));

        free(img.pixels);
        nextimage_lite_free(&jpeg_output);
        nextimage_lite_free(&avif_output);
        free(data);
    }

    printf("  ✓ Light API oriented JPEG -> AVIF test passed\n");
}

int main(void) {
    printf("=== Exif Orientation Tests ===\n");

    // Extraction tests
    test_extract_orientation();

    // Auto-orient tests
    test_auto_orient_1();
    test_auto_orient_3();
    test_auto_orient_6();
    test_auto_orient_8();
    test_auto_orient_no_exif();

    // Output quality tests
    test_output_subsampling();

    // Light API integration tests
    test_light_oriented_jpeg_to_webp();
    test_light_oriented_jpeg_to_avif();

    printf("\n=== All orientation tests passed! ===\n");
    return 0;
}
