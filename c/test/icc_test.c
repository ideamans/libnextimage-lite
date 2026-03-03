/**
 * ICC Profile Auto-Extraction Test
 *
 * Verifies that ICC profiles embedded in JPEG/PNG input images
 * are automatically extracted and preserved through AVIF encoding,
 * and that AVIF->JPEG/PNG decoding restores the ICC profile.
 */

#include "nextimage.h"
#include "nextimage_light.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

// ========================================
// Test: ICC extraction from JPEG
// ========================================
void test_extract_icc_from_jpeg(void) {
    printf("\nTesting ICC extraction from JPEG (sRGB)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/icc/srgb.jpg", &size);
    assert(data != NULL);
    printf("  ✓ Read JPEG file: %zu bytes\n", size);

    uint8_t* icc = NULL;
    size_t icc_size = 0;
    int ret = nextimage_extract_icc_from_jpeg(data, size, &icc, &icc_size);
    assert(ret == 0);
    assert(icc != NULL);
    assert(icc_size > 0);
    printf("  ✓ Extracted ICC profile: %zu bytes\n", icc_size);

    free(icc);
    free(data);
    printf("  ✓ JPEG ICC extraction test passed\n");
}

void test_extract_icc_from_jpeg_display_p3(void) {
    printf("\nTesting ICC extraction from JPEG (Display P3)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/icc/display-p3.jpg", &size);
    assert(data != NULL);
    printf("  ✓ Read JPEG file: %zu bytes\n", size);

    uint8_t* icc = NULL;
    size_t icc_size = 0;
    int ret = nextimage_extract_icc_from_jpeg(data, size, &icc, &icc_size);
    assert(ret == 0);
    assert(icc != NULL);
    assert(icc_size > 0);
    printf("  ✓ Extracted ICC profile: %zu bytes\n", icc_size);

    free(icc);
    free(data);
    printf("  ✓ JPEG Display P3 ICC extraction test passed\n");
}

void test_extract_icc_from_jpeg_no_icc(void) {
    printf("\nTesting ICC extraction from JPEG (no ICC)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/icc/no-icc.jpg", &size);
    assert(data != NULL);
    printf("  ✓ Read JPEG file: %zu bytes\n", size);

    uint8_t* icc = NULL;
    size_t icc_size = 0;
    int ret = nextimage_extract_icc_from_jpeg(data, size, &icc, &icc_size);
    assert(ret == 0);
    assert(icc == NULL);
    assert(icc_size == 0);
    printf("  ✓ No ICC profile found (expected)\n");

    free(data);
    printf("  ✓ JPEG no-ICC test passed\n");
}

// ========================================
// Test: ICC extraction from PNG
// ========================================
void test_extract_icc_from_png(void) {
    printf("\nTesting ICC extraction from PNG (sRGB)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/icc/srgb.png", &size);
    assert(data != NULL);
    printf("  ✓ Read PNG file: %zu bytes\n", size);

    uint8_t* icc = NULL;
    size_t icc_size = 0;
    int ret = nextimage_extract_icc_from_png(data, size, &icc, &icc_size);
    assert(ret == 0);
    assert(icc != NULL);
    assert(icc_size > 0);
    printf("  ✓ Extracted ICC profile: %zu bytes\n", icc_size);

    free(icc);
    free(data);
    printf("  ✓ PNG ICC extraction test passed\n");
}

void test_extract_icc_from_png_no_icc(void) {
    printf("\nTesting ICC extraction from PNG (no ICC)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/icc/no-icc.png", &size);
    assert(data != NULL);
    printf("  ✓ Read PNG file: %zu bytes\n", size);

    uint8_t* icc = NULL;
    size_t icc_size = 0;
    int ret = nextimage_extract_icc_from_png(data, size, &icc, &icc_size);
    assert(ret == 0);
    assert(icc == NULL);
    assert(icc_size == 0);
    printf("  ✓ No ICC profile found (expected)\n");

    free(data);
    printf("  ✓ PNG no-ICC test passed\n");
}

// ========================================
// Test: JPEG -> AVIF roundtrip preserves ICC
// ========================================
void test_jpeg_to_avif_icc_roundtrip(void) {
    printf("\nTesting JPEG(sRGB ICC) -> AVIF -> JPEG roundtrip...\n");

    // Read JPEG with sRGB ICC
    size_t jpeg_size;
    uint8_t* jpeg_data = read_file("../../testdata/icc/srgb.jpg", &jpeg_size);
    assert(jpeg_data != NULL);

    // Extract original ICC for comparison
    uint8_t* original_icc = NULL;
    size_t original_icc_size = 0;
    nextimage_extract_icc_from_jpeg(jpeg_data, jpeg_size, &original_icc, &original_icc_size);
    assert(original_icc != NULL && original_icc_size > 0);
    printf("  ✓ Original JPEG ICC: %zu bytes\n", original_icc_size);

    // JPEG -> AVIF (via light API)
    NextImageLightInput input = {0};
    input.data = jpeg_data;
    input.size = jpeg_size;
    input.quality = 60;
    input.min_quantizer = -1;
    input.max_quantizer = -1;

    NextImageLightOutput avif_output = {0};
    NextImageStatus status = nextimage_light_legacy_to_avif(&input, &avif_output);
    assert(status == NEXTIMAGE_OK);
    assert(avif_output.size > 0);
    printf("  ✓ Encoded to AVIF: %zu bytes\n", avif_output.size);

    // AVIF -> JPEG (via light API)
    NextImageLightInput avif_input = {0};
    avif_input.data = avif_output.data;
    avif_input.size = avif_output.size;
    avif_input.quality = -1;
    avif_input.min_quantizer = -1;
    avif_input.max_quantizer = -1;

    NextImageLightOutput jpeg_output = {0};
    status = nextimage_light_avif_to_legacy(&avif_input, &jpeg_output);
    assert(status == NEXTIMAGE_OK);
    assert(jpeg_output.size > 0);
    printf("  ✓ Decoded to JPEG: %zu bytes\n", jpeg_output.size);

    // Verify output JPEG has ICC profile
    uint8_t* output_icc = NULL;
    size_t output_icc_size = 0;
    nextimage_extract_icc_from_jpeg(jpeg_output.data, jpeg_output.size, &output_icc, &output_icc_size);
    assert(output_icc != NULL);
    assert(output_icc_size > 0);
    printf("  ✓ Output JPEG ICC: %zu bytes\n", output_icc_size);

    // ICC sizes should match (same profile roundtripped)
    assert(original_icc_size == output_icc_size);
    assert(memcmp(original_icc, output_icc, original_icc_size) == 0);
    printf("  ✓ ICC profiles match (binary identical)\n");

    free(original_icc);
    free(output_icc);
    free(jpeg_data);
    nextimage_light_free(&avif_output);
    nextimage_light_free(&jpeg_output);
    printf("  ✓ JPEG->AVIF->JPEG ICC roundtrip test passed\n");
}

// ========================================
// Test: PNG -> AVIF roundtrip preserves ICC
// ========================================
void test_png_to_avif_icc_roundtrip(void) {
    printf("\nTesting PNG(sRGB ICC) -> AVIF -> PNG roundtrip...\n");

    // Read PNG with sRGB ICC
    size_t png_size;
    uint8_t* png_data = read_file("../../testdata/icc/srgb.png", &png_size);
    assert(png_data != NULL);

    // Extract original ICC for comparison
    uint8_t* original_icc = NULL;
    size_t original_icc_size = 0;
    nextimage_extract_icc_from_png(png_data, png_size, &original_icc, &original_icc_size);
    assert(original_icc != NULL && original_icc_size > 0);
    printf("  ✓ Original PNG ICC: %zu bytes\n", original_icc_size);

    // PNG -> AVIF (via light API, lossless)
    NextImageLightInput input = {0};
    input.data = png_data;
    input.size = png_size;
    input.quality = -1;
    input.min_quantizer = -1;
    input.max_quantizer = -1;

    NextImageLightOutput avif_output = {0};
    NextImageStatus status = nextimage_light_legacy_to_avif(&input, &avif_output);
    assert(status == NEXTIMAGE_OK);
    assert(avif_output.size > 0);
    printf("  ✓ Encoded to AVIF: %zu bytes\n", avif_output.size);

    // AVIF -> PNG (via light API, lossless -> PNG)
    NextImageLightInput avif_input = {0};
    avif_input.data = avif_output.data;
    avif_input.size = avif_output.size;
    avif_input.quality = -1;
    avif_input.min_quantizer = -1;
    avif_input.max_quantizer = -1;

    NextImageLightOutput png_output = {0};
    status = nextimage_light_avif_to_legacy(&avif_input, &png_output);
    assert(status == NEXTIMAGE_OK);
    assert(png_output.size > 0);
    printf("  ✓ Decoded to PNG: %zu bytes\n", png_output.size);

    // Verify output PNG has ICC profile
    uint8_t* output_icc = NULL;
    size_t output_icc_size = 0;
    nextimage_extract_icc_from_png(png_output.data, png_output.size, &output_icc, &output_icc_size);
    assert(output_icc != NULL);
    assert(output_icc_size > 0);
    printf("  ✓ Output PNG ICC: %zu bytes\n", output_icc_size);

    // ICC sizes should match
    assert(original_icc_size == output_icc_size);
    assert(memcmp(original_icc, output_icc, original_icc_size) == 0);
    printf("  ✓ ICC profiles match (binary identical)\n");

    free(original_icc);
    free(output_icc);
    free(png_data);
    nextimage_light_free(&avif_output);
    nextimage_light_free(&png_output);
    printf("  ✓ PNG->AVIF->PNG ICC roundtrip test passed\n");
}

// ========================================
// Test: JPEG without ICC -> AVIF -> JPEG (no ICC expected)
// ========================================
void test_no_icc_jpeg_roundtrip(void) {
    printf("\nTesting JPEG(no ICC) -> AVIF -> JPEG roundtrip...\n");

    size_t jpeg_size;
    uint8_t* jpeg_data = read_file("../../testdata/icc/no-icc.jpg", &jpeg_size);
    assert(jpeg_data != NULL);

    // JPEG -> AVIF
    NextImageLightInput input = {0};
    input.data = jpeg_data;
    input.size = jpeg_size;
    input.quality = 60;
    input.min_quantizer = -1;
    input.max_quantizer = -1;

    NextImageLightOutput avif_output = {0};
    NextImageStatus status = nextimage_light_legacy_to_avif(&input, &avif_output);
    assert(status == NEXTIMAGE_OK);
    printf("  ✓ Encoded to AVIF: %zu bytes\n", avif_output.size);

    // AVIF -> JPEG
    NextImageLightInput avif_input = {0};
    avif_input.data = avif_output.data;
    avif_input.size = avif_output.size;
    avif_input.quality = -1;
    avif_input.min_quantizer = -1;
    avif_input.max_quantizer = -1;

    NextImageLightOutput jpeg_output = {0};
    status = nextimage_light_avif_to_legacy(&avif_input, &jpeg_output);
    assert(status == NEXTIMAGE_OK);
    printf("  ✓ Decoded to JPEG: %zu bytes\n", jpeg_output.size);

    // Verify output JPEG has NO ICC profile
    uint8_t* output_icc = NULL;
    size_t output_icc_size = 0;
    nextimage_extract_icc_from_jpeg(jpeg_output.data, jpeg_output.size, &output_icc, &output_icc_size);
    assert(output_icc == NULL);
    assert(output_icc_size == 0);
    printf("  ✓ No ICC in output (expected)\n");

    free(jpeg_data);
    nextimage_light_free(&avif_output);
    nextimage_light_free(&jpeg_output);
    printf("  ✓ No-ICC JPEG roundtrip test passed\n");
}

int main(void) {
    printf("=== ICC Profile Auto-Extraction Tests ===\n");

    // Extraction tests
    test_extract_icc_from_jpeg();
    test_extract_icc_from_jpeg_display_p3();
    test_extract_icc_from_jpeg_no_icc();
    test_extract_icc_from_png();
    test_extract_icc_from_png_no_icc();

    // Roundtrip tests
    test_jpeg_to_avif_icc_roundtrip();
    test_png_to_avif_icc_roundtrip();
    test_no_icc_jpeg_roundtrip();

    printf("\n=== All ICC tests passed! ===\n");
    return 0;
}
