// Package libnextimage provides simplified Go bindings for libnextimage.
//
// This package provides 4 unified image conversion functions with automatic format detection:
//   - LegacyToWebP: JPEG/PNG/GIF -> WebP
//   - WebPToLegacy: WebP -> JPEG/PNG/GIF (auto-detected)
//   - LegacyToAvif: JPEG/PNG -> AVIF
//   - AvifToLegacy: AVIF -> JPEG/PNG (auto-detected)
package libnextimage

/*
#cgo CFLAGS: -I${SRCDIR}/shared/include

// libnextimage.a is a fully self-contained static library that includes:
// - webp, avif, aom (image codecs)
// - jpeg, png, gif (system image libraries)
//
// Only minimal system libraries are needed:
// - zlib: compression (required by PNG)
// - C++ standard library: libavif and libaom are written in C++
// - pthread: multi-threading support
// - math library: mathematical functions

// Platform-specific embedded static libraries
#cgo darwin,arm64 LDFLAGS: ${SRCDIR}/shared/lib/darwin-arm64/libnextimage.a
#cgo darwin,amd64 LDFLAGS: ${SRCDIR}/shared/lib/darwin-amd64/libnextimage.a
#cgo linux,amd64 LDFLAGS: ${SRCDIR}/shared/lib/linux-amd64/libnextimage.a
#cgo linux,arm64 LDFLAGS: ${SRCDIR}/shared/lib/linux-arm64/libnextimage.a
#cgo windows,amd64 LDFLAGS: ${SRCDIR}/shared/lib/windows-amd64/libnextimage.a

// macOS
#cgo darwin LDFLAGS: -lz -lc++ -lpthread -lm

// Linux
#cgo linux LDFLAGS: -lz -lstdc++ -lpthread -lm

// Windows (MSYS2/MinGW)
#cgo windows LDFLAGS: -lz -lstdc++ -lpthread -lm

#include <stdlib.h>
#include <string.h>
#include "nextimage.h"
#include "nextimage_light.h"
*/
import "C"
import (
	"fmt"
	"runtime"
	"unsafe"
)

// ConvertInput represents input for all conversion functions.
type ConvertInput struct {
	Data         []byte // input image data
	Quality      int    // -1=use default, 0-100=explicit quality
	MinQuantizer int    // -1=use default, 0-63 (AVIF lossy min quantizer, 0=best quality)
	MaxQuantizer int    // -1=use default, 0-63 (AVIF lossy max quantizer, 63=worst quality)
}

// ConvertOutput represents output from all conversion functions.
type ConvertOutput struct {
	Data     []byte // output image data
	MimeType string // output MIME type (e.g. "image/webp", "image/jpeg", "image/png", "image/gif")
	Error    error  // nil on success
}

// callLight is a helper that sets up C structs, calls a C function, and collects results.
func callLight(
	input ConvertInput,
	opName string,
	cfunc func(*C.NextImageLightInput, *C.NextImageLightOutput) C.NextImageStatus,
) ConvertOutput {
	if len(input.Data) == 0 {
		return ConvertOutput{Error: fmt.Errorf("%s: empty input data", opName)}
	}

	// Pin the Go slice data so CGO allows passing it through the C struct
	var pinner runtime.Pinner
	pinner.Pin(&input.Data[0])
	defer pinner.Unpin()

	cInput := C.NextImageLightInput{
		data:          (*C.uint8_t)(unsafe.Pointer(&input.Data[0])),
		size:          C.size_t(len(input.Data)),
		quality:       C.int(input.Quality),
		min_quantizer: C.int(input.MinQuantizer),
		max_quantizer: C.int(input.MaxQuantizer),
	}

	var cOutput C.NextImageLightOutput

	status := cfunc(&cInput, &cOutput)

	if status != C.NEXTIMAGE_OK {
		errMsg := C.nextimage_last_error_message()
		if errMsg != nil {
			return ConvertOutput{Error: fmt.Errorf("%s: %s", opName, C.GoString(errMsg))}
		}
		return ConvertOutput{Error: fmt.Errorf("%s failed with status %d", opName, int(status))}
	}

	if cOutput.data == nil || cOutput.size == 0 {
		return ConvertOutput{Error: fmt.Errorf("%s: produced empty output", opName)}
	}

	result := C.GoBytes(unsafe.Pointer(cOutput.data), C.int(cOutput.size))
	mimeType := C.GoString(&cOutput.mime_type[0])
	C.nextimage_light_free(&cOutput)

	return ConvertOutput{Data: result, MimeType: mimeType}
}

// LegacyToWebP converts JPEG/PNG/GIF image data to WebP format.
// JPEG: lossy (quality default=75), PNG: lossless, GIF: gif2webp default.
func LegacyToWebP(input ConvertInput) ConvertOutput {
	return callLight(input, "legacy_to_webp", func(in *C.NextImageLightInput, out *C.NextImageLightOutput) C.NextImageStatus {
		return C.nextimage_light_legacy_to_webp(in, out)
	})
}

// WebPToLegacy converts WebP image data to the appropriate legacy format.
// Animated WebP -> GIF, Lossless WebP -> PNG, Lossy WebP -> JPEG (quality default=90).
func WebPToLegacy(input ConvertInput) ConvertOutput {
	return callLight(input, "webp_to_legacy", func(in *C.NextImageLightInput, out *C.NextImageLightOutput) C.NextImageStatus {
		return C.nextimage_light_webp_to_legacy(in, out)
	})
}

// LegacyToAvif converts JPEG/PNG image data to AVIF format.
// JPEG: lossy (quality default=60, or use MinQuantizer/MaxQuantizer), PNG: lossless high-precision.
func LegacyToAvif(input ConvertInput) ConvertOutput {
	return callLight(input, "legacy_to_avif", func(in *C.NextImageLightInput, out *C.NextImageLightOutput) C.NextImageStatus {
		return C.nextimage_light_legacy_to_avif(in, out)
	})
}

// AvifToLegacy converts AVIF image data to the appropriate legacy format.
// Lossless AVIF -> PNG, Lossy AVIF -> JPEG (quality default=90).
func AvifToLegacy(input ConvertInput) ConvertOutput {
	return callLight(input, "avif_to_legacy", func(in *C.NextImageLightInput, out *C.NextImageLightOutput) C.NextImageStatus {
		return C.nextimage_light_avif_to_legacy(in, out)
	})
}

// Version returns the library version string.
func Version() string {
	return C.GoString(C.nextimage_version())
}
