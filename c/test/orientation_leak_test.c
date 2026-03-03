/**
 * Exif Orientation Memory Leak Test
 *
 * Verifies that orientation extraction, auto-orient, and Lite API
 * conversion of oriented JPEGs have no memory leaks.
 *
 * Uses the NEXTIMAGE_DEBUG allocation counter to track allocations.
 * Each test records the counter before the operation, performs the
 * operation, frees all output, and asserts the counter returned to
 * its original value.
 *
 * Requires debug build: cmake -DCMAKE_BUILD_TYPE=Debug
 */

#include "nextimage.h"
#include "nextimage_lite.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef NEXTIMAGE_DEBUG

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
    size_t read_bytes = fread(data, 1, *size, f);
    fclose(f);
    if (read_bytes != *size) { free(data); return NULL; }
    return data;
}

// ========================================
// Test: nextimage_extract_exif_orientation has no leaks
// ========================================
void test_extract_orientation_no_leak(void) {
    printf("Testing extract_exif_orientation memory (no allocation expected)...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-6.jpg", &size);
    assert(data != NULL);

    int64_t before = nextimage_allocation_counter();

    // Call orientation extraction multiple times
    for (int i = 0; i < 100; i++) {
        int orient = nextimage_extract_exif_orientation(data, size);
        assert(orient == 6);
        (void)orient;
    }

    int64_t after = nextimage_allocation_counter();
    assert(before == after);
    printf("  Counter: before=%lld, after=%lld (delta=0)\n",
           (long long)before, (long long)after);

    free(data);
    printf("  ✓ No leaks in extract_exif_orientation\n");
}

// ========================================
// Test: nextimage_jpeg_auto_orient no-op path (orientation=1)
// ========================================
void test_auto_orient_noop_no_leak(void) {
    printf("Testing auto_orient no-op path (orientation=1) memory...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/orientation/orientation-1.jpg", &size);
    assert(data != NULL);

    int64_t before = nextimage_allocation_counter();

    for (int i = 0; i < 10; i++) {
        NextImageBuffer output = {0};
        int ret = nextimage_jpeg_auto_orient(data, size, &output);
        assert(ret == 0);
        assert(output.data != NULL);
        nextimage_free_buffer(&output);
    }

    int64_t after = nextimage_allocation_counter();
    assert(before == after);
    printf("  Counter: before=%lld, after=%lld (delta=0)\n",
           (long long)before, (long long)after);

    free(data);
    printf("  ✓ No leaks in auto_orient (no-op path)\n");
}

// ========================================
// Test: nextimage_jpeg_auto_orient rotation path (orientation=3, 6, 8)
// ========================================
void test_auto_orient_rotation_no_leak(void) {
    printf("Testing auto_orient rotation path memory...\n");

    const char* files[] = {
        "../../testdata/orientation/orientation-3.jpg",
        "../../testdata/orientation/orientation-6.jpg",
        "../../testdata/orientation/orientation-8.jpg",
    };

    for (int f = 0; f < 3; f++) {
        size_t size;
        uint8_t* data = read_file(files[f], &size);
        assert(data != NULL);

        int64_t before = nextimage_allocation_counter();

        for (int i = 0; i < 10; i++) {
            NextImageBuffer output = {0};
            int ret = nextimage_jpeg_auto_orient(data, size, &output);
            assert(ret == 0);
            assert(output.data != NULL);
            nextimage_free_buffer(&output);
        }

        int64_t after = nextimage_allocation_counter();
        assert(before == after);
        printf("  %s: counter before=%lld, after=%lld (delta=0)\n",
               files[f], (long long)before, (long long)after);

        free(data);
    }

    printf("  ✓ No leaks in auto_orient (rotation paths)\n");
}

// ========================================
// Test: nextimage_jpeg_auto_orient with no-Exif JPEG
// ========================================
void test_auto_orient_no_exif_no_leak(void) {
    printf("Testing auto_orient with no-Exif JPEG memory...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/jpeg-source/small-128x128.jpg", &size);
    assert(data != NULL);

    int64_t before = nextimage_allocation_counter();

    for (int i = 0; i < 10; i++) {
        NextImageBuffer output = {0};
        int ret = nextimage_jpeg_auto_orient(data, size, &output);
        assert(ret == 0);
        assert(output.data != NULL);
        nextimage_free_buffer(&output);
    }

    int64_t after = nextimage_allocation_counter();
    assert(before == after);
    printf("  Counter: before=%lld, after=%lld (delta=0)\n",
           (long long)before, (long long)after);

    free(data);
    printf("  ✓ No leaks in auto_orient (no-Exif)\n");
}

// ========================================
// Test: Lite API legacy_to_webp with oriented JPEGs
// ========================================
void test_light_webp_oriented_no_leak(void) {
    printf("Testing Lite API legacy_to_webp with oriented JPEGs memory...\n");

    const char* files[] = {
        "../../testdata/orientation/orientation-1.jpg",
        "../../testdata/orientation/orientation-3.jpg",
        "../../testdata/orientation/orientation-6.jpg",
        "../../testdata/orientation/orientation-8.jpg",
    };

    for (int f = 0; f < 4; f++) {
        size_t size;
        uint8_t* data = read_file(files[f], &size);
        assert(data != NULL);

        int64_t before = nextimage_allocation_counter();

        for (int i = 0; i < 5; i++) {
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
            nextimage_lite_free(&output);
        }

        int64_t after = nextimage_allocation_counter();
        assert(before == after);
        printf("  %s: counter before=%lld, after=%lld (delta=0)\n",
               files[f], (long long)before, (long long)after);

        free(data);
    }

    printf("  ✓ No leaks in legacy_to_webp (oriented JPEGs)\n");
}

// ========================================
// Test: Lite API legacy_to_avif with oriented JPEGs
// ========================================
void test_light_avif_oriented_no_leak(void) {
    printf("Testing Lite API legacy_to_avif with oriented JPEGs memory...\n");

    const char* files[] = {
        "../../testdata/orientation/orientation-1.jpg",
        "../../testdata/orientation/orientation-3.jpg",
        "../../testdata/orientation/orientation-6.jpg",
        "../../testdata/orientation/orientation-8.jpg",
    };

    for (int f = 0; f < 4; f++) {
        size_t size;
        uint8_t* data = read_file(files[f], &size);
        assert(data != NULL);

        int64_t before = nextimage_allocation_counter();

        for (int i = 0; i < 5; i++) {
            NextImageLiteInput input = {0};
            input.data = data;
            input.size = size;
            input.quality = 60;
            input.min_quantizer = -1;
            input.max_quantizer = -1;

            NextImageLiteOutput output = {0};
            NextImageStatus status = nextimage_lite_legacy_to_avif(&input, &output);
            assert(status == NEXTIMAGE_OK);
            assert(output.data != NULL);
            nextimage_lite_free(&output);
        }

        int64_t after = nextimage_allocation_counter();
        assert(before == after);
        printf("  %s: counter before=%lld, after=%lld (delta=0)\n",
               files[f], (long long)before, (long long)after);

        free(data);
    }

    printf("  ✓ No leaks in legacy_to_avif (oriented JPEGs)\n");
}

// ========================================
// Test: Lite API webp_to_legacy (roundtrip: oriented JPEG -> WebP -> JPEG)
// ========================================
void test_light_webp_to_legacy_no_leak(void) {
    printf("Testing Lite API webp_to_legacy (oriented JPEG -> WebP -> JPEG) memory...\n");

    const char* files[] = {
        "../../testdata/orientation/orientation-1.jpg",
        "../../testdata/orientation/orientation-3.jpg",
        "../../testdata/orientation/orientation-6.jpg",
        "../../testdata/orientation/orientation-8.jpg",
    };

    for (int f = 0; f < 4; f++) {
        size_t size;
        uint8_t* data = read_file(files[f], &size);
        assert(data != NULL);

        int64_t before = nextimage_allocation_counter();

        for (int i = 0; i < 5; i++) {
            // JPEG -> WebP
            NextImageLiteInput input = {0};
            input.data = data;
            input.size = size;
            input.quality = 75;
            input.min_quantizer = -1;
            input.max_quantizer = -1;

            NextImageLiteOutput webp_output = {0};
            NextImageStatus status = nextimage_lite_legacy_to_webp(&input, &webp_output);
            assert(status == NEXTIMAGE_OK);

            // WebP -> JPEG (lossy WebP -> JPEG)
            NextImageLiteInput webp_input = {0};
            webp_input.data = webp_output.data;
            webp_input.size = webp_output.size;
            webp_input.quality = -1;
            webp_input.min_quantizer = -1;
            webp_input.max_quantizer = -1;

            NextImageLiteOutput jpeg_output = {0};
            status = nextimage_lite_webp_to_legacy(&webp_input, &jpeg_output);
            assert(status == NEXTIMAGE_OK);

            nextimage_lite_free(&jpeg_output);
            nextimage_lite_free(&webp_output);
        }

        int64_t after = nextimage_allocation_counter();
        assert(before == after);
        printf("  %s: counter before=%lld, after=%lld (delta=0)\n",
               files[f], (long long)before, (long long)after);

        free(data);
    }

    printf("  ✓ No leaks in webp_to_legacy (oriented JPEG roundtrip)\n");
}

// ========================================
// Test: Lite API avif_to_legacy (roundtrip: oriented JPEG -> AVIF -> JPEG)
// ========================================
void test_light_avif_to_legacy_no_leak(void) {
    printf("Testing Lite API avif_to_legacy (oriented JPEG -> AVIF -> JPEG) memory...\n");

    const char* files[] = {
        "../../testdata/orientation/orientation-1.jpg",
        "../../testdata/orientation/orientation-3.jpg",
        "../../testdata/orientation/orientation-6.jpg",
        "../../testdata/orientation/orientation-8.jpg",
    };

    for (int f = 0; f < 4; f++) {
        size_t size;
        uint8_t* data = read_file(files[f], &size);
        assert(data != NULL);

        int64_t before = nextimage_allocation_counter();

        for (int i = 0; i < 5; i++) {
            // JPEG -> AVIF
            NextImageLiteInput input = {0};
            input.data = data;
            input.size = size;
            input.quality = 60;
            input.min_quantizer = -1;
            input.max_quantizer = -1;

            NextImageLiteOutput avif_output = {0};
            NextImageStatus status = nextimage_lite_legacy_to_avif(&input, &avif_output);
            assert(status == NEXTIMAGE_OK);

            // AVIF -> JPEG
            NextImageLiteInput avif_input = {0};
            avif_input.data = avif_output.data;
            avif_input.size = avif_output.size;
            avif_input.quality = -1;
            avif_input.min_quantizer = -1;
            avif_input.max_quantizer = -1;

            NextImageLiteOutput jpeg_output = {0};
            status = nextimage_lite_avif_to_legacy(&avif_input, &jpeg_output);
            assert(status == NEXTIMAGE_OK);

            nextimage_lite_free(&jpeg_output);
            nextimage_lite_free(&avif_output);
        }

        int64_t after = nextimage_allocation_counter();
        assert(before == after);
        printf("  %s: counter before=%lld, after=%lld (delta=0)\n",
               files[f], (long long)before, (long long)after);

        free(data);
    }

    printf("  ✓ No leaks in avif_to_legacy (oriented JPEG roundtrip)\n");
}

// ========================================
// Test: webp_to_legacy with lossless WebP -> PNG path
// ========================================
void test_light_webp_to_legacy_lossless_no_leak(void) {
    printf("Testing Lite API webp_to_legacy (PNG -> lossless WebP -> PNG) memory...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/png-source/small-128x128.png", &size);
    assert(data != NULL);

    int64_t before = nextimage_allocation_counter();

    for (int i = 0; i < 5; i++) {
        // PNG -> lossless WebP
        NextImageLiteInput input = {0};
        input.data = data;
        input.size = size;
        input.quality = -1;
        input.min_quantizer = -1;
        input.max_quantizer = -1;

        NextImageLiteOutput webp_output = {0};
        NextImageStatus status = nextimage_lite_legacy_to_webp(&input, &webp_output);
        assert(status == NEXTIMAGE_OK);

        // lossless WebP -> PNG
        NextImageLiteInput webp_input = {0};
        webp_input.data = webp_output.data;
        webp_input.size = webp_output.size;
        webp_input.quality = -1;
        webp_input.min_quantizer = -1;
        webp_input.max_quantizer = -1;

        NextImageLiteOutput png_output = {0};
        status = nextimage_lite_webp_to_legacy(&webp_input, &png_output);
        assert(status == NEXTIMAGE_OK);

        nextimage_lite_free(&png_output);
        nextimage_lite_free(&webp_output);
    }

    int64_t after = nextimage_allocation_counter();
    assert(before == after);
    printf("  Counter: before=%lld, after=%lld (delta=0)\n",
           (long long)before, (long long)after);

    free(data);
    printf("  ✓ No leaks in webp_to_legacy (lossless PNG roundtrip)\n");
}

// ========================================
// Test: avif_to_legacy with lossless AVIF -> PNG path
// ========================================
void test_light_avif_to_legacy_lossless_no_leak(void) {
    printf("Testing Lite API avif_to_legacy (PNG -> lossless AVIF -> PNG) memory...\n");

    size_t size;
    uint8_t* data = read_file("../../testdata/png-source/small-128x128.png", &size);
    assert(data != NULL);

    int64_t before = nextimage_allocation_counter();

    for (int i = 0; i < 3; i++) {
        // PNG -> lossless AVIF
        NextImageLiteInput input = {0};
        input.data = data;
        input.size = size;
        input.quality = -1;
        input.min_quantizer = -1;
        input.max_quantizer = -1;

        NextImageLiteOutput avif_output = {0};
        NextImageStatus status = nextimage_lite_legacy_to_avif(&input, &avif_output);
        assert(status == NEXTIMAGE_OK);

        // lossless AVIF -> PNG
        NextImageLiteInput avif_input = {0};
        avif_input.data = avif_output.data;
        avif_input.size = avif_output.size;
        avif_input.quality = -1;
        avif_input.min_quantizer = -1;
        avif_input.max_quantizer = -1;

        NextImageLiteOutput png_output = {0};
        status = nextimage_lite_avif_to_legacy(&avif_input, &png_output);
        assert(status == NEXTIMAGE_OK);

        nextimage_lite_free(&png_output);
        nextimage_lite_free(&avif_output);
    }

    int64_t after = nextimage_allocation_counter();
    assert(before == after);
    printf("  Counter: before=%lld, after=%lld (delta=0)\n",
           (long long)before, (long long)after);

    free(data);
    printf("  ✓ No leaks in avif_to_legacy (lossless PNG roundtrip)\n");
}

int main(void) {
    printf("=== Exif Orientation Memory Leak Tests (Debug Build) ===\n\n");

    int64_t initial = nextimage_allocation_counter();
    printf("Initial allocation counter: %lld\n", (long long)initial);

    // Utility function leak tests
    test_extract_orientation_no_leak();
    test_auto_orient_noop_no_leak();
    test_auto_orient_rotation_no_leak();
    test_auto_orient_no_exif_no_leak();

    // Lite API integration leak tests
    test_light_webp_oriented_no_leak();
    test_light_avif_oriented_no_leak();

    // Reverse direction (webp_to_legacy, avif_to_legacy) leak tests
    test_light_webp_to_legacy_no_leak();
    test_light_avif_to_legacy_no_leak();
    test_light_webp_to_legacy_lossless_no_leak();
    test_light_avif_to_legacy_lossless_no_leak();

    int64_t final_count = nextimage_allocation_counter();
    printf("\nFinal allocation counter: %lld (initial was %lld)\n",
           (long long)final_count, (long long)initial);
    assert(final_count == initial);

    printf("\n=== All orientation memory leak tests passed! ===\n");
    return 0;
}

#else

int main(void) {
    printf("=== Exif Orientation Memory Leak Tests ===\n\n");
    printf("NEXTIMAGE_DEBUG not defined - allocation counter not available.\n");
    printf("Build with: cmake -DCMAKE_BUILD_TYPE=Debug\n");
    return 0;
}

#endif
