package libnextimage

import (
	"bytes"
	"fmt"
	"image"
	"image/jpeg"
	"os"
	"os/exec"
	"path/filepath"
	"testing"
)

// setupAVIFDecodeCompatTest ensures avifdec/avifenc are available and creates temp directories
func setupAVIFDecodeCompatTest(t *testing.T) {
	t.Helper()

	if _, err := exec.LookPath("avifenc"); err != nil {
		t.Skip("avifenc not found in PATH - skipping AVIF decode compatibility tests")
	}
	if _, err := exec.LookPath("avifdec"); err != nil {
		t.Skip("avifdec not found in PATH - skipping AVIF decode compatibility tests")
	}

	if err := os.MkdirAll(filepath.Join(tempDir, "avif-decode-cmd-output"), 0755); err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	if err := os.MkdirAll(filepath.Join(tempDir, "avif-decode-lib-output"), 0755); err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
	if err := os.MkdirAll(filepath.Join(tempDir, "avif-samples"), 0755); err != nil {
		t.Fatalf("Failed to create temp dir: %v", err)
	}
}

// prepareAVIFSample encodes a PNG to AVIF using avifenc and returns the AVIF file path
func prepareAVIFSample(t *testing.T, pngPath string, name string, avifencArgs []string) string {
	t.Helper()

	avifPath := filepath.Join(tempDir, "avif-samples", name+".avif")

	// Check if already prepared
	if _, err := os.Stat(avifPath); err == nil {
		return avifPath
	}

	cmdArgs := append(avifencArgs, pngPath, avifPath)
	cmd := exec.Command("avifenc", cmdArgs...)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("avifenc failed for %s: %v\nOutput: %s", name, err, string(output))
	}

	return avifPath
}

// runAVIFDecToFile runs avifdec command and saves output to a PNG file
func runAVIFDecToFile(t *testing.T, avifPath string, args []string, outputPNG string) {
	t.Helper()

	cmdArgs := append(args, avifPath, outputPNG)
	cmd := exec.Command("avifdec", cmdArgs...)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("avifdec failed: %v\nOutput: %s", err, string(output))
	}
}

// decodeAVIFWithLibraryToFile decodes AVIF to PNG using the library's AVIFDecCommand
func decodeAVIFWithLibraryToFile(t *testing.T, avifPath string, outputPNG string, opts AVIFDecOptions) {
	t.Helper()

	avifData, err := os.ReadFile(avifPath)
	if err != nil {
		t.Fatalf("Failed to read AVIF file: %v", err)
	}

	cmd, err := NewAVIFDecCommand(&opts)
	if err != nil {
		t.Fatalf("Failed to create AVIFDecCommand: %v", err)
	}
	defer cmd.Close()

	pngData, err := cmd.Run(avifData)
	if err != nil {
		t.Fatalf("Library decode failed: %v", err)
	}

	err = os.WriteFile(outputPNG, pngData, 0644)
	if err != nil {
		t.Fatalf("Failed to write PNG file: %v", err)
	}
}

// TestDecodeCompat_AVIF_Default tests default AVIF decoding against avifdec
func TestDecodeCompat_AVIF_Default(t *testing.T) {
	setupAVIFDecodeCompatTest(t)

	srcMedium := filepath.Join(testdataDir, "source/sizes/medium-512x512.png")
	srcAlpha := filepath.Join(testdataDir, "source/alpha/alpha-gradient.png")

	testCases := []struct {
		name        string
		srcPNG      string
		avifencArgs []string
		avifdecArgs []string
		decodeOpts  AVIFDecOptions
	}{
		{
			name:        "lossy-q50",
			srcPNG:      srcMedium,
			avifencArgs: []string{"-q", "50"},
			avifdecArgs: []string{},
			decodeOpts:  NewDefaultAVIFDecOptions(),
		},
		{
			name:        "lossy-q75",
			srcPNG:      srcMedium,
			avifencArgs: []string{"-q", "75"},
			avifdecArgs: []string{},
			decodeOpts:  NewDefaultAVIFDecOptions(),
		},
		{
			name:        "lossy-q90",
			srcPNG:      srcMedium,
			avifencArgs: []string{"-q", "90"},
			avifdecArgs: []string{},
			decodeOpts:  NewDefaultAVIFDecOptions(),
		},
		{
			name:        "lossless",
			srcPNG:      srcMedium,
			avifencArgs: []string{"-l"},
			avifdecArgs: []string{},
			decodeOpts:  NewDefaultAVIFDecOptions(),
		},
		{
			name:        "alpha-gradient-q75",
			srcPNG:      srcAlpha,
			avifencArgs: []string{"-q", "75"},
			avifdecArgs: []string{},
			decodeOpts:  NewDefaultAVIFDecOptions(),
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			// Prepare AVIF sample
			avifPath := prepareAVIFSample(t, tc.srcPNG, tc.name, tc.avifencArgs)

			// avifdec decode
			cmdPNG := filepath.Join(tempDir, "avif-decode-cmd-output", tc.name+".png")
			runAVIFDecToFile(t, avifPath, tc.avifdecArgs, cmdPNG)

			// Library decode
			libPNG := filepath.Join(tempDir, "avif-decode-lib-output", tc.name+".png")
			decodeAVIFWithLibraryToFile(t, avifPath, libPNG, tc.decodeOpts)

			// Compare pixel data
			compareAVIFDecodeOutputs(t, tc.name, cmdPNG, libPNG)
		})
	}
}

// TestDecodeCompat_AVIF_Threading tests AVIF decoding with threading enabled
func TestDecodeCompat_AVIF_Threading(t *testing.T) {
	setupAVIFDecodeCompatTest(t)

	srcLarge := filepath.Join(testdataDir, "source/sizes/large-2048x2048.png")

	testCases := []struct {
		name        string
		srcPNG      string
		avifencArgs []string
		avifdecArgs []string
		decodeOpts  AVIFDecOptions
	}{
		{
			name:        "large-mt",
			srcPNG:      srcLarge,
			avifencArgs: []string{"-q", "75"},
			avifdecArgs: []string{"-j", "all"},
			decodeOpts: func() AVIFDecOptions {
				o := NewDefaultAVIFDecOptions()
				o.UseThreads = true
				return o
			}(),
		},
		{
			name:        "large-st",
			srcPNG:      srcLarge,
			avifencArgs: []string{"-q", "75"},
			avifdecArgs: []string{"-j", "1"},
			decodeOpts: func() AVIFDecOptions {
				o := NewDefaultAVIFDecOptions()
				o.UseThreads = false
				return o
			}(),
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			avifPath := prepareAVIFSample(t, tc.srcPNG, "threading-"+tc.name, tc.avifencArgs)

			cmdPNG := filepath.Join(tempDir, "avif-decode-cmd-output", "threading-"+tc.name+".png")
			runAVIFDecToFile(t, avifPath, tc.avifdecArgs, cmdPNG)

			libPNG := filepath.Join(tempDir, "avif-decode-lib-output", "threading-"+tc.name+".png")
			decodeAVIFWithLibraryToFile(t, avifPath, libPNG, tc.decodeOpts)

			compareAVIFDecodeOutputs(t, tc.name, cmdPNG, libPNG)
		})
	}
}

// TestDecodeCompat_AVIF_ChromaUpsampling tests chroma upsampling mode compatibility
func TestDecodeCompat_AVIF_ChromaUpsampling(t *testing.T) {
	setupAVIFDecodeCompatTest(t)

	// YUV420 images require chroma upsampling
	srcMedium := filepath.Join(testdataDir, "source/sizes/medium-512x512.png")

	testCases := []struct {
		name             string
		avifencArgs      []string
		avifdecArgs      []string
		chromaUpsampling int
	}{
		{
			name:             "yuv420-automatic",
			avifencArgs:      []string{"-q", "75", "-y", "420"},
			avifdecArgs:      []string{"-u", "automatic"},
			chromaUpsampling: 0, // automatic
		},
		{
			name:             "yuv420-fastest",
			avifencArgs:      []string{"-q", "75", "-y", "420"},
			avifdecArgs:      []string{"-u", "fastest"},
			chromaUpsampling: 1, // fastest
		},
		{
			name:             "yuv420-best",
			avifencArgs:      []string{"-q", "75", "-y", "420"},
			avifdecArgs:      []string{"-u", "best"},
			chromaUpsampling: 2, // best_quality
		},
		{
			name:             "yuv420-nearest",
			avifencArgs:      []string{"-q", "75", "-y", "420"},
			avifdecArgs:      []string{"-u", "nearest"},
			chromaUpsampling: 3, // nearest
		},
		{
			name:             "yuv420-bilinear",
			avifencArgs:      []string{"-q", "75", "-y", "420"},
			avifdecArgs:      []string{"-u", "bilinear"},
			chromaUpsampling: 4, // bilinear
		},
		{
			name:             "yuv422-automatic",
			avifencArgs:      []string{"-q", "75", "-y", "422"},
			avifdecArgs:      []string{"-u", "automatic"},
			chromaUpsampling: 0,
		},
		{
			name:             "yuv422-nearest",
			avifencArgs:      []string{"-q", "75", "-y", "422"},
			avifdecArgs:      []string{"-u", "nearest"},
			chromaUpsampling: 3,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			avifPath := prepareAVIFSample(t, srcMedium, "chroma-"+tc.name, tc.avifencArgs)

			cmdPNG := filepath.Join(tempDir, "avif-decode-cmd-output", "chroma-"+tc.name+".png")
			runAVIFDecToFile(t, avifPath, tc.avifdecArgs, cmdPNG)

			opts := NewDefaultAVIFDecOptions()
			opts.ChromaUpsampling = tc.chromaUpsampling
			libPNG := filepath.Join(tempDir, "avif-decode-lib-output", "chroma-"+tc.name+".png")
			decodeAVIFWithLibraryToFile(t, avifPath, libPNG, opts)

			compareAVIFDecodeOutputs(t, tc.name, cmdPNG, libPNG)
		})
	}
}

// TestDecodeCompat_AVIF_JPEGOutput tests AVIF decoding to JPEG output
func TestDecodeCompat_AVIF_JPEGOutput(t *testing.T) {
	setupAVIFDecodeCompatTest(t)

	srcMedium := filepath.Join(testdataDir, "source/sizes/medium-512x512.png")

	testCases := []struct {
		name        string
		avifencArgs []string
		jpegQuality int
	}{
		{
			name:        "jpeg-q75",
			avifencArgs: []string{"-q", "75"},
			jpegQuality: 75,
		},
		{
			name:        "jpeg-q90",
			avifencArgs: []string{"-q", "90"},
			jpegQuality: 90,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			avifPath := prepareAVIFSample(t, srcMedium, "jpeg-"+tc.name, tc.avifencArgs)

			// avifdec outputs JPEG
			cmdJPEG := filepath.Join(tempDir, "avif-decode-cmd-output", tc.name+".jpg")
			runAVIFDecToFile(t, avifPath, []string{"-q", fmt.Sprintf("%d", tc.jpegQuality)}, cmdJPEG)

			// Library outputs JPEG
			opts := NewDefaultAVIFDecOptions()
			opts.OutputFormat = OutputJPEG
			opts.JPEGQuality = tc.jpegQuality
			libJPEG := filepath.Join(tempDir, "avif-decode-lib-output", tc.name+".jpg")

			avifData, err := os.ReadFile(avifPath)
			if err != nil {
				t.Fatalf("Failed to read AVIF file: %v", err)
			}

			cmd, err := NewAVIFDecCommand(&opts)
			if err != nil {
				t.Fatalf("Failed to create AVIFDecCommand: %v", err)
			}
			defer cmd.Close()

			jpegData, err := cmd.Run(avifData)
			if err != nil {
				t.Fatalf("Library JPEG decode failed: %v", err)
			}

			err = os.WriteFile(libJPEG, jpegData, 0644)
			if err != nil {
				t.Fatalf("Failed to write JPEG file: %v", err)
			}

			// Compare JPEG outputs by decoding to RGBA pixels
			compareDecodeOutputsJPEG(t, tc.name, cmdJPEG, libJPEG)
		})
	}
}

// compareAVIFDecodeOutputs compares two PNG outputs with tolerance for minor rounding differences.
// AVIF decoding may produce ±1 differences in alpha channels due to premultiplication rounding.
func compareAVIFDecodeOutputs(t *testing.T, testName string, cmdPNGFile, libPNGFile string) {
	t.Helper()

	cmdPixels, cmdW, cmdH, err := decodePNGToRGBA(cmdPNGFile)
	if err != nil {
		t.Fatalf("Failed to decode command PNG: %v", err)
	}

	libPixels, libW, libH, err := decodePNGToRGBA(libPNGFile)
	if err != nil {
		t.Fatalf("Failed to decode library PNG: %v", err)
	}

	t.Logf("  avifdec: %dx%d, %d bytes", cmdW, cmdH, len(cmdPixels))
	t.Logf("  library: %dx%d, %d bytes", libW, libH, len(libPixels))

	if cmdW != libW || cmdH != libH {
		t.Errorf("  ❌ FAILED: Dimension mismatch (cmd: %dx%d, lib: %dx%d)", cmdW, cmdH, libW, libH)
		return
	}

	if bytes.Equal(cmdPixels, libPixels) {
		t.Logf("  ✓ PASSED: Pixel data exact match")
		return
	}

	diffCount := 0
	maxDiff := 0
	for i := 0; i < len(cmdPixels); i++ {
		diff := int(cmdPixels[i]) - int(libPixels[i])
		if diff < 0 {
			diff = -diff
		}
		if diff > 0 {
			diffCount++
			if diff > maxDiff {
				maxDiff = diff
			}
		}
	}

	diffPercent := float64(diffCount) * 100.0 / float64(len(cmdPixels))
	t.Logf("  Pixel differences: %d/%d (%.2f%%), max diff: %d", diffCount, len(cmdPixels), diffPercent, maxDiff)

	// Allow max diff of 1 (rounding differences in alpha premultiplication)
	if maxDiff <= 1 {
		t.Logf("  ✓ PASSED: Pixel data within acceptable tolerance (max diff: %d)", maxDiff)
	} else {
		t.Errorf("  ❌ FAILED: Pixel data mismatch exceeds tolerance (max diff: %d)", maxDiff)
	}
}

// decodeJPEGToRGBA decodes a JPEG file to RGBA pixel data
func decodeJPEGToRGBA(filePath string) ([]byte, int, int, error) {
	f, err := os.Open(filePath)
	if err != nil {
		return nil, 0, 0, err
	}
	defer f.Close()

	img, err := jpeg.Decode(f)
	if err != nil {
		return nil, 0, 0, err
	}

	bounds := img.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()

	rgba := image.NewRGBA(image.Rect(0, 0, width, height))
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			rgba.Set(x, y, img.At(bounds.Min.X+x, bounds.Min.Y+y))
		}
	}

	return rgba.Pix, width, height, nil
}

// compareDecodeOutputsJPEG compares two JPEG outputs by decoding to RGBA pixels
func compareDecodeOutputsJPEG(t *testing.T, testName string, cmdJPEGFile, libJPEGFile string) {
	t.Helper()

	cmdPixels, cmdW, cmdH, err := decodeJPEGToRGBA(cmdJPEGFile)
	if err != nil {
		t.Fatalf("Failed to decode command JPEG: %v", err)
	}

	libPixels, libW, libH, err := decodeJPEGToRGBA(libJPEGFile)
	if err != nil {
		t.Fatalf("Failed to decode library JPEG: %v", err)
	}

	t.Logf("  avifdec: %dx%d, %d bytes", cmdW, cmdH, len(cmdPixels))
	t.Logf("  library: %dx%d, %d bytes", libW, libH, len(libPixels))

	if cmdW != libW || cmdH != libH {
		t.Errorf("  ❌ FAILED: Dimension mismatch (cmd: %dx%d, lib: %dx%d)", cmdW, cmdH, libW, libH)
		return
	}

	if bytes.Equal(cmdPixels, libPixels) {
		t.Logf("  ✓ PASSED: Pixel data exact match")
		return
	}

	// JPEG comparison allows small differences due to encoder implementations
	diffCount := 0
	maxDiff := 0
	for i := 0; i < len(cmdPixels); i++ {
		diff := int(cmdPixels[i]) - int(libPixels[i])
		if diff < 0 {
			diff = -diff
		}
		if diff > 0 {
			diffCount++
			if diff > maxDiff {
				maxDiff = diff
			}
		}
	}

	diffPercent := float64(diffCount) * 100.0 / float64(len(cmdPixels))
	t.Logf("  Pixel differences: %d/%d (%.2f%%), max diff: %d", diffCount, len(cmdPixels), diffPercent, maxDiff)

	// JPEG is lossy, allow small differences from different JPEG encoder implementations
	// (avifdec uses libjpeg, our library uses stb_image_write)
	if maxDiff <= 4 {
		t.Logf("  ✓ PASSED: Pixel data within acceptable JPEG tolerance (max diff: %d)", maxDiff)
	} else {
		t.Errorf("  ❌ FAILED: Pixel data mismatch exceeds JPEG tolerance (max diff: %d)", maxDiff)
	}
}
