package libnextimage

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// CLI tool paths
var (
	binDir = filepath.Join("..", "bin")
)

// setupCompatTest checks that CLI tools are available
func skipIfNoCLI(t *testing.T, tool string) {
	t.Helper()
	path := filepath.Join(binDir, tool)
	if _, err := os.Stat(path); os.IsNotExist(err) {
		t.Skipf("%s not found at %s, run scripts/build-cli-tools.sh first", tool, path)
	}
}

// runCLI runs a CLI tool and returns the output bytes from a temp file
func runCLI(t *testing.T, tool string, inputFile string, args []string) []byte {
	t.Helper()

	tmpFile, err := os.CreateTemp("", "compat-*.out")
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer os.Remove(tmpFile.Name())
	tmpFile.Close()

	cliPath := filepath.Join(binDir, tool)
	cmdArgs := make([]string, 0)
	cmdArgs = append(cmdArgs, args...)

	if tool == "avifenc" || tool == "avifdec" {
		// avifenc/avifdec: input output (positional)
		cmdArgs = append(cmdArgs, inputFile, tmpFile.Name())
	} else {
		// cwebp/dwebp/gif2webp: input -o output
		cmdArgs = append(cmdArgs, inputFile, "-o", tmpFile.Name())
	}

	cmd := exec.Command(cliPath, cmdArgs...)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("%s command failed: %v\nOutput: %s", tool, err, string(output))
	}

	data, err := os.ReadFile(tmpFile.Name())
	if err != nil {
		t.Fatalf("Failed to read %s output: %v", tool, err)
	}

	return data
}

// ============================================
// LegacyToWebP: JPEG → WebP compat tests (cwebp)
// ============================================

func TestCompat_LegacyToWebP_JPEG(t *testing.T) {
	skipIfNoCLI(t, "cwebp")

	testCases := []struct {
		name    string
		file    string
		quality int
		args    []string
	}{
		{"jpeg-q75", "jpeg-source/medium-512x512.jpg", 75, []string{"-q", "75"}},
		{"jpeg-q90", "jpeg-source/medium-512x512.jpg", 90, []string{"-q", "90"}},
		{"jpeg-small-q75", "jpeg-source/small-128x128.jpg", 75, []string{"-q", "75"}},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			// CLI
			cliOutput := runCLI(t, "cwebp", inputPath, tc.args)

			// Library
			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      tc.quality,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if libOutput.MimeType != "image/webp" {
				t.Errorf("Expected MimeType image/webp, got %s", libOutput.MimeType)
			}

			// Compare
			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToWebP: PNG → WebP compat tests (cwebp -lossless)
// ============================================

func TestCompat_LegacyToWebP_PNG(t *testing.T) {
	skipIfNoCLI(t, "cwebp")

	testCases := []struct {
		name string
		file string
	}{
		{"png-medium", "source/sizes/medium-512x512.png"},
		{"png-small", "source/sizes/small-128x128.png"},
		{"png-alpha", "source/alpha/alpha-gradient.png"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			// CLI: cwebp -lossless
			cliOutput := runCLI(t, "cwebp", inputPath, []string{"-lossless"})

			// Library: LegacyToWebP auto-detects PNG → lossless
			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      -1,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToWebP: GIF → WebP compat tests (gif2webp)
// ============================================

func TestCompat_LegacyToWebP_GIF(t *testing.T) {
	skipIfNoCLI(t, "gif2webp")

	testCases := []struct {
		name string
		file string
	}{
		{"gif-static", "gif-source/static-64x64.gif"},
		{"gif-animated", "gif-source/animated-3frames.gif"},
		{"gif-alpha", "gif-source/static-alpha.gif"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			// CLI: gif2webp (default options)
			cliOutput := runCLI(t, "gif2webp", inputPath, []string{})

			// Library
			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      -1,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if libOutput.MimeType != "image/webp" {
				t.Errorf("Expected MimeType image/webp, got %s", libOutput.MimeType)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToWebP: JPEG with multiple quality values
// ============================================

func TestCompat_LegacyToWebP_QualityRange(t *testing.T) {
	skipIfNoCLI(t, "cwebp")

	// Use JPEG input - quality parameter only applies to lossy encoding (JPEG→WebP)
	inputPath := filepath.Join(testdataDir, "jpeg-source/medium-512x512.jpg")
	inputData, err := os.ReadFile(inputPath)
	if err != nil {
		t.Fatalf("Failed to read input: %v", err)
	}

	qualities := []int{0, 25, 50, 75, 90, 100}

	for _, q := range qualities {
		t.Run(fmt.Sprintf("quality-%d", q), func(t *testing.T) {
			cliOutput := runCLI(t, "cwebp", inputPath, []string{"-q", fmt.Sprintf("%d", q)})

			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      q,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToWebP: Various PNG sizes (lossless)
// ============================================

func TestCompat_LegacyToWebP_PNGSizes(t *testing.T) {
	skipIfNoCLI(t, "cwebp")

	testCases := []struct {
		name string
		file string
	}{
		{"tiny-16x16", "source/sizes/tiny-16x16.png"},
		{"small-128x128", "source/sizes/small-128x128.png"},
		{"medium-512x512", "source/sizes/medium-512x512.png"},
		{"large-2048x2048", "source/sizes/large-2048x2048.png"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			// PNG → lossless WebP: CLI uses -lossless, library auto-detects
			cliOutput := runCLI(t, "cwebp", inputPath, []string{"-lossless"})

			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      -1,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToWebP: Compression patterns (PNG → lossless)
// ============================================

func TestCompat_LegacyToWebP_Compression(t *testing.T) {
	skipIfNoCLI(t, "cwebp")

	testCases := []struct {
		name string
		file string
	}{
		{"flat-color", "source/compression/flat-color.png"},
		{"noisy", "source/compression/noisy.png"},
		{"edges", "source/compression/edges.png"},
		{"text", "source/compression/text.png"},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			// PNG → lossless WebP: CLI uses -lossless, library auto-detects
			cliOutput := runCLI(t, "cwebp", inputPath, []string{"-lossless"})

			libOutput := LegacyToWebP(ConvertInput{
				Data:         inputData,
				Quality:      -1,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				t.Errorf("Binary mismatch: cli=%d bytes, lib=%d bytes", len(cliOutput), len(libOutput.Data))
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToAvif: JPEG → AVIF (lossy with quality)
// NOTE: Known CLI differences exist. The CLI avifenc uses "Directly copied
// JPEG pixel data" optimization for JPEG input (skips RGB decode/re-encode),
// so binary exact match is not expected. This test validates that the library
// produces valid AVIF output with correct MIME type.
// ============================================

func TestCompat_LegacyToAvif_JPEG(t *testing.T) {
	skipIfNoCLI(t, "avifenc")

	testCases := []struct {
		name    string
		file    string
		quality int
		args    []string
	}{
		{"jpeg-q50", "jpeg-source/small-128x128.jpg", 50, []string{"-q", "50"}},
		{"jpeg-q75", "jpeg-source/small-128x128.jpg", 75, []string{"-q", "75"}},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			inputPath := filepath.Join(testdataDir, tc.file)
			inputData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read input: %v", err)
			}

			cliOutput := runCLI(t, "avifenc", inputPath, tc.args)

			libOutput := LegacyToAvif(ConvertInput{
				Data:         inputData,
				Quality:      tc.quality,
				MinQuantizer: -1,
				MaxQuantizer: -1,
			})
			if libOutput.Error != nil {
				t.Fatalf("Library failed: %v", libOutput.Error)
			}

			if libOutput.MimeType != "image/avif" {
				t.Errorf("Expected MimeType image/avif, got %s", libOutput.MimeType)
			}

			if !bytes.Equal(cliOutput, libOutput.Data) {
				sizeDiff := len(cliOutput) - len(libOutput.Data)
				if sizeDiff < 0 {
					sizeDiff = -sizeDiff
				}
				// Known difference: CLI avifenc uses JPEG direct copy optimization
				t.Logf("Expected binary mismatch (JPEG direct copy): cli=%d bytes, lib=%d bytes (diff=%d)", len(cliOutput), len(libOutput.Data), sizeDiff)
			} else {
				t.Logf("Binary exact match (%d bytes)", len(cliOutput))
			}
		})
	}
}

// ============================================
// LegacyToAvif: PNG → AVIF (lossless)
// The library sets lossless parameters (quality=100, quantizer=0, YUV444, IDENTITY).
// CLI avifenc -l uses different internal settings, so binary match is not expected.
// ============================================

func TestCompat_LegacyToAvif_PNG_Lossless(t *testing.T) {
	inputData, err := os.ReadFile(filepath.Join(testdataDir, "source/sizes/small-128x128.png"))
	if err != nil {
		t.Fatalf("Failed to read input: %v", err)
	}

	libOutput := LegacyToAvif(ConvertInput{
		Data:         inputData,
		Quality:      -1,
		MinQuantizer: -1,
		MaxQuantizer: -1,
	})
	if libOutput.Error != nil {
		t.Fatalf("Library failed: %v", libOutput.Error)
	}

	if libOutput.MimeType != "image/avif" {
		t.Errorf("Expected MimeType image/avif, got %s", libOutput.MimeType)
	}

	// Verify valid AVIF output (ftyp box)
	if len(libOutput.Data) < 12 || string(libOutput.Data[4:8]) != "ftyp" {
		t.Error("Output is not valid AVIF (missing ftyp box)")
	}
	t.Logf("PNG->AVIF lossless: %d -> %d bytes (mime=%s)", len(inputData), len(libOutput.Data), libOutput.MimeType)
}
