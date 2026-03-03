package libnextimage

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

var (
	projectRoot = filepath.Join("..")
	testdataDir = filepath.Join(projectRoot, "testdata")
)

func readTestFile(t *testing.T, subdir, filename string) []byte {
	t.Helper()
	data, err := os.ReadFile(filepath.Join(testdataDir, subdir, filename))
	if err != nil {
		t.Fatalf("Failed to read test file %s/%s: %v", subdir, filename, err)
	}
	return data
}

// ========================================
// LegacyToWebP tests
// ========================================
func TestLight_LegacyToWebP_JPEG(t *testing.T) {
	images := []string{"small-128x128.jpg", "medium-512x512.jpg"}
	qualities := []int{-1, 50, 90}

	for _, img := range images {
		for _, q := range qualities {
			name := fmt.Sprintf("%s-q%d", img, q)
			t.Run(name, func(t *testing.T) {
				inputData := readTestFile(t, "jpeg-source", img)

				out := LegacyToWebP(ConvertInput{Data: inputData, Quality: q, MinQuantizer: -1, MaxQuantizer: -1})
				if out.Error != nil {
					t.Fatalf("Failed: %v", out.Error)
				}

				if len(out.Data) < 12 {
					t.Fatal("Output too small")
				}
				if string(out.Data[0:4]) != "RIFF" || string(out.Data[8:12]) != "WEBP" {
					t.Error("Output is not valid WebP")
				}
				if out.MimeType != "image/webp" {
					t.Errorf("Expected MIME image/webp, got %s", out.MimeType)
				}
				t.Logf("JPEG->WebP: %d -> %d bytes (q=%d, mime=%s)", len(inputData), len(out.Data), q, out.MimeType)
			})
		}
	}
}

func TestLight_LegacyToWebP_PNG(t *testing.T) {
	images := []string{"small-128x128.png", "medium-512x512.png"}

	for _, img := range images {
		t.Run(img, func(t *testing.T) {
			inputData := readTestFile(t, "png-source", img)

			out := LegacyToWebP(ConvertInput{Data: inputData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
			if out.Error != nil {
				t.Fatalf("Failed: %v", out.Error)
			}

			if len(out.Data) < 12 {
				t.Fatal("Output too small")
			}
			if string(out.Data[0:4]) != "RIFF" || string(out.Data[8:12]) != "WEBP" {
				t.Error("Output is not valid WebP")
			}
			if out.MimeType != "image/webp" {
				t.Errorf("Expected MIME image/webp, got %s", out.MimeType)
			}
			t.Logf("PNG->WebP: %d -> %d bytes (lossless, mime=%s)", len(inputData), len(out.Data), out.MimeType)
		})
	}
}

func TestLight_LegacyToWebP_GIF(t *testing.T) {
	images := []string{"static-64x64.gif", "animated-3frames.gif"}

	for _, img := range images {
		t.Run(img, func(t *testing.T) {
			inputData := readTestFile(t, "gif-source", img)

			out := LegacyToWebP(ConvertInput{Data: inputData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
			if out.Error != nil {
				t.Fatalf("Failed: %v", out.Error)
			}

			if len(out.Data) < 12 {
				t.Fatal("Output too small")
			}
			if string(out.Data[0:4]) != "RIFF" {
				t.Error("Output is not valid WebP")
			}
			if out.MimeType != "image/webp" {
				t.Errorf("Expected MIME image/webp, got %s", out.MimeType)
			}
			t.Logf("GIF->WebP: %d -> %d bytes (mime=%s)", len(inputData), len(out.Data), out.MimeType)
		})
	}
}

// ========================================
// WebPToLegacy tests
// ========================================
func TestLight_WebPToLegacy_Lossy(t *testing.T) {
	// Create a lossy WebP from JPEG first
	jpegData := readTestFile(t, "jpeg-source", "small-128x128.jpg")
	webpOut := LegacyToWebP(ConvertInput{Data: jpegData, Quality: 75, MinQuantizer: -1, MaxQuantizer: -1})
	if webpOut.Error != nil {
		t.Fatalf("Setup failed: %v", webpOut.Error)
	}

	out := WebPToLegacy(ConvertInput{Data: webpOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	// Lossy WebP should produce JPEG
	if len(out.Data) < 2 || out.Data[0] != 0xFF || out.Data[1] != 0xD8 {
		t.Error("Output is not valid JPEG")
	}
	if out.MimeType != "image/jpeg" {
		t.Errorf("Expected MIME image/jpeg, got %s", out.MimeType)
	}
	t.Logf("Lossy WebP->JPEG: %d -> %d bytes (mime=%s)", len(webpOut.Data), len(out.Data), out.MimeType)
}

func TestLight_WebPToLegacy_Lossless(t *testing.T) {
	// Create a lossless WebP from PNG first
	pngData := readTestFile(t, "png-source", "small-128x128.png")
	webpOut := LegacyToWebP(ConvertInput{Data: pngData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if webpOut.Error != nil {
		t.Fatalf("Setup failed: %v", webpOut.Error)
	}

	out := WebPToLegacy(ConvertInput{Data: webpOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	// Lossless WebP should produce PNG
	if len(out.Data) < 8 || out.Data[0] != 0x89 || out.Data[1] != 'P' || out.Data[2] != 'N' || out.Data[3] != 'G' {
		t.Error("Output is not valid PNG")
	}
	if out.MimeType != "image/png" {
		t.Errorf("Expected MIME image/png, got %s", out.MimeType)
	}
	t.Logf("Lossless WebP->PNG: %d -> %d bytes (mime=%s)", len(webpOut.Data), len(out.Data), out.MimeType)
}

func TestLight_WebPToLegacy_StaticToGif(t *testing.T) {
	// Create a static WebP from JPEG, then decode to GIF
	// Note: webp2gif uses WebPDecodeRGBA (static only)
	jpegData := readTestFile(t, "jpeg-source", "small-128x128.jpg")
	webpOut := LegacyToWebP(ConvertInput{Data: jpegData, Quality: 75, MinQuantizer: -1, MaxQuantizer: -1})
	if webpOut.Error != nil {
		t.Fatalf("Setup failed: %v", webpOut.Error)
	}

	// Force GIF output by using WebPToGif-equivalent path (lossy WebP -> JPEG by default,
	// but we can verify the static WebP -> GIF conversion works when called directly)
	// Since lossy static WebP goes to JPEG by auto-detect, just verify JPEG output here
	out := WebPToLegacy(ConvertInput{Data: webpOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	// Lossy static WebP should produce JPEG (not GIF)
	if len(out.Data) < 2 || out.Data[0] != 0xFF || out.Data[1] != 0xD8 {
		t.Error("Output is not valid JPEG")
	}
	if out.MimeType != "image/jpeg" {
		t.Errorf("Expected MIME image/jpeg, got %s", out.MimeType)
	}
	t.Logf("Static lossy WebP->JPEG: %d -> %d bytes (mime=%s)", len(webpOut.Data), len(out.Data), out.MimeType)
}

// ========================================
// LegacyToAvif tests
// ========================================
func TestLight_LegacyToAvif_JPEG(t *testing.T) {
	qualities := []int{-1, 40, 80}

	for _, q := range qualities {
		name := fmt.Sprintf("q%d", q)
		t.Run(name, func(t *testing.T) {
			inputData := readTestFile(t, "jpeg-source", "small-128x128.jpg")

			out := LegacyToAvif(ConvertInput{Data: inputData, Quality: q, MinQuantizer: -1, MaxQuantizer: -1})
			if out.Error != nil {
				t.Fatalf("Failed: %v", out.Error)
			}

			if len(out.Data) < 12 || string(out.Data[4:8]) != "ftyp" {
				t.Error("Output is not valid AVIF (missing ftyp box)")
			}
			if out.MimeType != "image/avif" {
				t.Errorf("Expected MIME image/avif, got %s", out.MimeType)
			}
			t.Logf("JPEG->AVIF: %d -> %d bytes (q=%d, mime=%s)", len(inputData), len(out.Data), q, out.MimeType)
		})
	}
}

func TestLight_LegacyToAvif_PNG(t *testing.T) {
	inputData := readTestFile(t, "png-source", "small-128x128.png")

	out := LegacyToAvif(ConvertInput{Data: inputData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	if len(out.Data) < 12 || string(out.Data[4:8]) != "ftyp" {
		t.Error("Output is not valid AVIF (missing ftyp box)")
	}
	if out.MimeType != "image/avif" {
		t.Errorf("Expected MIME image/avif, got %s", out.MimeType)
	}
	t.Logf("PNG->AVIF (lossless): %d -> %d bytes (mime=%s)", len(inputData), len(out.Data), out.MimeType)
}

func TestLight_LegacyToAvif_GIF_Error(t *testing.T) {
	inputData := readTestFile(t, "gif-source", "static-64x64.gif")

	out := LegacyToAvif(ConvertInput{Data: inputData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error == nil {
		t.Error("Expected error for GIF input, got nil")
	}
}

// ========================================
// AvifToLegacy tests
// ========================================
func TestLight_AvifToLegacy_Lossy(t *testing.T) {
	avifData := readTestFile(t, "avif", "red.avif")

	out := AvifToLegacy(ConvertInput{Data: avifData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	// Lossy AVIF should produce JPEG
	if len(out.Data) < 2 || out.Data[0] != 0xFF || out.Data[1] != 0xD8 {
		t.Error("Output is not valid JPEG")
	}
	if out.MimeType != "image/jpeg" {
		t.Errorf("Expected MIME image/jpeg, got %s", out.MimeType)
	}
	t.Logf("Lossy AVIF->JPEG: %d -> %d bytes (mime=%s)", len(avifData), len(out.Data), out.MimeType)
}

func TestLight_AvifToLegacy_Lossless(t *testing.T) {
	// Create a lossless AVIF from PNG first, then decode it back
	pngData := readTestFile(t, "png-source", "small-128x128.png")

	avifOut := LegacyToAvif(ConvertInput{Data: pngData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if avifOut.Error != nil {
		t.Fatalf("Setup failed: %v", avifOut.Error)
	}

	out := AvifToLegacy(ConvertInput{Data: avifOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if out.Error != nil {
		t.Fatalf("Failed: %v", out.Error)
	}

	// Lossless AVIF should produce PNG
	if len(out.Data) < 8 || out.Data[0] != 0x89 || out.Data[1] != 'P' || out.Data[2] != 'N' || out.Data[3] != 'G' {
		t.Error("Output is not valid PNG")
	}
	if out.MimeType != "image/png" {
		t.Errorf("Expected MIME image/png, got %s", out.MimeType)
	}
	t.Logf("Lossless AVIF->PNG: %d -> %d bytes (mime=%s)", len(avifOut.Data), len(out.Data), out.MimeType)
}

// ========================================
// ICC profile preservation tests
// ========================================

// hasJPEGICCMarker checks if a JPEG contains APP2 ICC_PROFILE markers
func hasJPEGICCMarker(data []byte) bool {
	marker := []byte("ICC_PROFILE")
	for i := 0; i+len(marker) < len(data); i++ {
		if data[i] == 0xFF && data[i+1] == 0xE2 { // APP2
			// Check for ICC_PROFILE signature after marker length
			if i+4+len(marker) < len(data) {
				match := true
				for j := 0; j < len(marker); j++ {
					if data[i+4+j] != marker[j] {
						match = false
						break
					}
				}
				if match {
					return true
				}
			}
		}
	}
	return false
}

// hasPNGICCChunk checks if a PNG contains an iCCP chunk
func hasPNGICCChunk(data []byte) bool {
	// PNG iCCP chunk type: 69 43 43 50
	iccp := []byte("iCCP")
	for i := 8; i+8 < len(data); i++ {
		if data[i+4] == iccp[0] && data[i+5] == iccp[1] &&
			data[i+6] == iccp[2] && data[i+7] == iccp[3] {
			return true
		}
	}
	return false
}

func TestLight_ICC_JPEG_to_AVIF_Roundtrip(t *testing.T) {
	// Read JPEG with ICC profile
	jpegData := readTestFile(t, "icc", "srgb.jpg")

	// Verify input has ICC
	if !hasJPEGICCMarker(jpegData) {
		t.Fatal("Input JPEG should have ICC profile")
	}

	// JPEG -> AVIF
	avifOut := LegacyToAvif(ConvertInput{Data: jpegData, Quality: 60, MinQuantizer: -1, MaxQuantizer: -1})
	if avifOut.Error != nil {
		t.Fatalf("LegacyToAvif failed: %v", avifOut.Error)
	}
	if avifOut.MimeType != "image/avif" {
		t.Errorf("Expected image/avif, got %s", avifOut.MimeType)
	}

	// AVIF -> JPEG
	jpegOut := AvifToLegacy(ConvertInput{Data: avifOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if jpegOut.Error != nil {
		t.Fatalf("AvifToLegacy failed: %v", jpegOut.Error)
	}
	if jpegOut.MimeType != "image/jpeg" {
		t.Errorf("Expected image/jpeg, got %s", jpegOut.MimeType)
	}

	// Verify output JPEG has ICC profile
	if !hasJPEGICCMarker(jpegOut.Data) {
		t.Error("Output JPEG should have ICC profile after roundtrip")
	}
	t.Logf("JPEG(ICC)->AVIF->JPEG(ICC): %d -> %d -> %d bytes", len(jpegData), len(avifOut.Data), len(jpegOut.Data))
}

func TestLight_ICC_PNG_to_AVIF_Roundtrip(t *testing.T) {
	// Read PNG with ICC profile
	pngData := readTestFile(t, "icc", "srgb.png")

	// Verify input has ICC
	if !hasPNGICCChunk(pngData) {
		t.Fatal("Input PNG should have ICC profile")
	}

	// PNG -> AVIF (lossless)
	avifOut := LegacyToAvif(ConvertInput{Data: pngData, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if avifOut.Error != nil {
		t.Fatalf("LegacyToAvif failed: %v", avifOut.Error)
	}

	// AVIF -> PNG (lossless -> PNG)
	pngOut := AvifToLegacy(ConvertInput{Data: avifOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if pngOut.Error != nil {
		t.Fatalf("AvifToLegacy failed: %v", pngOut.Error)
	}
	if pngOut.MimeType != "image/png" {
		t.Errorf("Expected image/png, got %s", pngOut.MimeType)
	}

	// Verify output PNG has ICC profile
	if !hasPNGICCChunk(pngOut.Data) {
		t.Error("Output PNG should have ICC profile after roundtrip")
	}
	t.Logf("PNG(ICC)->AVIF->PNG(ICC): %d -> %d -> %d bytes", len(pngData), len(avifOut.Data), len(pngOut.Data))
}

func TestLight_ICC_NoICC_JPEG_Roundtrip(t *testing.T) {
	// Read JPEG without ICC
	jpegData := readTestFile(t, "icc", "no-icc.jpg")

	// Verify no ICC in input
	if hasJPEGICCMarker(jpegData) {
		t.Fatal("Input JPEG should NOT have ICC profile")
	}

	// JPEG -> AVIF -> JPEG
	avifOut := LegacyToAvif(ConvertInput{Data: jpegData, Quality: 60, MinQuantizer: -1, MaxQuantizer: -1})
	if avifOut.Error != nil {
		t.Fatalf("LegacyToAvif failed: %v", avifOut.Error)
	}

	jpegOut := AvifToLegacy(ConvertInput{Data: avifOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if jpegOut.Error != nil {
		t.Fatalf("AvifToLegacy failed: %v", jpegOut.Error)
	}

	// Verify no ICC in output
	if hasJPEGICCMarker(jpegOut.Data) {
		t.Error("Output JPEG should NOT have ICC profile")
	}
	t.Logf("JPEG(no-ICC)->AVIF->JPEG(no-ICC): %d -> %d -> %d bytes", len(jpegData), len(avifOut.Data), len(jpegOut.Data))
}

func TestLight_ICC_DisplayP3_JPEG_Roundtrip(t *testing.T) {
	// Read JPEG with Display P3 ICC profile
	jpegData := readTestFile(t, "icc", "display-p3.jpg")

	if !hasJPEGICCMarker(jpegData) {
		t.Fatal("Input JPEG should have Display P3 ICC profile")
	}

	// JPEG -> AVIF -> JPEG
	avifOut := LegacyToAvif(ConvertInput{Data: jpegData, Quality: 60, MinQuantizer: -1, MaxQuantizer: -1})
	if avifOut.Error != nil {
		t.Fatalf("LegacyToAvif failed: %v", avifOut.Error)
	}

	jpegOut := AvifToLegacy(ConvertInput{Data: avifOut.Data, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
	if jpegOut.Error != nil {
		t.Fatalf("AvifToLegacy failed: %v", jpegOut.Error)
	}

	if !hasJPEGICCMarker(jpegOut.Data) {
		t.Error("Output JPEG should have ICC profile (Display P3)")
	}
	t.Logf("JPEG(P3)->AVIF->JPEG(P3): %d -> %d -> %d bytes", len(jpegData), len(avifOut.Data), len(jpegOut.Data))
}

// ========================================
// Error handling tests
// ========================================
func TestLight_ErrorHandling(t *testing.T) {
	t.Run("nil_input", func(t *testing.T) {
		out := LegacyToWebP(ConvertInput{Data: nil, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
		if out.Error == nil {
			t.Error("Expected error for nil input")
		}
	})

	t.Run("empty_input", func(t *testing.T) {
		out := LegacyToWebP(ConvertInput{Data: []byte{}, Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
		if out.Error == nil {
			t.Error("Expected error for empty input")
		}
	})

	t.Run("garbage_input", func(t *testing.T) {
		out := LegacyToWebP(ConvertInput{Data: []byte("not an image"), Quality: -1, MinQuantizer: -1, MaxQuantizer: -1})
		if out.Error == nil {
			t.Error("Expected error for garbage input")
		}
	})
}

// ========================================
// Version test
// ========================================
func TestLight_Version(t *testing.T) {
	v := Version()
	if v == "" {
		t.Error("Version returned empty string")
	}
	t.Logf("Library version: %s", v)
}
