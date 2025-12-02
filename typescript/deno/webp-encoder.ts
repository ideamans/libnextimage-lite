/**
 * WebP Encoder for Deno
 */

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
  private encoder: Deno.PointerValue
  private options: WebPEncodeOptions

  constructor(options: Partial<WebPEncodeOptions> = {}) {
    this.options = { quality: 75, lossless: false, method: 4, ...options }

    // Create options struct initialized with defaults
    const { pointer: optionsPtr, view: optionsView } = createWebPOptionsStruct()

    // Override with user-provided options
    writeWebPOptionsStruct(optionsView, {
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

  encode(data: Uint8Array): Uint8Array {
    if (!this.encoder) {
      throw new Error('Encoder has been closed')
    }

    const lib = getLibrary()
    const { pointer: outputPtr, view: outputView } = createBufferStruct()

    const status = lib.symbols.nextimage_webp_encoder_encode(
      this.encoder,
      data,
      data.length,
      outputPtr
    )

    // Status 0 = success (NextImageStatus.OK)
    if (status !== 0) {
      throw new Error('WebP encoding failed')
    }

    const output = readBufferStruct(outputView)
    const result = copyFromCMemory(output.data, output.size)

    // Free C-allocated memory
    lib.symbols.nextimage_free_buffer(outputPtr)

    return result
  }

  encodeFile(path: string): Uint8Array {
    const data = Deno.readFileSync(path)
    return this.encode(data)
  }

  close(): void {
    if (this.encoder) {
      const lib = getLibrary()
      lib.symbols.nextimage_webp_encoder_destroy(this.encoder)
      this.encoder = null!
    }
  }

  static getDefaultOptions(): WebPEncodeOptions {
    const { view } = createWebPOptionsStruct()
    const opts = readWebPOptionsStruct(view)

    return {
      quality: opts.quality,
      lossless: opts.lossless !== 0, // C bool to JS bool conversion
      method: opts.method
    }
  }
}
