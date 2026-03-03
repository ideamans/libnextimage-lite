/**
 * cwebp CLI互換性テスト
 *
 * cwebp_run_command() の出力が bin/cwebp CLI と
 * バイナリレベルで完全に一致することを検証する。
 *
 * これはこのライブラリの最重要テストである。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nextimage.h"
#include "nextimage/cwebp.h"

/* ============================================
 * ヘルパー関数
 * ============================================ */

static const char* PROJECT_ROOT = NULL;
static const char* BIN_DIR = NULL;
static const char* TESTDATA_DIR = NULL;
static const char* TEMP_DIR = NULL;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

static void init_paths(void) {
    /* build dir is c/build, project root is ../.. from test binary */
    PROJECT_ROOT = "../..";
    BIN_DIR = "../../bin";
    TESTDATA_DIR = "../../testdata";
    TEMP_DIR = "../../build-test-compat";
}

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

static int write_file(const char* path, const uint8_t* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return (written == size) ? 0 : -1;
}

/**
 * Run bin/cwebp CLI and return output bytes.
 * Returns NULL on failure.
 */
static uint8_t* run_cwebp_cli(const char* input_file, const char* args,
                               const char* output_file, size_t* out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/cwebp %s '%s' -o '%s' 2>/dev/null",
             BIN_DIR, args, input_file, output_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "    CLI command failed: %s\n", cmd);
        return NULL;
    }
    return read_file(output_file, out_size);
}

/**
 * Run library cwebp_run_command and return output bytes.
 */
static uint8_t* run_cwebp_lib(const uint8_t* input_data, size_t input_size,
                               const CWebPOptions* opts, size_t* out_size) {
    CWebPCommand* cmd = cwebp_new_command(opts);
    if (!cmd) return NULL;

    NextImageBuffer output = {0};
    NextImageStatus status = cwebp_run_command(cmd, input_data, input_size, &output);
    cwebp_free_command(cmd);

    if (status != NEXTIMAGE_OK) {
        fprintf(stderr, "    Library encode failed: status=%d\n", status);
        return NULL;
    }

    /* Copy output data (we need to free the buffer) */
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
 * Compare two byte arrays for exact match.
 * Returns 1 on match, 0 on mismatch.
 */
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
 * テストケース: Quality
 * ============================================ */
static void test_cwebp_quality(void) {
    printf("\n=== cwebp Quality Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/sizes/medium-512x512.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input: %s\n", input_path);
        tests_skipped++;
        return;
    }

    float qualities[] = {0, 25, 50, 75, 90, 100};
    int num_qualities = sizeof(qualities) / sizeof(qualities[0]);

    for (int i = 0; i < num_qualities; i++) {
        float q = qualities[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "quality-%.0f", q);

        /* CLI */
        char args[64];
        snprintf(args, sizeof(args), "-q %.0f", q);
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, test_name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, args, output_path, &cli_size);
        if (!cli_output) { tests_run++; tests_failed++; continue; }

        /* Library */
        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = q;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);
        if (!lib_output) { free(cli_output); tests_run++; tests_failed++; continue; }

        compare_outputs(test_name, cli_output, cli_size, lib_output, lib_size);
        free(cli_output);
        free(lib_output);
    }

    free(input_data);
}

/* ============================================
 * テストケース: Lossless
 * ============================================ */
static void test_cwebp_lossless(void) {
    printf("\n=== cwebp Lossless Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"lossless-medium",  "source/sizes/medium-512x512.png"},
        {"lossless-small",   "source/sizes/small-128x128.png"},
        {"lossless-alpha",   "source/alpha/alpha-gradient.png"},
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

        /* CLI */
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, "-lossless", output_path, &cli_size);

        /* Library */
        CWebPOptions* opts = cwebp_create_default_options();
        opts->lossless = 1;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

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
 * テストケース: Method
 * ============================================ */
static void test_cwebp_method(void) {
    printf("\n=== cwebp Method Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/sizes/medium-512x512.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    int methods[] = {0, 2, 4, 6};
    int num_methods = sizeof(methods) / sizeof(methods[0]);

    for (int i = 0; i < num_methods; i++) {
        int m = methods[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "method-%d", m);

        char args[64];
        snprintf(args, sizeof(args), "-q 75 -m %d", m);
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, test_name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, args, output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 75;
        opts->method = m;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(test_name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * テストケース: Sizes
 * ============================================ */
static void test_cwebp_sizes(void) {
    printf("\n=== cwebp Size Variation Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"tiny-16x16",         "source/sizes/tiny-16x16.png"},
        {"small-128x128",      "source/sizes/small-128x128.png"},
        {"medium-512x512",     "source/sizes/medium-512x512.png"},
        {"large-2048x2048",    "source/sizes/large-2048x2048.png"},
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
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, "-q 80", output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 80;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

/* ============================================
 * テストケース: Alpha
 * ============================================ */
static void test_cwebp_alpha(void) {
    printf("\n=== cwebp Alpha Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"alpha-opaque",      "source/alpha/opaque.png"},
        {"alpha-transparent", "source/alpha/transparent.png"},
        {"alpha-gradient",    "source/alpha/alpha-gradient.png"},
        {"alpha-complex",     "source/alpha/alpha-complex.png"},
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
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, "-q 75", output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 75;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

/* ============================================
 * テストケース: Compression characteristics
 * ============================================ */
static void test_cwebp_compression(void) {
    printf("\n=== cwebp Compression Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"flat-color", "source/compression/flat-color.png"},
        {"noisy",      "source/compression/noisy.png"},
        {"edges",      "source/compression/edges.png"},
        {"text",       "source/compression/text.png"},
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
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, "-q 75", output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 75;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

/* ============================================
 * テストケース: Alpha Quality
 * ============================================ */
static void test_cwebp_alpha_quality(void) {
    printf("\n=== cwebp Alpha Quality Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/alpha/alpha-gradient.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    int alpha_qualities[] = {0, 50, 100};
    int num = sizeof(alpha_qualities) / sizeof(alpha_qualities[0]);

    for (int i = 0; i < num; i++) {
        int aq = alpha_qualities[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "alpha-q-%d", aq);

        char args[128];
        snprintf(args, sizeof(args), "-q 75 -alpha_q %d", aq);
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, test_name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, args, output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 75;
        opts->alpha_quality = aq;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(test_name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * テストケース: Exact mode
 * ============================================ */
static void test_cwebp_exact(void) {
    printf("\n=== cwebp Exact Mode Test ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/alpha/alpha-gradient.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/cmd/exact.webp", TEMP_DIR);
    size_t cli_size;
    uint8_t* cli_output = run_cwebp_cli(input_path, "-q 75 -exact", output_path, &cli_size);

    CWebPOptions* opts = cwebp_create_default_options();
    opts->quality = 75;
    opts->exact = 1;
    size_t lib_size;
    uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
    cwebp_free_options(opts);

    if (cli_output && lib_output) {
        compare_outputs("exact-mode", cli_output, cli_size, lib_output, lib_size);
    } else {
        tests_run++;
        tests_failed++;
    }
    free(cli_output);
    free(lib_output);
    free(input_data);
}

/* ============================================
 * テストケース: Pass (entropy analysis passes)
 * ============================================ */
static void test_cwebp_pass(void) {
    printf("\n=== cwebp Pass Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/source/sizes/medium-512x512.png", TESTDATA_DIR);

    size_t input_size;
    uint8_t* input_data = read_file(input_path, &input_size);
    if (!input_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    int passes[] = {1, 5, 10};
    int num = sizeof(passes) / sizeof(passes[0]);

    for (int i = 0; i < num; i++) {
        int p = passes[i];
        char test_name[64];
        snprintf(test_name, sizeof(test_name), "pass-%d", p);

        char args[128];
        snprintf(args, sizeof(args), "-q 75 -pass %d", p);
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, test_name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, args, output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = 75;
        opts->pass = p;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(test_name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * テストケース: Option combinations
 * ============================================ */
static void test_cwebp_combinations(void) {
    printf("\n=== cwebp Option Combination Tests ===\n");

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
        const char* args;
        float quality;
        int method;
        int pass;
        int lossless;
        int alpha_quality;
    } cases[] = {
        {"q90-m6",              "-q 90 -m 6",               90, 6, 0, 0, -1},
        {"q75-m4-pass10",       "-q 75 -m 4 -pass 10",      75, 4, 10, 0, -1},
        {"lossless-m4",         "-lossless -m 4",            0, 4, 0, 1, -1},
        {"q80-alpha_q50",       "-q 80 -alpha_q 50",         80, -1, 0, 0, 50},
        {"q85-m6-pass5",        "-q 85 -m 6 -pass 5",        85, 6, 5, 0, -1},
        {"q90-m6-alpha_q100",   "-q 90 -m 6 -alpha_q 100",   90, 6, 0, 0, 100},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, cases[i].args, output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        if (cases[i].quality > 0) opts->quality = cases[i].quality;
        if (cases[i].method >= 0) opts->method = cases[i].method;
        if (cases[i].pass > 0) opts->pass = cases[i].pass;
        if (cases[i].lossless) opts->lossless = 1;
        if (cases[i].alpha_quality >= 0) opts->alpha_quality = cases[i].alpha_quality;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
    }
    free(input_data);
}

/* ============================================
 * テストケース: JPEG input
 * ============================================ */
static void test_cwebp_jpeg_input(void) {
    printf("\n=== cwebp JPEG Input Tests ===\n");

    struct {
        const char* name;
        const char* file;
        float quality;
    } cases[] = {
        {"jpeg-medium-q75",  "jpeg-source/medium-512x512.jpg", 75},
        {"jpeg-medium-q90",  "jpeg-source/medium-512x512.jpg", 90},
        {"jpeg-small-q75",   "jpeg-source/small-128x128.jpg",  75},
        {"jpeg-photo-q75",   "jpeg-source/photo-like.jpg",     75},
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

        char args[64];
        snprintf(args, sizeof(args), "-q %.0f", cases[i].quality);
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_cwebp_cli(input_path, args, output_path, &cli_size);

        CWebPOptions* opts = cwebp_create_default_options();
        opts->quality = cases[i].quality;
        size_t lib_size;
        uint8_t* lib_output = run_cwebp_lib(input_data, input_size, opts, &lib_size);
        cwebp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
        free(input_data);
    }
}

/* ============================================
 * main
 * ============================================ */
int main(void) {
    init_paths();

    /* Check bin/cwebp exists */
    char cwebp_path[512];
    snprintf(cwebp_path, sizeof(cwebp_path), "%s/cwebp", BIN_DIR);
    if (!file_exists(cwebp_path)) {
        printf("SKIP: bin/cwebp not found. Run scripts/build-cli-tools.sh first.\n");
        return 0;
    }

    /* Create temp directory */
    char cmd_dir[512];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/cmd", TEMP_DIR);
    ensure_dir(cmd_dir);

    printf("cwebp CLI Binary Exact Match Compatibility Tests\n");
    printf("================================================\n");

    test_cwebp_quality();
    test_cwebp_lossless();
    test_cwebp_method();
    test_cwebp_sizes();
    test_cwebp_alpha();
    test_cwebp_compression();
    test_cwebp_alpha_quality();
    test_cwebp_exact();
    test_cwebp_pass();
    test_cwebp_combinations();
    test_cwebp_jpeg_input();

    printf("\n================================================\n");
    printf("Results: %d tests, %d passed, %d failed, %d skipped\n",
           tests_run, tests_passed, tests_failed, tests_skipped);

    return tests_failed > 0 ? 1 : 0;
}
