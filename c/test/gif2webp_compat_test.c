/**
 * gif2webp CLI互換性テスト
 *
 * gif2webp_run_command() の出力が bin/gif2webp CLI と
 * バイナリレベルで完全に一致することを検証する。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nextimage.h"
#include "nextimage/gif2webp.h"

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

static uint8_t* run_gif2webp_cli(const char* input_file, const char* args,
                                  const char* output_file, size_t* out_size) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s/gif2webp %s '%s' -o '%s' 2>/dev/null",
             BIN_DIR, args, input_file, output_file);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "    CLI command failed: %s\n", cmd);
        return NULL;
    }
    return read_file(output_file, out_size);
}

static uint8_t* run_gif2webp_lib(const uint8_t* gif_data, size_t gif_size,
                                  const Gif2WebPOptions* opts, size_t* out_size) {
    Gif2WebPCommand* cmd = gif2webp_new_command(opts);
    if (!cmd) return NULL;

    NextImageBuffer output = {0};
    NextImageStatus status = gif2webp_run_command(cmd, gif_data, gif_size, &output);
    gif2webp_free_command(cmd);

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
 * Tests: Static GIFs
 * ============================================ */
static void test_gif2webp_static(void) {
    printf("\n=== gif2webp Static GIF Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"static-64x64",    "gif-source/static-64x64.gif"},
        {"static-512x512",  "gif-source/static-512x512.gif"},
        {"static-16x16",    "gif-source/static-16x16.gif"},
        {"gradient",        "gif-source/gradient.gif"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char input_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", TESTDATA_DIR, cases[i].file);

        size_t gif_size;
        uint8_t* gif_data = read_file(input_path, &gif_size);
        if (!gif_data) {
            printf("  [SKIP] Cannot read: %s\n", input_path);
            tests_skipped++;
            continue;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/gif2webp-cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_gif2webp_cli(input_path, "", output_path, &cli_size);

        Gif2WebPOptions* opts = gif2webp_create_default_options();
        size_t lib_size;
        uint8_t* lib_output = run_gif2webp_lib(gif_data, gif_size, opts, &lib_size);
        gif2webp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: conversion failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
        free(gif_data);
    }
}

/* ============================================
 * Tests: Animated GIFs
 * ============================================ */
static void test_gif2webp_animated(void) {
    printf("\n=== gif2webp Animated GIF Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"animated-3frames",  "gif-source/animated-3frames.gif"},
        {"animated-small",    "gif-source/animated-small.gif"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char input_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", TESTDATA_DIR, cases[i].file);

        size_t gif_size;
        uint8_t* gif_data = read_file(input_path, &gif_size);
        if (!gif_data) {
            printf("  [SKIP] Cannot read: %s\n", input_path);
            tests_skipped++;
            continue;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/gif2webp-cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_gif2webp_cli(input_path, "", output_path, &cli_size);

        Gif2WebPOptions* opts = gif2webp_create_default_options();
        size_t lib_size;
        uint8_t* lib_output = run_gif2webp_lib(gif_data, gif_size, opts, &lib_size);
        gif2webp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: conversion failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
        free(gif_data);
    }
}

/* ============================================
 * Tests: Alpha GIFs
 * ============================================ */
static void test_gif2webp_alpha(void) {
    printf("\n=== gif2webp Alpha GIF Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"static-alpha",    "gif-source/static-alpha.gif"},
        {"animated-alpha",  "gif-source/animated-alpha.gif"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char input_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", TESTDATA_DIR, cases[i].file);

        size_t gif_size;
        uint8_t* gif_data = read_file(input_path, &gif_size);
        if (!gif_data) {
            printf("  [SKIP] Cannot read: %s\n", input_path);
            tests_skipped++;
            continue;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/gif2webp-cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_gif2webp_cli(input_path, "", output_path, &cli_size);

        Gif2WebPOptions* opts = gif2webp_create_default_options();
        size_t lib_size;
        uint8_t* lib_output = run_gif2webp_lib(gif_data, gif_size, opts, &lib_size);
        gif2webp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
            printf("  [FAIL] %s: conversion failed\n", cases[i].name);
        }
        free(cli_output);
        free(lib_output);
        free(gif_data);
    }
}

/* ============================================
 * Tests: Quality settings
 * ============================================ */
static void test_gif2webp_quality(void) {
    printf("\n=== gif2webp Quality Tests ===\n");

    char input_path[512];
    snprintf(input_path, sizeof(input_path), "%s/gif-source/static-64x64.gif", TESTDATA_DIR);

    size_t gif_size;
    uint8_t* gif_data = read_file(input_path, &gif_size);
    if (!gif_data) {
        printf("  [SKIP] Cannot read input\n");
        tests_skipped++;
        return;
    }

    struct {
        const char* name;
        float quality;
        const char* args;
    } cases[] = {
        {"quality-50", 50, "-q 50"},
        {"quality-90", 90, "-q 90"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    for (int i = 0; i < num_cases; i++) {
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/gif2webp-cmd/%s.webp", TEMP_DIR, cases[i].name);
        size_t cli_size;
        uint8_t* cli_output = run_gif2webp_cli(input_path, cases[i].args, output_path, &cli_size);

        Gif2WebPOptions* opts = gif2webp_create_default_options();
        opts->quality = cases[i].quality;
        size_t lib_size;
        uint8_t* lib_output = run_gif2webp_lib(gif_data, gif_size, opts, &lib_size);
        gif2webp_free_options(opts);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
        } else {
            tests_run++;
            tests_failed++;
        }
        free(cli_output);
        free(lib_output);
    }
    free(gif_data);
}

int main(void) {
    char gif2webp_path[512];
    snprintf(gif2webp_path, sizeof(gif2webp_path), "%s/gif2webp", BIN_DIR);
    if (!file_exists(gif2webp_path)) {
        printf("SKIP: bin/gif2webp not found. Run scripts/build-cli-tools.sh first.\n");
        return 0;
    }

    char cmd_dir[512];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/gif2webp-cmd", TEMP_DIR);
    ensure_dir(cmd_dir);

    printf("gif2webp CLI Binary Exact Match Compatibility Tests\n");
    printf("===================================================\n");

    test_gif2webp_static();
    test_gif2webp_animated();
    test_gif2webp_alpha();
    test_gif2webp_quality();

    printf("\n===================================================\n");
    printf("Results: %d tests, %d passed, %d failed, %d skipped\n",
           tests_run, tests_passed, tests_failed, tests_skipped);

    return tests_failed > 0 ? 1 : 0;
}
