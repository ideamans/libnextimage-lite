package libnextimage

import (
	"bytes"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"testing"
)

// TestWebPEncoder tests the instance-based WebPEncoder
func TestWebPEncoder(t *testing.T) {
	// Read test image
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	// Create encoder with custom options
	encoder, err := NewWebPEncoder(func(opts *WebPEncodeOptions) {
		opts.Quality = 80
		opts.Method = 6
	})
	if err != nil {
		t.Fatalf("Failed to create encoder: %v", err)
	}
	defer encoder.Close()

	// Encode first image
	webp1, err := encoder.Encode(pngData)
	if err != nil {
		t.Fatalf("Failed to encode first image: %v", err)
	}

	if len(webp1) == 0 {
		t.Fatal("Encoded WebP is empty")
	}

	// Verify WebP signature
	if string(webp1[0:4]) != "RIFF" {
		t.Fatal("Invalid WebP signature (RIFF)")
	}
	if string(webp1[8:12]) != "WEBP" {
		t.Fatal("Invalid WebP signature (WEBP)")
	}

	// Reuse encoder for second image
	pngData2, _ := os.ReadFile(filepath.Join("..", "testdata", "png", "blue.png"))
	webp2, err := encoder.Encode(pngData2)
	if err != nil {
		t.Fatalf("Failed to encode second image: %v", err)
	}

	if len(webp2) == 0 {
		t.Fatal("Second encoded WebP is empty")
	}

	t.Logf("✓ WebP encoder reused successfully: image1=%d bytes, image2=%d bytes", len(webp1), len(webp2))
}

// TestWebPEncoderWithDefaults tests encoder with default options
func TestWebPEncoderWithDefaults(t *testing.T) {
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	// Create encoder with nil callback (use defaults)
	encoder, err := NewWebPEncoder(nil)
	if err != nil {
		t.Fatalf("Failed to create encoder: %v", err)
	}
	defer encoder.Close()

	webp, err := encoder.Encode(pngData)
	if err != nil {
		t.Fatalf("Failed to encode: %v", err)
	}

	if len(webp) == 0 {
		t.Fatal("Encoded WebP is empty")
	}

	t.Logf("✓ WebP encoder with defaults: %d bytes", len(webp))
}

// TestWebPDecoder tests the instance-based WebPDecoder
func TestWebPDecoder(t *testing.T) {
	// First encode an image
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	encoder, _ := NewWebPEncoder(func(opts *WebPEncodeOptions) {
		opts.Quality = 90
	})
	webpData, _ := encoder.Encode(pngData)
	encoder.Close()

	// Create decoder
	decoder, err := NewWebPDecoder(func(opts *WebPDecodeOptions) {
		opts.Format = FormatRGBA
		opts.UseThreads = true
	})
	if err != nil {
		t.Fatalf("Failed to create decoder: %v", err)
	}
	defer decoder.Close()

	// Decode
	decoded, err := decoder.Decode(webpData)
	if err != nil {
		t.Fatalf("Failed to decode: %v", err)
	}

	if decoded.Width == 0 || decoded.Height == 0 {
		t.Fatal("Decoded image has invalid dimensions")
	}

	if len(decoded.Data) == 0 {
		t.Fatal("Decoded image data is empty")
	}

	t.Logf("✓ WebP decoder: %dx%d, format=%d, %d bytes",
		decoded.Width, decoded.Height, decoded.Format, len(decoded.Data))
}

// TestAVIFEncoder tests the instance-based AVIFEncoder
func TestAVIFEncoder(t *testing.T) {
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	// Create encoder with custom options
	encoder, err := NewAVIFEncoder(func(opts *AVIFEncodeOptions) {
		opts.Quality = 60
		opts.Speed = 8
		opts.BitDepth = 8
	})
	if err != nil {
		t.Fatalf("Failed to create encoder: %v", err)
	}
	defer encoder.Close()

	// Encode first image
	avif1, err := encoder.Encode(pngData)
	if err != nil {
		t.Fatalf("Failed to encode first image: %v", err)
	}

	if len(avif1) == 0 {
		t.Fatal("Encoded AVIF is empty")
	}

	// Reuse encoder for second image
	pngData2, _ := os.ReadFile(filepath.Join("..", "testdata", "png", "blue.png"))
	avif2, err := encoder.Encode(pngData2)
	if err != nil {
		t.Fatalf("Failed to encode second image: %v", err)
	}

	if len(avif2) == 0 {
		t.Fatal("Second encoded AVIF is empty")
	}

	t.Logf("✓ AVIF encoder reused successfully: image1=%d bytes, image2=%d bytes", len(avif1), len(avif2))
}

// TestAVIFDecoder tests the instance-based AVIFDecoder
func TestAVIFDecoder(t *testing.T) {
	// First encode an image
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	encoder, _ := NewAVIFEncoder(func(opts *AVIFEncodeOptions) {
		opts.Quality = 70
		opts.Speed = 6
	})
	avifData, _ := encoder.Encode(pngData)
	encoder.Close()

	// Create decoder
	decoder, err := NewAVIFDecoder(func(opts *AVIFDecodeOptions) {
		opts.Format = FormatRGBA
		opts.Jobs = -1
	})
	if err != nil {
		t.Fatalf("Failed to create decoder: %v", err)
	}
	defer decoder.Close()

	// Decode
	decoded, err := decoder.Decode(avifData)
	if err != nil {
		t.Fatalf("Failed to decode: %v", err)
	}

	if decoded.Width == 0 || decoded.Height == 0 {
		t.Fatal("Decoded image has invalid dimensions")
	}

	if len(decoded.Data) == 0 {
		t.Fatal("Decoded image data is empty")
	}

	t.Logf("✓ AVIF decoder: %dx%d, format=%d, %d bytes",
		decoded.Width, decoded.Height, decoded.Format, len(decoded.Data))
}

// TestEncoderBatchProcessing tests efficient batch processing with encoder reuse
func TestEncoderBatchProcessing(t *testing.T) {
	files := []string{"red.png", "blue.png"}

	encoder, err := NewWebPEncoder(func(opts *WebPEncodeOptions) {
		opts.Quality = 75
		opts.Method = 4
	})
	if err != nil {
		t.Fatalf("Failed to create encoder: %v", err)
	}
	defer encoder.Close()

	var totalSize int
	for _, filename := range files {
		data, err := os.ReadFile(filepath.Join("..", "testdata", "png", filename))
		if err != nil {
			t.Fatalf("Failed to read %s: %v", filename, err)
		}

		webp, err := encoder.Encode(data)
		if err != nil {
			t.Fatalf("Failed to encode %s: %v", filename, err)
		}

		totalSize += len(webp)
		t.Logf("  %s: %d bytes → %d bytes", filename, len(data), len(webp))
	}

	t.Logf("✓ Batch processing complete: %d images, total %d bytes", len(files), totalSize)
}

// TestAVIFLossless_toCEncodeOptions tests that Lossless flag correctly overrides C options
func TestAVIFLossless_toCEncodeOptions(t *testing.T) {
	t.Run("lossless-overrides-quality-and-format", func(t *testing.T) {
		opts := DefaultAVIFEncodeOptions()
		opts.Lossless = true
		// Intentionally set values that should be overridden
		opts.Quality = 30
		opts.QualityAlpha = 50
		opts.YUVFormat = YUVFormat420
		opts.YUVRange = YUVRangeLimited
		opts.MatrixCoefficients = 6 // BT601

		copts := opts.toCEncodeOptions()

		if int(copts.quality) != 100 {
			t.Errorf("quality: got %d, want 100", int(copts.quality))
		}
		if int(copts.quality_alpha) != 100 {
			t.Errorf("quality_alpha: got %d, want 100", int(copts.quality_alpha))
		}
		if int(copts.matrix_coefficients) != 0 {
			t.Errorf("matrix_coefficients: got %d, want 0 (Identity)", int(copts.matrix_coefficients))
		}
		if int(copts.yuv_format) != 0 {
			t.Errorf("yuv_format: got %d, want 0 (444)", int(copts.yuv_format))
		}
		if int(copts.yuv_range) != 1 {
			t.Errorf("yuv_range: got %d, want 1 (Full)", int(copts.yuv_range))
		}
	})

	t.Run("lossless-preserves-speed", func(t *testing.T) {
		opts := DefaultAVIFEncodeOptions()
		opts.Lossless = true
		opts.Speed = 3

		copts := opts.toCEncodeOptions()

		if int(copts.speed) != 3 {
			t.Errorf("speed: got %d, want 3", int(copts.speed))
		}
	})

	t.Run("lossless-preserves-other-settings", func(t *testing.T) {
		opts := DefaultAVIFEncodeOptions()
		opts.Lossless = true
		opts.BitDepth = 10
		opts.EnableAlpha = true
		opts.ColorPrimaries = 1
		opts.TransferCharacteristics = 13
		opts.TileRowsLog2 = 2
		opts.TileColsLog2 = 3

		copts := opts.toCEncodeOptions()

		if int(copts.bit_depth) != 10 {
			t.Errorf("bit_depth: got %d, want 10", int(copts.bit_depth))
		}
		if int(copts.enable_alpha) != 1 {
			t.Errorf("enable_alpha: got %d, want 1", int(copts.enable_alpha))
		}
		if int(copts.color_primaries) != 1 {
			t.Errorf("color_primaries: got %d, want 1", int(copts.color_primaries))
		}
		if int(copts.transfer_characteristics) != 13 {
			t.Errorf("transfer_characteristics: got %d, want 13", int(copts.transfer_characteristics))
		}
		if int(copts.tile_rows_log2) != 2 {
			t.Errorf("tile_rows_log2: got %d, want 2", int(copts.tile_rows_log2))
		}
		if int(copts.tile_cols_log2) != 3 {
			t.Errorf("tile_cols_log2: got %d, want 3", int(copts.tile_cols_log2))
		}
	})

	t.Run("non-lossless-preserves-user-values", func(t *testing.T) {
		opts := DefaultAVIFEncodeOptions()
		opts.Lossless = false
		opts.Quality = 42
		opts.QualityAlpha = 85
		opts.YUVFormat = YUVFormat420
		opts.YUVRange = YUVRangeLimited
		opts.MatrixCoefficients = 6

		copts := opts.toCEncodeOptions()

		if int(copts.quality) != 42 {
			t.Errorf("quality: got %d, want 42", int(copts.quality))
		}
		if int(copts.quality_alpha) != 85 {
			t.Errorf("quality_alpha: got %d, want 85", int(copts.quality_alpha))
		}
		if int(copts.matrix_coefficients) != 6 {
			t.Errorf("matrix_coefficients: got %d, want 6", int(copts.matrix_coefficients))
		}
		if int(copts.yuv_format) != 2 {
			t.Errorf("yuv_format: got %d, want 2 (420)", int(copts.yuv_format))
		}
		if int(copts.yuv_range) != 0 {
			t.Errorf("yuv_range: got %d, want 0 (Limited)", int(copts.yuv_range))
		}
	})
}

// TestAVIFLossless_Encode tests lossless encoding produces valid AVIF
func TestAVIFLossless_Encode(t *testing.T) {
	pngData, err := os.ReadFile(filepath.Join("..", "testdata", "png", "red.png"))
	if err != nil {
		t.Fatalf("Failed to read test image: %v", err)
	}

	t.Run("lossless-via-AVIFEncodeBytes", func(t *testing.T) {
		opts := DefaultAVIFEncodeOptions()
		opts.Lossless = true

		avifData, err := AVIFEncodeBytes(pngData, opts)
		if err != nil {
			t.Fatalf("lossless encoding failed: %v", err)
		}
		if len(avifData) == 0 {
			t.Fatal("lossless encoding produced empty output")
		}
		t.Logf("lossless AVIF: %d bytes (from %d bytes PNG)", len(avifData), len(pngData))
	})

	t.Run("lossless-via-AVIFEncoder", func(t *testing.T) {
		encoder, err := NewAVIFEncoder(func(opts *AVIFEncodeOptions) {
			opts.Lossless = true
		})
		if err != nil {
			t.Fatalf("Failed to create lossless encoder: %v", err)
		}
		defer encoder.Close()

		avifData, err := encoder.Encode(pngData)
		if err != nil {
			t.Fatalf("lossless encoding failed: %v", err)
		}
		if len(avifData) == 0 {
			t.Fatal("lossless encoding produced empty output")
		}
		t.Logf("lossless AVIF (encoder): %d bytes", len(avifData))
	})

	t.Run("lossless-via-AVIFEncCommand", func(t *testing.T) {
		encOpts := NewDefaultAVIFEncOptions()
		// Simulate Lossless via manual settings (AVIFEncOptions has no Lossless field)
		encOpts.Quality = 100
		encOpts.QualityAlpha = 100
		encOpts.MatrixCoefficients = 0
		encOpts.YUVFormat = 0  // 444
		encOpts.YUVRange = 1   // Full

		cmd, err := NewAVIFEncCommand(&encOpts)
		if err != nil {
			t.Fatalf("Failed to create command: %v", err)
		}
		defer cmd.Close()

		avifData, err := cmd.Run(pngData)
		if err != nil {
			t.Fatalf("lossless encoding failed: %v", err)
		}
		if len(avifData) == 0 {
			t.Fatal("lossless encoding produced empty output")
		}
		t.Logf("lossless AVIF (command): %d bytes", len(avifData))
	})
}

// extractNRGBA extracts non-premultiplied RGBA pixels from a Go image.
// AVIF stores non-premultiplied alpha, so we need raw NRGBA values for comparison.
func extractNRGBA(img image.Image) []byte {
	bounds := img.Bounds()
	w, h := bounds.Dx(), bounds.Dy()
	pixels := make([]byte, w*h*4)

	// Fast path for NRGBA images (most PNGs with alpha)
	if nrgba, ok := img.(*image.NRGBA); ok {
		for y := 0; y < h; y++ {
			for x := 0; x < w; x++ {
				c := nrgba.NRGBAAt(bounds.Min.X+x, bounds.Min.Y+y)
				idx := (y*w + x) * 4
				pixels[idx+0] = c.R
				pixels[idx+1] = c.G
				pixels[idx+2] = c.B
				pixels[idx+3] = c.A
			}
		}
		return pixels
	}

	// Generic path: convert via color.NRGBAModel to avoid premultiply rounding errors
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			c := color.NRGBAModel.Convert(img.At(bounds.Min.X+x, bounds.Min.Y+y)).(color.NRGBA)
			idx := (y*w + x) * 4
			pixels[idx+0] = c.R
			pixels[idx+1] = c.G
			pixels[idx+2] = c.B
			pixels[idx+3] = c.A
		}
	}
	return pixels
}

// TestAVIFLossless_RoundTrip tests that lossless encode → decode is pixel-exact
func TestAVIFLossless_RoundTrip(t *testing.T) {
	testFiles := []string{
		filepath.Join("..", "testdata", "png", "red.png"),
		filepath.Join("..", "testdata", "png", "blue.png"),
	}

	for _, inputPath := range testFiles {
		baseName := filepath.Base(inputPath)
		t.Run(baseName, func(t *testing.T) {
			pngData, err := os.ReadFile(inputPath)
			if err != nil {
				t.Fatalf("Failed to read test image: %v", err)
			}

			// Decode original PNG with Go standard library to get reference NRGBA pixels
			img, err := png.Decode(bytes.NewReader(pngData))
			if err != nil {
				t.Fatalf("Failed to decode original PNG: %v", err)
			}
			bounds := img.Bounds()
			origWidth := bounds.Dx()
			origHeight := bounds.Dy()
			origPixels := extractNRGBA(img)

			// Lossless encode
			encOpts := DefaultAVIFEncodeOptions()
			encOpts.Lossless = true
			avifData, err := AVIFEncodeBytes(pngData, encOpts)
			if err != nil {
				t.Fatalf("lossless encode failed: %v", err)
			}

			// Decode the lossless AVIF
			decOpts := DefaultAVIFDecodeOptions()
			decOpts.Format = FormatRGBA
			decoded, err := AVIFDecodeBytes(avifData, decOpts)
			if err != nil {
				t.Fatalf("lossless decode failed: %v", err)
			}

			// Verify dimensions match
			if decoded.Width != origWidth || decoded.Height != origHeight {
				t.Fatalf("dimensions mismatch: orig=%dx%d, decoded=%dx%d",
					origWidth, origHeight, decoded.Width, decoded.Height)
			}

			// Extract pixel data without stride padding
			decodedPixels := make([]byte, decoded.Width*decoded.Height*4)
			for y := range decoded.Height {
				srcOffset := y * decoded.Stride
				dstOffset := y * decoded.Width * 4
				copy(decodedPixels[dstOffset:dstOffset+decoded.Width*4],
					decoded.Data[srcOffset:srcOffset+decoded.Width*4])
			}

			// Verify pixel-exact match
			mismatchCount := 0
			for i := range decodedPixels {
				if i < len(origPixels) && decodedPixels[i] != origPixels[i] {
					mismatchCount++
				}
			}

			if mismatchCount > 0 {
				totalBytes := len(decodedPixels)
				mismatchPercent := float64(mismatchCount) / float64(totalBytes) * 100.0
				t.Errorf("pixel mismatch: %d/%d bytes differ (%.2f%%) in %dx%d image",
					mismatchCount, totalBytes, mismatchPercent,
					decoded.Width, decoded.Height)
			} else {
				t.Logf("pixel-exact roundtrip: %dx%d, %d bytes",
					decoded.Width, decoded.Height, len(decodedPixels))
			}
		})
	}
}

// TestAVIFLossless_Deterministic tests that encoding the same input twice
// produces identical decoded pixels (self-consistency test).
// This avoids comparing against Go's PNG decoder which may interpret alpha differently.
func TestAVIFLossless_Deterministic(t *testing.T) {
	testFiles := []struct {
		name string
		path string
	}{
		{"solid-red", filepath.Join("..", "testdata", "source", "colors", "solid-red.png")},
		{"gradient", filepath.Join("..", "testdata", "source", "colors", "gradient-horizontal.png")},
		{"alpha-circle", filepath.Join("..", "testdata", "source", "alpha", "alpha-circle.png")},
		{"alpha-gradient", filepath.Join("..", "testdata", "source", "alpha", "alpha-gradient.png")},
		{"checkerboard", filepath.Join("..", "testdata", "source", "colors", "checkerboard.png")},
	}

	for _, tc := range testFiles {
		t.Run(tc.name, func(t *testing.T) {
			pngData, err := os.ReadFile(tc.path)
			if err != nil {
				t.Skipf("test image not found: %v", err)
			}

			encOpts := DefaultAVIFEncodeOptions()
			encOpts.Lossless = true
			decOpts := DefaultAVIFDecodeOptions()
			decOpts.Format = FormatRGBA

			// Encode twice
			avifData1, err := AVIFEncodeBytes(pngData, encOpts)
			if err != nil {
				t.Fatalf("first encode failed: %v", err)
			}
			avifData2, err := AVIFEncodeBytes(pngData, encOpts)
			if err != nil {
				t.Fatalf("second encode failed: %v", err)
			}

			// Decode both
			decoded1, err := AVIFDecodeBytes(avifData1, decOpts)
			if err != nil {
				t.Fatalf("first decode failed: %v", err)
			}
			decoded2, err := AVIFDecodeBytes(avifData2, decOpts)
			if err != nil {
				t.Fatalf("second decode failed: %v", err)
			}

			// Dimensions must match
			if decoded1.Width != decoded2.Width || decoded1.Height != decoded2.Height {
				t.Fatalf("dimensions differ: %dx%d vs %dx%d",
					decoded1.Width, decoded1.Height, decoded2.Width, decoded2.Height)
			}

			// Extract pixels
			extractPixels := func(d *DecodedImage) []byte {
				pixels := make([]byte, d.Width*d.Height*4)
				for y := range d.Height {
					srcOff := y * d.Stride
					dstOff := y * d.Width * 4
					copy(pixels[dstOff:dstOff+d.Width*4], d.Data[srcOff:srcOff+d.Width*4])
				}
				return pixels
			}
			pixels1 := extractPixels(decoded1)
			pixels2 := extractPixels(decoded2)

			mismatchCount := 0
			for i := range pixels1 {
				if pixels1[i] != pixels2[i] {
					mismatchCount++
				}
			}

			if mismatchCount > 0 {
				t.Errorf("non-deterministic: %d/%d bytes differ between two encodes", mismatchCount, len(pixels1))
			} else {
				t.Logf("deterministic lossless: %dx%d, %d bytes AVIF",
					decoded1.Width, decoded1.Height, len(avifData1))
			}
		})
	}
}

// TestAVIFLossless_LargerThanLossy verifies lossless output is larger than lossy
func TestAVIFLossless_LargerThanLossy(t *testing.T) {
	inputPath := filepath.Join("..", "testdata", "source", "sizes", "medium-512x512.png")
	pngData, err := os.ReadFile(inputPath)
	if err != nil {
		t.Skipf("test image not found: %v", err)
	}

	// Lossy encode (quality=60)
	lossyOpts := DefaultAVIFEncodeOptions()
	lossyOpts.Quality = 60
	lossyData, err := AVIFEncodeBytes(pngData, lossyOpts)
	if err != nil {
		t.Fatalf("lossy encoding failed: %v", err)
	}

	// Lossless encode
	losslessOpts := DefaultAVIFEncodeOptions()
	losslessOpts.Lossless = true
	losslessData, err := AVIFEncodeBytes(pngData, losslessOpts)
	if err != nil {
		t.Fatalf("lossless encoding failed: %v", err)
	}

	t.Logf("lossy (q=60): %d bytes, lossless: %d bytes", len(lossyData), len(losslessData))

	if len(losslessData) <= len(lossyData) {
		t.Errorf("lossless (%d bytes) should be larger than lossy q=60 (%d bytes)",
			len(losslessData), len(lossyData))
	}
}

// TestAVIFLossless_VariousInputTypes tests lossless encoding with different image types
func TestAVIFLossless_VariousInputTypes(t *testing.T) {
	testFiles := []struct {
		name string
		path string
	}{
		{"solid-color", filepath.Join("..", "testdata", "source", "colors", "solid-red.png")},
		{"gradient", filepath.Join("..", "testdata", "source", "colors", "gradient-horizontal.png")},
		{"checkerboard", filepath.Join("..", "testdata", "source", "colors", "checkerboard.png")},
		{"alpha-transparent", filepath.Join("..", "testdata", "source", "alpha", "transparent.png")},
		{"alpha-gradient", filepath.Join("..", "testdata", "source", "alpha", "alpha-gradient.png")},
		{"tiny-16x16", filepath.Join("..", "testdata", "source", "sizes", "tiny-16x16.png")},
		{"edges", filepath.Join("..", "testdata", "source", "compression", "edges.png")},
	}

	for _, tc := range testFiles {
		t.Run(tc.name, func(t *testing.T) {
			pngData, err := os.ReadFile(tc.path)
			if err != nil {
				t.Skipf("test image not found: %v", err)
			}

			opts := DefaultAVIFEncodeOptions()
			opts.Lossless = true

			avifData, err := AVIFEncodeBytes(pngData, opts)
			if err != nil {
				t.Fatalf("lossless encoding failed for %s: %v", tc.name, err)
			}

			if len(avifData) == 0 {
				t.Fatalf("lossless encoding produced empty output for %s", tc.name)
			}

			// Verify the output can be decoded
			decOpts := DefaultAVIFDecodeOptions()
			decoded, err := AVIFDecodeBytes(avifData, decOpts)
			if err != nil {
				t.Fatalf("failed to decode lossless AVIF for %s: %v", tc.name, err)
			}

			if decoded.Width <= 0 || decoded.Height <= 0 {
				t.Fatalf("invalid dimensions for %s: %dx%d", tc.name, decoded.Width, decoded.Height)
			}

			t.Logf("%s: %d bytes PNG → %d bytes AVIF lossless (%dx%d)",
				tc.name, len(pngData), len(avifData), decoded.Width, decoded.Height)
		})
	}
}

// TestAVIFLossless_EncoderReuse tests that lossless encoder can be reused for multiple images
func TestAVIFLossless_EncoderReuse(t *testing.T) {
	encoder, err := NewAVIFEncoder(func(opts *AVIFEncodeOptions) {
		opts.Lossless = true
		opts.Speed = 8
	})
	if err != nil {
		t.Fatalf("Failed to create lossless encoder: %v", err)
	}
	defer encoder.Close()

	files := []string{"red.png", "blue.png"}
	for _, filename := range files {
		data, err := os.ReadFile(filepath.Join("..", "testdata", "png", filename))
		if err != nil {
			t.Fatalf("Failed to read %s: %v", filename, err)
		}

		avifData, err := encoder.Encode(data)
		if err != nil {
			t.Fatalf("Failed to encode %s: %v", filename, err)
		}

		if len(avifData) == 0 {
			t.Fatalf("Empty output for %s", filename)
		}

		t.Logf("  %s: %d bytes → %d bytes (lossless)", filename, len(data), len(avifData))
	}
}
