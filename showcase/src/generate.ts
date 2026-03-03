import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { legacyToWebp, webpToLegacy, legacyToAvif, avifToLegacy, getPlatform } from 'libnextimage'
import type { ConvertInput } from 'libnextimage'
import type { Manifest, ImageEntry, ConversionResult } from './types.js'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const TESTDATA = path.resolve(__dirname, '../../testdata')
const OUT_DIR = path.resolve(__dirname, '../public')
const IMAGES_DIR = path.join(OUT_DIR, 'images')

const SOURCES: { file: string; mime: string }[] = [
  // Basic formats
  { file: 'jpeg-source/photo-like.jpg', mime: 'image/jpeg' },
  { file: 'jpeg-source/text.jpg', mime: 'image/jpeg' },
  { file: 'jpeg-source/gradient-radial.jpg', mime: 'image/jpeg' },
  { file: 'png-source/photo-like.png', mime: 'image/png' },
  { file: 'png-source/checkerboard.png', mime: 'image/png' },
  { file: 'gif-source/static-512x512.gif', mime: 'image/gif' },
  { file: 'gif-source/animated-3frames.gif', mime: 'image/gif' },
  { file: 'webp-samples/lossy-q90.webp', mime: 'image/webp' },
  { file: 'webp-samples/lossless.webp', mime: 'image/webp' },
  { file: 'avif/red.avif', mime: 'image/avif' },
  // ICC profiles
  { file: 'icc/srgb.jpg', mime: 'image/jpeg' },
  { file: 'icc/srgb.png', mime: 'image/png' },
  { file: 'icc/display-p3.jpg', mime: 'image/jpeg' },
  { file: 'icc/display-p3.png', mime: 'image/png' },
  { file: 'icc/no-icc.jpg', mime: 'image/jpeg' },
  { file: 'icc/no-icc.png', mime: 'image/png' },
  // EXIF orientation
  { file: 'orientation/orientation-1.jpg', mime: 'image/jpeg' },
  { file: 'orientation/orientation-3.jpg', mime: 'image/jpeg' },
  { file: 'orientation/orientation-6.jpg', mime: 'image/jpeg' },
  { file: 'orientation/orientation-8.jpg', mime: 'image/jpeg' },
]

type ConvertFn = (input: ConvertInput) => { data: Buffer; mimeType: string }

const CONVERSIONS: Record<string, { fn: ConvertFn; name: string }[]> = {
  'image/jpeg': [
    { fn: legacyToWebp, name: 'legacyToWebp' },
    { fn: legacyToAvif, name: 'legacyToAvif' },
  ],
  'image/png': [
    { fn: legacyToWebp, name: 'legacyToWebp' },
    { fn: legacyToAvif, name: 'legacyToAvif' },
  ],
  'image/gif': [{ fn: legacyToWebp, name: 'legacyToWebp' }],
  'image/webp': [{ fn: webpToLegacy, name: 'webpToLegacy' }],
  'image/avif': [{ fn: avifToLegacy, name: 'avifToLegacy' }],
}

const MIME_TO_EXT: Record<string, string> = {
  'image/jpeg': '.jpg',
  'image/png': '.png',
  'image/gif': '.gif',
  'image/webp': '.webp',
  'image/avif': '.avif',
}

function makeId(file: string): string {
  const dir = path.dirname(file).replace(/[^a-z0-9-]/gi, '-')
  const name = path.basename(file, path.extname(file)).replace(/[^a-z0-9-]/gi, '-')
  return `${dir}-${name}`
}

function deriveTags(file: string, mime: string): string[] {
  const tags: string[] = [mime.replace('image/', '').toUpperCase()]
  const dir = path.dirname(file)
  if (dir === 'icc') tags.push('ICC')
  if (dir === 'orientation') tags.push('EXIF Orientation')
  return tags
}

function main() {
  fs.mkdirSync(IMAGES_DIR, { recursive: true })

  const entries: ImageEntry[] = []

  for (const source of SOURCES) {
    const srcPath = path.join(TESTDATA, source.file)
    if (!fs.existsSync(srcPath)) {
      console.warn(`SKIP: ${source.file} not found`)
      continue
    }

    const data = fs.readFileSync(srcPath)
    const id = makeId(source.file)
    const originalImage = `${id}${path.extname(source.file)}`

    fs.copyFileSync(srcPath, path.join(IMAGES_DIR, originalImage))

    const conversions: ConversionResult[] = []
    const fns = CONVERSIONS[source.mime] || []

    for (const { fn, name } of fns) {
      const start = performance.now()
      try {
        const result = fn({ data })
        const elapsed = performance.now() - start
        const ext = MIME_TO_EXT[result.mimeType] || '.bin'
        const outputFile = `${id}-${name}${ext}`

        fs.writeFileSync(path.join(IMAGES_DIR, outputFile), result.data)

        conversions.push({
          function: name,
          outputFile,
          outputMime: result.mimeType,
          outputSize: result.data.length,
          compressionRatio: result.data.length / data.length,
          durationMs: Math.round(elapsed * 10) / 10,
        })
        console.log(`  ${name}: ${data.length} -> ${result.data.length} (${(conversions.at(-1)!.compressionRatio * 100).toFixed(1)}%) ${elapsed.toFixed(1)}ms`)
      } catch (err: unknown) {
        const elapsed = performance.now() - start
        conversions.push({
          function: name,
          outputFile: '',
          outputMime: '',
          outputSize: 0,
          compressionRatio: 0,
          durationMs: Math.round(elapsed * 10) / 10,
          error: err instanceof Error ? err.message : String(err),
        })
        console.error(`  ${name}: ERROR - ${err instanceof Error ? err.message : err}`)
      }
    }

    entries.push({
      id,
      sourceFile: source.file,
      sourceMime: source.mime,
      sourceSize: data.length,
      originalImage,
      tags: deriveTags(source.file, source.mime),
      conversions,
    })
    console.log(`${source.file} (${data.length} bytes) - ${conversions.length} conversions`)
  }

  const manifest: Manifest = {
    generatedAt: new Date().toISOString(),
    platform: getPlatform(),
    entries,
  }

  fs.writeFileSync(path.join(OUT_DIR, 'manifest.json'), JSON.stringify(manifest, null, 2))
  console.log(`\nDone: ${entries.length} entries, manifest written to public/manifest.json`)
}

main()
