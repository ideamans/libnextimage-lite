/**
 * Deno FFI bindings for libnextimage
 * Uses Deno.dlopen() instead of Koffi
 */

import { getLibraryPath } from './library.ts'

// Type definitions matching the C API
export interface NextImageBuffer {
  data: Deno.PointerValue
  size: number
}

export interface NextImageDecodeBuffer {
  data: Deno.PointerValue
  size: number
  width: number
  height: number
  format: number
}

// FFI symbols definition
const symbols = {
  // WebP Encoder
  nextimage_webp_encoder_create: {
    parameters: ['pointer'],
    result: 'pointer'
  },
  nextimage_webp_encoder_encode: {
    parameters: ['pointer', 'buffer', 'usize', 'pointer'],
    result: 'i32'
  },
  nextimage_webp_encoder_destroy: {
    parameters: ['pointer'],
    result: 'void'
  },

  // WebP Decoder
  nextimage_webp_decoder_create: {
    parameters: ['pointer'],
    result: 'pointer'
  },
  nextimage_webp_decoder_decode: {
    parameters: ['pointer', 'buffer', 'usize', 'pointer'],
    result: 'i32'
  },
  nextimage_webp_decoder_destroy: {
    parameters: ['pointer'],
    result: 'void'
  },

  // AVIF Encoder
  nextimage_avif_encoder_create: {
    parameters: ['pointer'],
    result: 'pointer'
  },
  nextimage_avif_encoder_encode: {
    parameters: ['pointer', 'buffer', 'usize', 'pointer'],
    result: 'i32'
  },
  nextimage_avif_encoder_destroy: {
    parameters: ['pointer'],
    result: 'void'
  },

  // AVIF Decoder
  nextimage_avif_decoder_create: {
    parameters: ['pointer'],
    result: 'pointer'
  },
  nextimage_avif_decoder_decode: {
    parameters: ['pointer', 'buffer', 'usize', 'pointer'],
    result: 'i32'
  },
  nextimage_avif_decoder_destroy: {
    parameters: ['pointer'],
    result: 'void'
  },

  // Memory management
  nextimage_free_buffer: {
    parameters: ['pointer'],
    result: 'void'
  },
  nextimage_free_decode_buffer: {
    parameters: ['pointer'],
    result: 'void'
  },

  // Options helpers
  nextimage_webp_default_encode_options: {
    parameters: ['pointer'],
    result: 'void'
  },
  nextimage_avif_default_encode_options: {
    parameters: ['pointer'],
    result: 'void'
  }
} as const

// Load the dynamic library
let lib: Deno.DynamicLibrary<typeof symbols> | null = null

export function getLibrary(): Deno.DynamicLibrary<typeof symbols> {
  if (!lib) {
    const libPath = getLibraryPath()
    lib = Deno.dlopen(libPath, symbols)
  }
  return lib
}

/**
 * Helper function to read a C string from pointer
 */
export function readCString(ptr: Deno.PointerValue): string {
  if (!ptr) return ''

  const view = new Deno.UnsafePointerView(ptr)
  return view.getCString()
}

/**
 * Helper function to create a buffer struct in memory
 */
export function createBufferStruct(): { pointer: Deno.PointerValue; view: DataView } {
  // NextImageBuffer struct: { data: *u8, size: usize }
  // On 64-bit: 8 bytes (pointer) + 8 bytes (size) = 16 bytes
  const buffer = new ArrayBuffer(16)
  const view = new DataView(buffer)
  const pointer = Deno.UnsafePointer.of(buffer)

  return { pointer, view }
}

/**
 * Helper function to read NextImageBuffer from memory
 */
export function readBufferStruct(view: DataView): NextImageBuffer {
  // Read pointer (8 bytes on 64-bit)
  const dataLow = view.getUint32(0, true)
  const dataHigh = view.getUint32(4, true)
  const data = Deno.UnsafePointer.create(BigInt(dataLow) | (BigInt(dataHigh) << 32n))

  // Read size (8 bytes on 64-bit)
  const sizeLow = view.getUint32(8, true)
  const sizeHigh = view.getUint32(12, true)
  const size = Number(BigInt(sizeLow) | (BigInt(sizeHigh) << 32n))

  return { data, size }
}

/**
 * Helper function to create a decode buffer struct in memory
 */
export function createDecodeBufferStruct(): { pointer: Deno.PointerValue; view: DataView } {
  // NextImageDecodeBuffer struct: { data: *u8, size: usize, width: u32, height: u32, format: i32 }
  // 8 + 8 + 4 + 4 + 4 = 28 bytes (+ padding to 32)
  const buffer = new ArrayBuffer(32)
  const view = new DataView(buffer)
  const pointer = Deno.UnsafePointer.of(buffer)

  return { pointer, view }
}

/**
 * Helper function to read NextImageDecodeBuffer from memory
 */
export function readDecodeBufferStruct(view: DataView): NextImageDecodeBuffer {
  // Read pointer (8 bytes)
  const dataLow = view.getUint32(0, true)
  const dataHigh = view.getUint32(4, true)
  const data = Deno.UnsafePointer.create(BigInt(dataLow) | (BigInt(dataHigh) << 32n))

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
 * Helper function to copy data from C memory to Uint8Array
 */
export function copyFromCMemory(ptr: Deno.PointerValue, size: number): Uint8Array {
  if (!ptr || size === 0) {
    return new Uint8Array(0)
  }

  const view = new Deno.UnsafePointerView(ptr)
  return new Uint8Array(view.getArrayBuffer(size))
}

/**
 * Create a WebP encode options struct and initialize with defaults
 */
export function createWebPOptionsStruct(): { pointer: Deno.PointerValue; view: DataView } {
  // Based on NextImageWebPEncodeOptions struct - approximately 40 int fields + 2 floats
  // Size estimate: 40 * 4 (int) + 2 * 4 (float) = 168 bytes, round up to 256 for safety
  const buffer = new ArrayBuffer(256)
  const view = new DataView(buffer)
  const pointer = Deno.UnsafePointer.of(buffer)

  // Initialize with default values
  const lib = getLibrary()
  lib.symbols.nextimage_webp_default_encode_options(pointer)

  return { pointer, view }
}

/**
 * Read WebP options from struct DataView
 */
export function readWebPOptionsStruct(view: DataView): {
  quality: number
  lossless: number
  method: number
  preset: number
  image_hint: number
  exact: number
} {
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
 * Write WebP options to struct DataView
 */
export function writeWebPOptionsStruct(
  view: DataView,
  opts: {
    quality?: number
    lossless?: number | boolean
    method?: number
    preset?: number
    image_hint?: number
    exact?: number | boolean
  }
): void {
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

