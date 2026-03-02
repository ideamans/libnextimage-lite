/**
 * libnextimage Light API v2 - Simplified image conversion
 *
 * Provides 4 unified conversion functions with automatic format detection:
 * - legacyToWebp: JPEG/PNG/GIF -> WebP
 * - webpToLegacy: WebP -> JPEG/PNG/GIF (auto-detected)
 * - legacyToAvif: JPEG/PNG -> AVIF
 * - avifToLegacy: AVIF -> JPEG/PNG (auto-detected)
 *
 * Each function internally delegates to the appropriate low-level command
 * (cwebp, dwebp, gif2webp, webp2gif, avifenc, avifdec) based on format detection.
 */

import koffi from 'koffi'
import {
  nextimage_light_legacy_to_webp,
  nextimage_light_webp_to_legacy,
  nextimage_light_legacy_to_avif,
  nextimage_light_avif_to_legacy,
  nextimage_light_free,
} from './ffi'
import { NextImageStatus, NextImageError } from './types'

/** Input for all conversion functions */
export interface ConvertInput {
  /** Input image data */
  data: Buffer
  /** Quality: -1 or omit for default, 0-100 for explicit quality */
  quality?: number
  /** AVIF lossy min quantizer: -1 or omit for default, 0-63 (0=best quality) */
  minQuantizer?: number
  /** AVIF lossy max quantizer: -1 or omit for default, 0-63 (63=worst quality) */
  maxQuantizer?: number
}

/** Output from all conversion functions */
export interface ConvertOutput {
  /** Output image data */
  data: Buffer
  /** Output MIME type (e.g. "image/webp", "image/jpeg", "image/png", "image/gif") */
  mimeType: string
}

function runConversion(
  cfunc: (input: any, output: any) => number,
  input: ConvertInput,
  opName: string,
): ConvertOutput {
  if (!input.data || input.data.length === 0) {
    throw new NextImageError(NextImageStatus.ERROR_INVALID_PARAM, `${opName}: empty input data`)
  }

  const cInput = {
    data: input.data,
    size: input.data.length,
    quality: input.quality ?? -1,
    min_quantizer: input.minQuantizer ?? -1,
    max_quantizer: input.maxQuantizer ?? -1,
  }

  const cOutput = {
    status: 0,
    data: null as any,
    size: 0,
    mime_type: Buffer.alloc(32),
  }

  const status = cfunc(cInput, cOutput)

  if (status !== NextImageStatus.OK) {
    throw new NextImageError(status, `${opName} failed with status ${status}`)
  }

  if (!cOutput.data || cOutput.size === 0) {
    throw new NextImageError(NextImageStatus.ERROR_ENCODE_FAILED, `${opName}: produced empty output`)
  }

  // Copy data to a Buffer
  const result = Buffer.from(koffi.decode(cOutput.data, 'uint8_t', cOutput.size) as Uint8Array)

  // Extract MIME type from char[32] array
  const mimeBytes = cOutput.mime_type as unknown
  let mimeType: string
  if (Buffer.isBuffer(mimeBytes)) {
    const nullIdx = (mimeBytes as Buffer).indexOf(0)
    mimeType = (mimeBytes as Buffer).toString('utf8', 0, nullIdx >= 0 ? nullIdx : 32)
  } else if (Array.isArray(mimeBytes)) {
    const nullIdx = (mimeBytes as number[]).indexOf(0)
    mimeType = String.fromCharCode(...(mimeBytes as number[]).slice(0, nullIdx >= 0 ? nullIdx : 32))
  } else {
    mimeType = String(mimeBytes).replace(/\0.*$/, '')
  }

  // Free C buffer
  nextimage_light_free(cOutput)

  return { data: result, mimeType }
}

/**
 * Convert legacy format (JPEG/PNG/GIF) to WebP.
 * JPEG: lossy (quality default=75), PNG: lossless, GIF: gif2webp default.
 */
export function legacyToWebp(input: ConvertInput): ConvertOutput {
  return runConversion(nextimage_light_legacy_to_webp, input, 'legacy_to_webp')
}

/**
 * Convert WebP to the appropriate legacy format (auto-detected).
 * Animated WebP -> GIF, Lossless WebP -> PNG, Lossy WebP -> JPEG (quality default=90).
 */
export function webpToLegacy(input: ConvertInput): ConvertOutput {
  return runConversion(nextimage_light_webp_to_legacy, input, 'webp_to_legacy')
}

/**
 * Convert legacy format (JPEG/PNG) to AVIF.
 * JPEG: lossy (quality/quantizer params), PNG: lossless high-precision.
 */
export function legacyToAvif(input: ConvertInput): ConvertOutput {
  return runConversion(nextimage_light_legacy_to_avif, input, 'legacy_to_avif')
}

/**
 * Convert AVIF to the appropriate legacy format (auto-detected).
 * Lossless AVIF -> PNG, Lossy AVIF -> JPEG (quality default=90).
 */
export function avifToLegacy(input: ConvertInput): ConvertOutput {
  return runConversion(nextimage_light_avif_to_legacy, input, 'avif_to_legacy')
}
