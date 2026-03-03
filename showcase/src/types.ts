export interface ConversionResult {
  function: string
  outputFile: string
  outputMime: string
  outputSize: number
  compressionRatio: number
  durationMs: number
  error?: string
}

export interface ImageEntry {
  id: string
  sourceFile: string
  sourceMime: string
  sourceSize: number
  originalImage: string
  tags: string[]
  conversions: ConversionResult[]
}

export interface Manifest {
  generatedAt: string
  platform: string
  entries: ImageEntry[]
}
