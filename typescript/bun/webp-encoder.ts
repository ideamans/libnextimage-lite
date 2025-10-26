/**
 * WebP Encoder for Bun
 */

import { ptr, type Pointer } from 'bun:ffi'
import {
  getLibrary,
  createBufferStruct,
  readBufferStruct,
  copyFromCMemory,
  createWebPOptionsStruct,
  writeWebPOptionsStruct,
  readWebPOptionsStruct
} from './ffi.ts'

export interface WebPEncodeOptions {
  quality?: number
  lossless?: boolean
  method?: number
  [key: string]: unknown
}

export class WebPEncoder {
  private encoder: Pointer
  private options: WebPEncodeOptions

  constructor(options: Partial<WebPEncodeOptions> = {}) {
    this.options = { quality: 75, lossless: false, method: 4, ...options }

    // Create options struct initialized with defaults
    const { buffer: optionsBuffer, ptr: optionsPtr } = createWebPOptionsStruct()

    // Override with user-provided options
    writeWebPOptionsStruct(optionsBuffer, {
      quality: this.options.quality,
      lossless: this.options.lossless,
      method: this.options.method
    })

    const lib = getLibrary()
    this.encoder = lib.symbols.nextimage_webp_encoder_create(optionsPtr)

    if (!this.encoder) {
      throw new Error('Failed to create WebP encoder')
    }
  }

  encode(data: Buffer): Buffer {
    if (!this.encoder) {
      throw new Error('Encoder has been closed')
    }

    const lib = getLibrary()
    const { buffer: outputBuffer, ptr: outputPtr } = createBufferStruct()

    const status = lib.symbols.nextimage_webp_encoder_encode(this.encoder, ptr(data), data.length, outputPtr)

    if (status !== 0) {
      throw new Error('WebP encoding failed')
    }

    const output = readBufferStruct(outputBuffer)
    const result = copyFromCMemory(output.data, output.size)

    // Free C-allocated memory
    lib.symbols.nextimage_free_buffer(outputPtr)

    return result
  }

  encodeFile(path: string): Buffer {
    const file = Bun.file(path)
    const arrayBuffer = file.arrayBuffer()
    return this.encode(Buffer.from(arrayBuffer))
  }

  close(): void {
    if (this.encoder) {
      const lib = getLibrary()
      lib.symbols.nextimage_webp_encoder_destroy(this.encoder)
      this.encoder = null!
    }
  }

  static getDefaultOptions(): WebPEncodeOptions {
    const { buffer } = createWebPOptionsStruct()
    const opts = readWebPOptionsStruct(buffer)

    return {
      quality: opts.quality,
      lossless: opts.lossless !== 0,
      method: opts.method
    }
  }
}
