/**
 * avifenc CLI互換性テスト
 *
 * avifenc_run_command() の出力が bin/avifenc CLI と
 * バイナリレベルで完全に一致することを検証する。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nextimage.h"
#include "nextimage/avifenc.h"

static const char* BIN_DIR = "../../bin";
static const char* TESTDATA_DIR = "../../testdata";
static const char* TEMP_DIR = "../../build-test-compat";

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

static int file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void ensure_dir(const char* path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
    system(cmd);
}

static uint8_t* read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* data = (uint8_t*)malloc(size);
    if (!data) { fclose(f); return NULL; }
    size_t read = fread(data, 1, size, f);
    fclose(f);
    if ((long)read != size) { free(data); return NULL; }
    *out_size = (size_t)size;
    return data;
}

static uint8_t* run_avifenc_cli(const char* input_file, const char* args,
                                 const char* output_file, size_t* out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/avifenc %s '%s' '%s' 2>/dev/null",
             BIN_DIR, args, input_file, output_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "    CLI command failed: %s\n", cmd);
        return NULL;
    }
    return read_file(output_file, out_size);
}

static uint8_t* run_avifenc_lib(const uint8_t* input_data, size_t input_size,
                                 const AVIFEncOptions* opts, size_t* out_size) {
    AVIFEncCommand* cmd = avifenc_new_command(opts);
    if (!cmd) return NULL;

    NextImageBuffer output = {0};
    NextImageStatus status = avifenc_run_command(cmd, input_data, input_size, &output);
    avifenc_free_command(cmd);

    if (status != NEXTIMAGE_OK) {
        fprintf(stderr, "    Library encode failed: status=%d\n", status);
        return NULL;
    }

    uint8_t* result = (uint8_t*)malloc(output.size);
    if (!result) { nextimage_free_buffer(&output); return NULL; }
    memcpy(result, output.data, output.size);
    *out_size = output.size;
    nextimage_free_buffer(&output);
    return result;
}

static int compare_outputs(const char* test_name,
                           const uint8_t* cli_data, size_t cli_size,
                           const uint8_t* lib_data, size_t lib_size) {
    tests_run++;
    if (cli_size == lib_size && memcmp(cli_data, lib_data, cli_size) == 0) {
        printf("  [PASS] %s: Binary exact match (%zu bytes)\n", test_name, cli_size);
        tests_passed++;
        return 1;
    }
    long diff = (long)lib_size - (long)cli_size;
    if (diff < 0) diff = -diff;
    double pct = cli_size > 0 ? (double)diff * 100.0 / (double)cli_size : 0;
    printf("  [FAIL] %s: Binary mismatch (cli=%zu, lib=%zu, diff=%ld bytes, %.2f%%)\n",
           test_name, cli_size, lib_size, diff, pct);
    tests_failed++;
    return 0;
}

/* ============================================
 * Tests: Quality settings (PNG input)
 * ============================================ */
static void test_avifenc_quality_png(void) {
    printf("\n=== avifenc Quality Tests (PNG input) ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/sizes/medium-512x512.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    struct {
        const char* name;
        int quality;
        const char* args;
    } cases[] = {
        {"png-q30", 30, "-q 30"},
        {"png-q50", 50, "-q 50"},
        {"png-q75", 75, "-q 75"},
        {"png-q90", 90, "-q 90"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/avifenc-cmd/%s.avif", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_avifenc_cli(input_path, cases[i].args, output_path, &cli_size);

        AVIFEncOptions* opts = avifenc_create_default_options();
        opts->quality = cases[i].quality;
        size_t lib_size;
        uint8_t* lib_output = run_avifenc_lib(input_data, input_size, opts, &lib_size);
        avifenc_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: encode failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * Tests: Quality settings (JPEG input)
 * ============================================ */
static void test_avifenc_quality_jpeg(void) {
    printf("\n=== avifenc Quality Tests (JPEG input) ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/jpeg-source/medium-512x512.jpg", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    struct {
        const char* name;
        int quality;
        const char* args;
    } cases[] = {
        {"jpeg-q50", 50, "-q 50"},
        {"jpeg-q75", 75, "-q 75"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/avifenc-cmd/%s.avif", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_avifenc_cli(input_path, cases[i].args, output_path, &cli_size);

        AVIFEncOptions* opts = avifenc_create_default_options();
        opts->quality = cases[i].quality;
        size_t lib_size;
        uint8_t* lib_output = run_avifenc_lib(input_data, input_size, opts, &lib_size);
        avifenc_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: encode failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * Tests: Lossless
 * ============================================ */
static void test_avifenc_lossless(void) {
    printf("\n=== avifenc Lossless Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"lossless-medium", "source/sizes/medium-512x512.png"},
        {"lossless-small",  "source/sizes/small-128x128.png"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char input_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", TESTDATA_DIR, cases[i].file);

        size_t input_size;
        uint8_t* input_data = read_file(input_path, &input_size);
        if (!input_data) {
            printf("  [SKIP] Cannot read: %s\n", input_path);
            tests_skipped++;
            continue;
        }

        /* CLI: avifenc -l (lossless) */
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/avifenc-cmd/%s.avif", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_avifenc_cli(input_path, "-l", output_path, &cli_size);

        /* Library: lossless settings */
        AVIFEncOptions* opts = avifenc_create_default_options();
        opts->quality = 100;
        opts->min_quantizer = 0;
        opts->max_quantizer = 0;
        opts->min_quantizer_alpha = 0;
        opts->max_quantizer_alpha = 0;
        opts->yuv_format = 0; /* 444 */
        opts->matrix_coefficients = 0; /* identity */
        opts->speed = 0;
        size_t lib_size;
        uint8_t* lib_output = run_avifenc_lib(input_data, input_size, opts, &lib_size);
        avifenc_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: encode failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

/* ============================================
 * Tests: Speed settings
 * ============================================ */
static void test_avifenc_speed(void) {
    printf("\n=== avifenc Speed Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/sizes/small-128x128.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    struct {
        const char* name;
        int speed;
        const char* args;
    } cases[] = {
        {"speed-0",  0,  "-q 75 -s 0"},
        {"speed-6",  6,  "-q 75 -s 6"},
        {"speed-10", 10, "-q 75 -s 10"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/avifenc-cmd/%s.avif", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_avifenc_cli(input_path, cases[i].args, output_path, &cli_size);

        AVIFEncOptions* opts = avifenc_create_default_options();
        opts->quality = 75;
        opts->speed = cases[i].speed;
        size_t lib_size;
        uint8_t* lib_output = run_avifenc_lib(input_data, input_size, opts, &lib_size);
        avifenc_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: encode failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * Tests: Alpha (PNG with alpha channel)
 * ============================================ */
static void test_avifenc_alpha(void) {
    printf("\n=== avifenc Alpha Tests ===\n");

    struct {
        const char* name;
        const char* file;
        int quality;
        const char* args;
    } cases[] = {
        {"alpha-gradient-q75", "source/alpha/alpha-gradient.png", 75, "-q 75"},
        {"alpha-complex-q75",  "source/alpha/alpha-complex.png",  75, "-q 75"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char input_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", TESTDATA_DIR, cases[i].file);

        size_t input_size;
        uint8_t* input_data = read_file(input_path, &input_size);
        if (!input_data) {
            printf("  [SKIP] Cannot read: %s\n", input_path);
            tests_skipped++;
            continue;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/avifenc-cmd/%s.avif", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_avifenc_cli(input_path, cases[i].args, output_path, &cli_size);

        AVIFEncOptions* opts = avifenc_create_default_options();
        opts->quality = cases[i].quality;
        size_t lib_size;
        uint8_t* lib_output = run_avifenc_lib(input_data, input_size, opts, &lib_size);
        avifenc_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: encode failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

int main(void) {
    char avifenc_path[512];
    snprintf(avifenc_path, sizeof(avifenc_path), "%s/avifenc", BIN_DIR);
    if (!file_exists(avifenc_path)) {
        printf("SKIP: bin/avifenc not found. Run scripts/build-cli-tools.sh first.\n");
        return 0;
    }

    char cmd_dir[512];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/avifenc-cmd", TEMP_DIR);
    ensure_dir(cmd_dir);

    printf("avifenc CLI Binary Exact Match Compatibility Tests\n");
    printf("==================================================\n");

    test_avifenc_quality_png();
    test_avifenc_quality_jpeg();
    test_avifenc_lossless();
    test_avifenc_speed();
    test_avifenc_alpha();

    printf("\n==================================================\n");
    printf("Results: %d tests, %d passed, %d failed, %d skipped\n",
           tests_run, tests_passed, tests_failed, tests_skipped);

    return tests_failed > 0 ? 1 : 0;
}
