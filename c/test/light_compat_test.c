/**
 * Light API CLI互換性テスト
 *
 * nextimage_lite_legacy_to_webp() の出力が
 * cwebp / gif2webp CLI とバイナリレベルで完全に一致することを検証する。
 *
 * legacyToWebp の変換仕様:
 *   JPEG → cwebp -metadata icc -q 80 -o out.webp in.jpg
 *   PNG  → cwebp -metadata icc -lossless -o out.webp in.png
 *   GIF  → gif2webp -o out.webp in.gif
 *
 * さらに、ICC付き入力は変換後もICCが保持されていることを
 * フォーマット別の解析ユーティリティで検証する。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "nextimage.h"
#include "nextimage_lite.h"

/* ============================================
 * ヘルパー関数
 * ============================================ */

static const char* BIN_DIR = NULL;
static const char* TESTDATA_DIR = NULL;
static const char* TEMP_DIR = NULL;

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

static void init_paths(void) {
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
    size_t rd = fread(data, 1, size, f);
    fclose(f);
    if ((long)rd != size) { free(data); return NULL; }
    *out_size = (size_t)size;
    return data;
}

/* ============================================
 * ICC プロファイル検出ユーティリティ
 *
 * JPEG / PNG / WebP それぞれのフォーマットで
 * ICC プロファイルの有無とサイズを返す。
 * ============================================ */

static uint32_t read_u32_be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t read_u32_le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * JPEG: ICC は APP2 マーカー (0xFFE2) 内の "ICC_PROFILE\0" で格納。
 * 複数チャンクに分割される場合がある。合計サイズを返す。
 * 戻り値: ICC プロファイルの合計バイト数 (0 = なし)
 */
static size_t detect_icc_in_jpeg(const uint8_t* data, size_t size) {
    static const uint8_t icc_sig[] = "ICC_PROFILE";
    const size_t icc_header_len = 14; /* "ICC_PROFILE\0" + seq(1) + count(1) */
    size_t total_icc = 0;
    size_t pos = 2; /* skip SOI (FF D8) */

    if (size < 4 || data[0] != 0xFF || data[1] != 0xD8) return 0;

    while (pos + 4 <= size) {
        if (data[pos] != 0xFF) break;
        uint8_t marker = data[pos + 1];
        if (marker == 0xD9) break; /* EOI */
        if (marker == 0xDA) break; /* SOS - end of headers */

        uint16_t seg_len = ((uint16_t)data[pos + 2] << 8) | data[pos + 3];
        if (seg_len < 2) break;

        /* APP2 = 0xE2 */
        if (marker == 0xE2 && seg_len >= icc_header_len + 2) {
            const uint8_t* seg_data = data + pos + 4;
            if (memcmp(seg_data, icc_sig, 12) == 0) {
                /* seg_data[12] = seq_no, seg_data[13] = num_markers */
                size_t payload = seg_len - 2 - icc_header_len;
                total_icc += payload;
            }
        }

        pos += 2 + seg_len;
    }
    return total_icc;
}

/**
 * PNG: ICC は iCCP チャンクに格納 (zlib 圧縮)。
 * チャンクの存在と圧縮済みサイズを返す。
 * 戻り値: iCCP チャンクのデータ長 (0 = なし)
 *
 * 注意: 実際のICCプロファイルはzlib展開後のサイズだが、
 *       ここではチャンク存在の検出が目的なので圧縮済みサイズを返す。
 */
static size_t detect_icc_in_png(const uint8_t* data, size_t size) {
    /* PNG signature: 89 50 4E 47 0D 0A 1A 0A */
    if (size < 8 || data[0] != 0x89 || data[1] != 'P') return 0;

    size_t pos = 8; /* skip signature */
    while (pos + 12 <= size) {
        uint32_t chunk_len = read_u32_be(data + pos);
        const uint8_t* chunk_type = data + pos + 4;

        if (memcmp(chunk_type, "iCCP", 4) == 0) {
            return chunk_len; /* iCCP チャンクが存在 */
        }
        if (memcmp(chunk_type, "IEND", 4) == 0) break;

        pos += 12 + chunk_len; /* length(4) + type(4) + data(chunk_len) + crc(4) */
    }
    return 0;
}

/**
 * WebP: ICC は RIFF コンテナ内の ICCP チャンクに格納。
 * VP8X 拡張ヘッダの ICCP フラグ (0x20) もチェック。
 * 戻り値: ICCP チャンクのデータ長 (0 = なし)
 */
static size_t detect_icc_in_webp(const uint8_t* data, size_t size) {
    /* RIFF....WEBP */
    if (size < 12) return 0;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) return 0;

    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t* chunk_type = data + pos;
        uint32_t chunk_len = read_u32_le(data + pos + 4);

        if (memcmp(chunk_type, "ICCP", 4) == 0) {
            return chunk_len; /* ICCP チャンクが存在 */
        }

        /* VP8/VP8L/VP8X 以降のチャンクも走査 */
        pos += 8 + chunk_len;
        if (chunk_len & 1) pos++; /* RIFF padding */
    }
    return 0;
}

/**
 * ICC存在アサーション: 入力と出力の両方で ICC の有無を検証。
 * expect_icc=1: 入力にICCあり → 出力WebPにもICCあり を期待
 * expect_icc=0: 入力にICCなし → 出力WebPにもICCなし を期待
 */
typedef enum { FMT_JPEG, FMT_PNG } InputFormat;

static void assert_icc(const char* test_name,
                       const uint8_t* input_data, size_t input_size,
                       InputFormat input_fmt,
                       const uint8_t* webp_data, size_t webp_size,
                       int expect_icc) {
    /* 入力の ICC 検出 */
    size_t input_icc = 0;
    const char* fmt_name = "?";
    if (input_fmt == FMT_JPEG) {
        input_icc = detect_icc_in_jpeg(input_data, input_size);
        fmt_name = "JPEG";
    } else {
        input_icc = detect_icc_in_png(input_data, input_size);
        fmt_name = "PNG";
    }

    /* 出力 WebP の ICC 検出 */
    size_t output_icc = detect_icc_in_webp(webp_data, webp_size);

    tests_run++;

    if (expect_icc) {
        /* 入力にICCがあること */
        if (input_icc == 0) {
            printf("  [FAIL] %s ICC: input %s has no ICC (expected ICC)\n", test_name, fmt_name);
            tests_failed++;
            return;
        }
        /* 出力にもICCがあること */
        if (output_icc == 0) {
            printf("  [FAIL] %s ICC: output WebP has no ICCP (input %s had %zu bytes ICC)\n",
                   test_name, fmt_name, input_icc);
            tests_failed++;
            return;
        }
        printf("  [PASS] %s ICC: %s(%zu bytes) -> WebP ICCP(%zu bytes) preserved\n",
               test_name, fmt_name, input_icc, output_icc);
        tests_passed++;
    } else {
        /* 入力にICCがないこと */
        if (input_icc != 0) {
            printf("  [FAIL] %s ICC: input %s has ICC (%zu bytes) but expected none\n",
                   test_name, fmt_name, input_icc);
            tests_failed++;
            return;
        }
        /* 出力にもICCがないこと */
        if (output_icc != 0) {
            printf("  [FAIL] %s ICC: output WebP has ICCP (%zu bytes) but input had no ICC\n",
                   test_name, output_icc);
            tests_failed++;
            return;
        }
        printf("  [PASS] %s ICC: no ICC in input %s, no ICCP in output WebP\n",
               test_name, fmt_name);
        tests_passed++;
    }
}

/* ============================================
 * CLI / Library 実行ヘルパー
 * ============================================ */

static uint8_t* run_cli(const char* full_cmd, const char* output_file, size_t* out_size) {
    int ret = system(full_cmd);
    if (ret != 0) {
        fprintf(stderr, "    CLI command failed: %s\n", full_cmd);
        return NULL;
    }
    return read_file(output_file, out_size);
}

static uint8_t* run_light_legacy_to_webp(const uint8_t* input_data, size_t input_size,
                                          size_t* out_size) {
    NextImageLiteInput input;
    memset(&input, 0, sizeof(input));
    input.data = input_data;
    input.size = input_size;
    input.quality = -1;
    input.min_quantizer = -1;
    input.max_quantizer = -1;

    NextImageLiteOutput output;
    memset(&output, 0, sizeof(output));

    NextImageStatus status = nextimage_lite_legacy_to_webp(&input, &output);
    if (status != NEXTIMAGE_OK) {
        fprintf(stderr, "    Library encode failed: status=%d\n", status);
        return NULL;
    }

    uint8_t* result = (uint8_t*)malloc(output.size);
    if (!result) {
        nextimage_lite_free(&output);
        return NULL;
    }
    memcpy(result, output.data, output.size);
    *out_size = output.size;
    nextimage_lite_free(&output);
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
 * テストケース: JPEG → WebP (cwebp -metadata icc -q 80)
 * ============================================ */
static void test_light_jpeg_to_webp(void) {
    printf("\n=== Light API: JPEG -> WebP Tests ===\n");

    struct {
        const char* name;
        const char* file;
        int has_icc;
    } cases[] = {
        {"jpeg-srgb",       "icc/srgb.jpg",       1},
        {"jpeg-display-p3", "icc/display-p3.jpg",  1},
        {"jpeg-no-icc",     "icc/no-icc.jpg",      0},
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

        /* CLI: cwebp -metadata icc -q 80 -o out.webp in.jpg */
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/light-cmd/%s.webp", TEMP_DIR, cases[i].name);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s/cwebp -metadata icc -q 80 '%s' -o '%s' 2>/dev/null",
                 BIN_DIR, input_path, output_path);
        size_t cli_size;
        uint8_t* cli_output = run_cli(cmd, output_path, &cli_size);

        /* Library */
        size_t lib_size;
        uint8_t* lib_output = run_light_legacy_to_webp(input_data, input_size, &lib_size);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
            /* ICC アサーション */
            assert_icc(cases[i].name, input_data, input_size, FMT_JPEG,
                       lib_output, lib_size, cases[i].has_icc);
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
 * テストケース: PNG → WebP (cwebp -metadata icc -lossless)
 * ============================================ */
static void test_light_png_to_webp(void) {
    printf("\n=== Light API: PNG -> WebP Tests ===\n");

    struct {
        const char* name;
        const char* file;
        int has_icc;
    } cases[] = {
        {"png-srgb",       "icc/srgb.png",       1},
        {"png-display-p3", "icc/display-p3.png",  1},
        {"png-no-icc",     "icc/no-icc.png",      0},
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

        /* CLI: cwebp -metadata icc -lossless -o out.webp in.png */
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/light-cmd/%s.webp", TEMP_DIR, cases[i].name);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s/cwebp -metadata icc -lossless '%s' -o '%s' 2>/dev/null",
                 BIN_DIR, input_path, output_path);
        size_t cli_size;
        uint8_t* cli_output = run_cli(cmd, output_path, &cli_size);

        /* Library */
        size_t lib_size;
        uint8_t* lib_output = run_light_legacy_to_webp(input_data, input_size, &lib_size);

        if (cli_output && lib_output) {
            compare_outputs(cases[i].name, cli_output, cli_size, lib_output, lib_size);
            /* ICC アサーション */
            assert_icc(cases[i].name, input_data, input_size, FMT_PNG,
                       lib_output, lib_size, cases[i].has_icc);
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
 * テストケース: GIF → WebP (gif2webp default)
 * ============================================ */
static void test_light_gif_to_webp(void) {
    printf("\n=== Light API: GIF -> WebP Tests ===\n");

    struct {
        const char* name;
        const char* file;
    } cases[] = {
        {"gif-static-64x64",  "gif-source/static-64x64.gif"},
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);

    char gif2webp_path[512];
    snprintf(gif2webp_path, sizeof(gif2webp_path), "%s/gif2webp", BIN_DIR);
    if (!file_exists(gif2webp_path)) {
        printf("  [SKIP] bin/gif2webp not found\n");
        tests_skipped += num_cases;
        return;
    }

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

        /* CLI: gif2webp -o out.webp in.gif */
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/light-cmd/%s.webp", TEMP_DIR, cases[i].name);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s/gif2webp '%s' -o '%s' 2>/dev/null",
                 BIN_DIR, input_path, output_path);
        size_t cli_size;
        uint8_t* cli_output = run_cli(cmd, output_path, &cli_size);

        /* Library */
        size_t lib_size;
        uint8_t* lib_output = run_light_legacy_to_webp(input_data, input_size, &lib_size);

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
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/light-cmd", TEMP_DIR);
    ensure_dir(cmd_dir);

    printf("Light API (legacyToWebp) CLI Binary Exact Match Tests\n");
    printf("=====================================================\n");

    test_light_jpeg_to_webp();
    test_light_png_to_webp();
    test_light_gif_to_webp();

    printf("\n=====================================================\n");
    printf("Results: %d tests, %d passed, %d failed, %d skipped\n",
           tests_run, tests_passed, tests_failed, tests_skipped);

    return tests_failed > 0 ? 1 : 0;
}
