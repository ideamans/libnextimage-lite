/**
 * dwebp CLI互換性テスト
 *
 * dwebp_run_command() のデコード結果が bin/dwebp CLI と
 * バイナリレベルで完全に一致することを検証する。
 *
 * 同じlibpng/libjpegを同じ設定で使用しているため、
 * PNG出力のバイナリ完全一致が期待できる。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nextimage.h"
#include "nextimage/dwebp.h"

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

/**
 * Run bin/dwebp CLI and read the output file as raw bytes.
 */
static uint8_t* run_dwebp_cli(const char* webp_file, const char* args,
                               const char* output_file, size_t* out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/dwebp %s '%s' -o '%s' 2>/dev/null",
             BIN_DIR, args, webp_file, output_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "    CLI command failed: %s\n", cmd);
        return NULL;
    }

    return read_file(output_file, out_size);
}

/**
 * Run library dwebp_run_command, return output buffer bytes.
 */
static uint8_t* run_dwebp_lib(const uint8_t* webp_data, size_t webp_size,
                               const DWebPOptions* opts, size_t* out_size) {
    DWebPCommand* cmd = dwebp_new_command(opts);
    if (!cmd) return NULL;

    NextImageBuffer output = {0};
    NextImageStatus status = dwebp_run_command(cmd, webp_data, webp_size, &output);
    dwebp_free_command(cmd);

    if (status != NEXTIMAGE_OK) {
        fprintf(stderr, "    Library decode failed: status=%d\n", status);
        return NULL;
    }

    /* Copy to malloc'd buffer so caller can free() */
    uint8_t* result = (uint8_t*)malloc(output.size);
    if (!result) {
        nextimage_free_buffer(&output);
        return NULL;
    }
    memcpy(result, output.data, output.size);
    *out_size = output.size;
    nextimage_free_buffer(&output);

    return result;
}

/**
 * Compare binary data for exact match.
 */
static int compare_binary(const char* test_name,
                           const uint8_t* cli_data, size_t cli_size,
                           const uint8_t* lib_data, size_t lib_size) {
    tests_run++;

    if (cli_size != lib_size) {
        printf("  [FAIL] %s: Size mismatch (cli=%zu, lib=%zu)\n",
               test_name, cli_size, lib_size);
        tests_failed++;
        return 0;
    }

    if (memcmp(cli_data, lib_data, cli_size) == 0) {
        printf("  [PASS] %s: Binary exact match (%zu bytes)\n",
               test_name, cli_size);
        tests_passed++;
        return 1;
    }

    /* Count byte differences for debug info */
    int diff_count = 0;
    size_t first_diff = 0;
    for (size_t i = 0; i < cli_size; i++) {
        if (cli_data[i] != lib_data[i]) {
            if (diff_count == 0) first_diff = i;
            diff_count++;
        }
    }
    printf("  [FAIL] %s: Binary mismatch (%d/%zu bytes differ, first at offset %zu)\n",
           test_name, diff_count, cli_size, first_diff);
    tests_failed++;
    return 0;
}

/* ============================================
 * Tests: Default PNG decode
 * ============================================ */
static void test_dwebp_default_png(void) {
    printf("\n=== dwebp Default PNG Decode - Binary Exact Match Tests ===\n");

    struct {
        const char* name;
        const char* webp_file;
    } cases[] = {
        {"lossy-q75",       "webp-samples/lossy-q75.webp"},
        {"lossy-q90",       "webp-samples/lossy-q90.webp"},
        {"lossless",        "webp-samples/lossless.webp"},
        {"alpha-gradient",  "webp-samples/alpha-gradient.webp"},
        {"alpha-lossless",  "webp-samples/alpha-lossless.webp"},
        {"small-128x128",   "webp-samples/small-128x128.webp"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char webp_path[512];
        snprintf(webp_path, sizeof(webp_path), "%s/%s", TESTDATA_DIR, cases[i].webp_file);

        if (!file_exists(webp_path)) {
            printf("  [SKIP] %s: file not found\n", cases[i].name);
            tests_skipped++;
            continue;
        }

        size_t webp_size;
        uint8_t* webp_data = read_file(webp_path, &webp_size);
        if (!webp_data) { tests_skipped++; continue; }

        /* CLI */
        char cli_png[512];
        snprintf(cli_png, sizeof(cli_png), "%s/dwebp-cmd/%s.png", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_data = run_dwebp_cli(webp_path, "", cli_png, &cli_size);

        /* Library */
        DWebPOptions* opts = dwebp_create_default_options();
        opts->output_format = DWEBP_OUTPUT_PNG;
        size_t lib_size;
        uint8_t* lib_data = run_dwebp_lib(webp_data, webp_size, opts, &lib_size);
        dwebp_free_options(opts);

        if (cli_data && lib_data) {
            compare_binary(cases[i].name, cli_data, cli_size, lib_data, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: decode failed\n", cases[i].name);
        }
        if (cli_data) free(cli_data);
        if (lib_data) free(lib_data);
        free(webp_data);
    }
}

/* ============================================
 * Tests: NoFancy upsampling
 * ============================================ */
static void test_dwebp_nofancy(void) {
    printf("\n=== dwebp NoFancy - Binary Exact Match Tests ===\n");

    struct {
        const char* name;
        const char* webp_file;
    } cases[] = {
        {"lossy-q75-nofancy",       "webp-samples/lossy-q75.webp"},
        {"alpha-gradient-nofancy",  "webp-samples/alpha-gradient.webp"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char webp_path[512];
        snprintf(webp_path, sizeof(webp_path), "%s/%s", TESTDATA_DIR, cases[i].webp_file);

        size_t webp_size;
        uint8_t* webp_data = read_file(webp_path, &webp_size);
        if (!webp_data) { tests_skipped++; continue; }

        char cli_png[512];
        snprintf(cli_png, sizeof(cli_png), "%s/dwebp-cmd/%s.png", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_data = run_dwebp_cli(webp_path, "-nofancy", cli_png, &cli_size);

        DWebPOptions* opts = dwebp_create_default_options();
        opts->output_format = DWEBP_OUTPUT_PNG;
        opts->no_fancy_upsampling = 1;
        size_t lib_size;
        uint8_t* lib_data = run_dwebp_lib(webp_data, webp_size, opts, &lib_size);
        dwebp_free_options(opts);

        if (cli_data && lib_data) {
            compare_binary(cases[i].name, cli_data, cli_size, lib_data, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        if (cli_data) free(cli_data);
        if (lib_data) free(lib_data);
        free(webp_data);
    }
}

/* ============================================
 * Tests: NoFilter
 * ============================================ */
static void test_dwebp_nofilter(void) {
    printf("\n=== dwebp NoFilter - Binary Exact Match Tests ===\n");

    struct {
        const char* name;
        const char* webp_file;
    } cases[] = {
        {"lossy-q75-nofilter",       "webp-samples/lossy-q75.webp"},
        {"alpha-gradient-nofilter",  "webp-samples/alpha-gradient.webp"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char webp_path[512];
        snprintf(webp_path, sizeof(webp_path), "%s/%s", TESTDATA_DIR, cases[i].webp_file);

        size_t webp_size;
        uint8_t* webp_data = read_file(webp_path, &webp_size);
        if (!webp_data) { tests_skipped++; continue; }

        char cli_png[512];
        snprintf(cli_png, sizeof(cli_png), "%s/dwebp-cmd/%s.png", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_data = run_dwebp_cli(webp_path, "-nofilter", cli_png, &cli_size);

        DWebPOptions* opts = dwebp_create_default_options();
        opts->output_format = DWEBP_OUTPUT_PNG;
        opts->bypass_filtering = 1;
        size_t lib_size;
        uint8_t* lib_data = run_dwebp_lib(webp_data, webp_size, opts, &lib_size);
        dwebp_free_options(opts);

        if (cli_data && lib_data) {
            compare_binary(cases[i].name, cli_data, cli_size, lib_data, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        if (cli_data) free(cli_data);
        if (lib_data) free(lib_data);
        free(webp_data);
    }
}

/* ============================================
 * Tests: Large image
 * ============================================ */
static void test_dwebp_large(void) {
    printf("\n=== dwebp Large Image - Binary Exact Match Tests ===\n");

    char webp_path[512];
    snprintf(webp_path, sizeof(webp_path), "%s/webp-samples/large-2048x2048.webp", TESTDATA_DIR);

    if (!file_exists(webp_path)) {
        printf("  [SKIP] large WebP sample not found\n");
        tests_skipped++;
        return;
    }

    size_t webp_size;
    uint8_t* webp_data = read_file(webp_path, &webp_size);
    if (!webp_data) { tests_skipped++; return; }

    char cli_png[512];
    snprintf(cli_png, sizeof(cli_png), "%s/dwebp-cmd/large-2048x2048.png", TEMP_DIR);
    size_t cli_size;
    uint8_t* cli_data = run_dwebp_cli(webp_path, "", cli_png, &cli_size);

    DWebPOptions* opts = dwebp_create_default_options();
    opts->output_format = DWEBP_OUTPUT_PNG;
    size_t lib_size;
    uint8_t* lib_data = run_dwebp_lib(webp_data, webp_size, opts, &lib_size);
    dwebp_free_options(opts);

    if (cli_data && lib_data) {
        compare_binary("large-2048x2048", cli_data, cli_size, lib_data, lib_size);
    } else {
        tests_run++;
        tests_failed++;
    }
    if (cli_data) free(cli_data);
    if (lib_data) free(lib_data);
    free(webp_data);
}

int main(void) {
    char dwebp_path[512];
    snprintf(dwebp_path, sizeof(dwebp_path), "%s/dwebp", BIN_DIR);
    if (!file_exists(dwebp_path)) {
        printf("SKIP: bin/dwebp not found. Run scripts/build-cli-tools.sh first.\n");
        return 0;
    }

    ensure_dir("../../build-test-compat/dwebp-cmd");

    printf("dwebp CLI Binary Exact Match Compatibility Tests\n");
    printf("================================================\n");

    test_dwebp_default_png();
    test_dwebp_nofancy();
    test_dwebp_nofilter();
    test_dwebp_large();

    printf("\n================================================\n");
    printf("Results: %d tests, %d passed, %d failed, %d skipped\n",
           tests_run, tests_passed, tests_failed, tests_skipped);

    return tests_failed > 0 ? 1 : 0;
}
