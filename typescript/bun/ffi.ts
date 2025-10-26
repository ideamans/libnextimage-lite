/**
 * Bun FFI bindings for libnextimage
 * Uses bun:ffi
 */

import { dlopen, FFIType, type Pointer, ptr, CString } from 'bun:ffi'
import { getLibraryPath } from './library.ts'

// Type definitions
export interface NextImageBuffer {
  data: Pointer
  size: number
}

export interface NextImageDecodeBuffer {
  data: Pointer
  size: number
  width: number
  height: number
  format: number
}

// FFI symbols definition
const symbols = {
  // WebP Encoder
  nextimage_webp_encoder_create: {
    args: [FFIType.ptr],
    returns: FFIType.ptr
  },
  nextimage_webp_encoder_encode: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr],
    returns: FFIType.i32
  },
  nextimage_webp_encoder_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },

  // WebP Decoder
  nextimage_webp_decoder_create: {
    args: [FFIType.ptr],
    returns: FFIType.ptr
  },
  nextimage_webp_decoder_decode: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr],
    returns: FFIType.i32
  },
  nextimage_webp_decoder_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },

  // AVIF Encoder
  nextimage_avif_encoder_create: {
    args: [FFIType.ptr],
    returns: FFIType.ptr
  },
  nextimage_avif_encoder_encode: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr],
    returns: FFIType.i32
  },
  nextimage_avif_encoder_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },

  // AVIF Decoder
  nextimage_avif_decoder_create: {
    args: [FFIType.ptr],
    returns: FFIType.ptr
  },
  nextimage_avif_decoder_decode: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64, FFIType.ptr],
    returns: FFIType.i32
  },
  nextimage_avif_decoder_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },

  // Memory management
  nextimage_free_buffer: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },
  nextimage_free_decode_buffer: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },

  // Options helpers
  nextimage_webp_default_encode_options: {
    args: [FFIType.ptr],
    returns: FFIType.void
  },
  nextimage_avif_default_encode_options: {
    args: [FFIType.ptr],
    returns: FFIType.void
  }
} as const

// Load the dynamic library
let lib: ReturnType<typeof dlopen> | null = null

export function getLibrary() {
  if (!lib) {
    const libPath = getLibraryPath()
    lib = dlopen(libPath, symbols)
  }
  return lib
}

/**
 * Helper function to create a buffer struct
 */
export function createBufferStruct(): { buffer: ArrayBuffer; ptr: Pointer } {
  // NextImageBuffer struct: { data: *u8, size: usize }
  // On 64-bit: 8 bytes (pointer) + 8 bytes (size) = 16 bytes
  const buffer = new ArrayBuffer(16)
  return { buffer, ptr: ptr(buffer) }
}

/**
 * Helper function to read NextImageBuffer from memory
 */
export function readBufferStruct(buffer: ArrayBuffer): NextImageBuffer {
  const view = new DataView(buffer)

  // Read pointer (8 bytes on 64-bit)
  const dataLow = view.getUint32(0, true)
  const dataHigh = view.getUint32(4, true)
  const data = ptr(BigInt(dataLow) | (BigInt(dataHigh) << 32n))

  // Read size (8 bytes on 64-bit)
  const sizeLow = view.getUint32(8, true)
  const sizeHigh = view.getUint32(12, true)
  const size = Number(BigInt(sizeLow) | (BigInt(sizeHigh) << 32n))

  return { data, size }
}

/**
 * Helper function to create a decode buffer struct
 */
export function createDecodeBufferStruct(): { buffer: ArrayBuffer; ptr: Pointer } {
  // NextImageDecodeBuffer struct: { data: *u8, size: usize, width: u32, height: u32, format: i32 }
  // 8 + 8 + 4 + 4 + 4 = 28 bytes (+ padding to 32)
  const buffer = new ArrayBuffer(32)
  return { buffer, ptr: ptr(buffer) }
}

/**
 * Helper function to read NextImageDecodeBuffer from memory
 */
export function readDecodeBufferStruct(buffer: ArrayBuffer): NextImageDecodeBuffer {
  const view = new DataView(buffer)

  // Read pointer (8 bytes)
  const dataLow = view.getUint32(0, true)
  const dataHigh = view.getUint32(4, true)
  const data = ptr(BigInt(dataLow) | (BigInt(dataHigh) << 32n))

  // Read size (8 bytes)
  const sizeLow = view.getUint32(8, true)
  const sizeHigh = view.getUint32(12, true)
  const size = Number(BigInt(sizeLow) | (BigInt(sizeHigh) << 32n))

  // Read width, height, format (4 bytes each)
  const width = view.getUint32(16, true)
  const height = view.getUint32(20, true)
  const format = view.getInt32(24, true)

  return { data, size, width, height, format }
}

/**
 * Helper function to copy data from C memory to Buffer
 */
export function copyFromCMemory(pointer: Pointer, size: number): Buffer {
  if (!pointer || size === 0) {
    return Buffer.alloc(0)
  }

  // Bun's FFI: read bytes from pointer using DataView
  // Create a DataView from the pointer and copy byte by byte
  const buffer = Buffer.alloc(size)

  // Use Bun's toArrayBuffer with pointer
  // Need to cast pointer to number first
  const ptrValue = Number(pointer)

  // Read using Bun.peek which can read from raw memory addresses
  const bytes = new Uint8Array(Bun.peek(ptrValue, size))
  buffer.set(bytes)

  return buffer
}

/**
 * Create a WebP encode options struct and initialize with defaults
 */
export function createWebPOptionsStruct(): { buffer: ArrayBuffer; ptr: Pointer } {
  // Based on NextImageWebPEncodeOptions struct - approximately 40 int fields + 2 floats
  // Size estimate: 40 * 4 (int) + 2 * 4 (float) = 168 bytes, round up to 256 for safety
  const buffer = new ArrayBuffer(256)
  const pointer = ptr(buffer)

  // Initialize with default values
  const lib = getLibrary()
  lib.symbols.nextimage_webp_default_encode_options(pointer)

  return { buffer, ptr: pointer }
}

/**
 * Read WebP options from struct buffer
 */
export function readWebPOptionsStruct(buffer: ArrayBuffer): {
  quality: number
  lossless: number
  method: number
  preset: number
  image_hint: number
  exact: number
} {
  const view = new DataView(buffer)

  // Field offsets based on NextImageWebPEncodeOptions struct
  // float quality, int lossless, int method, int preset, int image_hint, ...
  return {
    quality: view.getFloat32(0, true),      // offset 0
    lossless: view.getInt32(4, true),       // offset 4
    method: view.getInt32(8, true),         // offset 8
    preset: view.getInt32(12, true),        // offset 12
    image_hint: view.getInt32(16, true),    // offset 16
    exact: view.getInt32(100, true)         // offset 100 (approximate, needs verification)
  }
}

/**
 * Write WebP options to struct buffer
 */
export function writeWebPOptionsStruct(
  buffer: ArrayBuffer,
  opts: {
    quality?: number
    lossless?: number | boolean
    method?: number
    preset?: number
    image_hint?: number
    exact?: number | boolean
  }
): void {
  const view = new DataView(buffer)

  if (opts.quality !== undefined) {
    view.setFloat32(0, opts.quality, true)
  }
  if (opts.lossless !== undefined) {
    view.setInt32(4, typeof opts.lossless === 'boolean' ? (opts.lossless ? 1 : 0) : opts.lossless, true)
  }
  if (opts.method !== undefined) {
    view.setInt32(8, opts.method, true)
  }
  if (opts.preset !== undefined) {
    view.setInt32(12, opts.preset, true)
  }
  if (opts.image_hint !== undefined) {
    view.setInt32(16, opts.image_hint, true)
  }
  if (opts.exact !== undefined) {
    view.setInt32(100, typeof opts.exact === 'boolean' ? (opts.exact ? 1 : 0) : opts.exact, true)
  }
}

