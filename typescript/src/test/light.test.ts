/**
 * Light API v2 tests
 */

import { describe, it } from 'node:test'
import * as assert from 'node:assert'
import * as fs from 'fs'
import * as path from 'path'
import { legacyToWebp, webpToLegacy, legacyToAvif, avifToLegacy, NextImageError, getLibraryVersion } from '../index'

// From dist/test/ -> ../../ = typescript/, then ../testdata
const testdataDir = path.join(__dirname, '..', '..', '..', 'testdata')

function readTestFile(subdir: string, filename: string): Buffer {
  return fs.readFileSync(path.join(testdataDir, subdir, filename))
}

// ========================================
// legacyToWebp tests
// ========================================
describe('legacyToWebp', () => {
  it('should convert JPEG to WebP with default quality', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const result = legacyToWebp({ data: jpeg })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data.toString('ascii', 0, 4), 'RIFF')
    assert.strictEqual(result.data.toString('ascii', 8, 12), 'WEBP')
    assert.strictEqual(result.mimeType, 'image/webp')
  })

  it('should convert JPEG to WebP with explicit quality', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const q50 = legacyToWebp({ data: jpeg, quality: 50 })
    const q90 = legacyToWebp({ data: jpeg, quality: 90 })
    assert.ok(q50.data.length > 0)
    assert.ok(q90.data.length > q50.data.length, 'Higher quality should produce larger output')
    assert.strictEqual(q50.mimeType, 'image/webp')
    assert.strictEqual(q90.mimeType, 'image/webp')
  })

  it('should convert PNG to WebP (lossless)', () => {
    const png = readTestFile('png-source', 'small-128x128.png')
    const result = legacyToWebp({ data: png })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data.toString('ascii', 0, 4), 'RIFF')
    assert.strictEqual(result.data.toString('ascii', 8, 12), 'WEBP')
    assert.strictEqual(result.mimeType, 'image/webp')
  })

  it('should convert static GIF to WebP', () => {
    const gif = readTestFile('gif-source', 'static-64x64.gif')
    const result = legacyToWebp({ data: gif })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data.toString('ascii', 0, 4), 'RIFF')
    assert.strictEqual(result.mimeType, 'image/webp')
  })

  it('should convert animated GIF to WebP', () => {
    const gif = readTestFile('gif-source', 'animated-3frames.gif')
    const result = legacyToWebp({ data: gif })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data.toString('ascii', 0, 4), 'RIFF')
    assert.strictEqual(result.mimeType, 'image/webp')
  })

  it('should throw on empty input', () => {
    assert.throws(() => legacyToWebp({ data: Buffer.alloc(0) }), NextImageError)
  })

  it('should throw on unsupported input', () => {
    assert.throws(() => legacyToWebp({ data: Buffer.from('not an image') }), NextImageError)
  })
})

// ========================================
// webpToLegacy tests
// ========================================
describe('webpToLegacy', () => {
  it('should convert lossy WebP to JPEG', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const webp = legacyToWebp({ data: jpeg, quality: 75 })
    const result = webpToLegacy({ data: webp.data })
    assert.ok(result.data.length > 0)
    // JPEG magic: FF D8
    assert.strictEqual(result.data[0], 0xff)
    assert.strictEqual(result.data[1], 0xd8)
    assert.strictEqual(result.mimeType, 'image/jpeg')
  })

  it('should convert lossless WebP to PNG', () => {
    const png = readTestFile('png-source', 'small-128x128.png')
    const webp = legacyToWebp({ data: png })
    const result = webpToLegacy({ data: webp.data })
    assert.ok(result.data.length > 0)
    // PNG magic: 89 50 4E 47
    assert.strictEqual(result.data[0], 0x89)
    assert.strictEqual(result.data.toString('ascii', 1, 4), 'PNG')
    assert.strictEqual(result.mimeType, 'image/png')
  })

  it('should convert lossy WebP to JPEG with explicit quality', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const webp = legacyToWebp({ data: jpeg })
    const result = webpToLegacy({ data: webp.data, quality: 50 })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data[0], 0xff)
    assert.strictEqual(result.data[1], 0xd8)
    assert.strictEqual(result.mimeType, 'image/jpeg')
  })
})

// ========================================
// legacyToAvif tests
// ========================================
describe('legacyToAvif', () => {
  it('should convert JPEG to AVIF with default quality', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const result = legacyToAvif({ data: jpeg })
    assert.ok(result.data.length > 0)
    // AVIF: ISOBMFF "ftyp" at offset 4
    assert.strictEqual(result.data.toString('ascii', 4, 8), 'ftyp')
    assert.strictEqual(result.mimeType, 'image/avif')
  })

  it('should convert JPEG to AVIF with explicit quality', () => {
    const jpeg = readTestFile('jpeg-source', 'small-128x128.jpg')
    const q40 = legacyToAvif({ data: jpeg, quality: 40 })
    const q80 = legacyToAvif({ data: jpeg, quality: 80 })
    assert.ok(q40.data.length > 0)
    assert.ok(q80.data.length > q40.data.length, 'Higher quality should produce larger output')
    assert.strictEqual(q40.mimeType, 'image/avif')
  })

  it('should convert PNG to AVIF (lossless)', () => {
    const png = readTestFile('png-source', 'small-128x128.png')
    const result = legacyToAvif({ data: png })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data.toString('ascii', 4, 8), 'ftyp')
    assert.strictEqual(result.mimeType, 'image/avif')
  })

  it('should throw on GIF input (unsupported)', () => {
    const gif = readTestFile('gif-source', 'static-64x64.gif')
    assert.throws(() => legacyToAvif({ data: gif }), NextImageError)
  })
})

// ========================================
// avifToLegacy tests
// ========================================
describe('avifToLegacy', () => {
  it('should convert lossy AVIF to JPEG', () => {
    const avif = readTestFile('avif', 'red.avif')
    const result = avifToLegacy({ data: avif })
    assert.ok(result.data.length > 0)
    assert.strictEqual(result.data[0], 0xff)
    assert.strictEqual(result.data[1], 0xd8)
    assert.strictEqual(result.mimeType, 'image/jpeg')
  })

  it('should convert lossless AVIF to PNG', () => {
    // Create lossless AVIF from PNG first
    const png = readTestFile('png-source', 'small-128x128.png')
    const avif = legacyToAvif({ data: png })

    const result = avifToLegacy({ data: avif.data })
    assert.ok(result.data.length > 0)
    // PNG magic: 89 50 4E 47
    assert.strictEqual(result.data[0], 0x89)
    assert.strictEqual(result.data.toString('ascii', 1, 4), 'PNG')
    assert.strictEqual(result.mimeType, 'image/png')
  })
})

// ========================================
// ICC profile preservation tests
// ========================================

function hasJPEGICCMarker(data: Buffer): boolean {
  const marker = Buffer.from('ICC_PROFILE')
  for (let i = 0; i + marker.length < data.length; i++) {
    if (data[i] === 0xff && data[i + 1] === 0xe2) {
      // APP2 marker found, check for ICC_PROFILE signature after length bytes
      if (i + 4 + marker.length < data.length) {
        if (data.subarray(i + 4, i + 4 + marker.length).equals(marker)) {
          return true
        }
      }
    }
  }
  return false
}

function hasPNGICCChunk(data: Buffer): boolean {
  const iccp = Buffer.from('iCCP')
  for (let i = 8; i + 8 < data.length; i++) {
    if (data[i + 4] === iccp[0] && data[i + 5] === iccp[1] && data[i + 6] === iccp[2] && data[i + 7] === iccp[3]) {
      return true
    }
  }
  return false
}

describe('ICC profile preservation', () => {
  it('should preserve JPEG ICC through AVIF roundtrip', () => {
    const jpeg = readTestFile('icc', 'srgb.jpg')
    assert.ok(hasJPEGICCMarker(jpeg), 'Input JPEG should have ICC')

    const avif = legacyToAvif({ data: jpeg, quality: 60 })
    assert.strictEqual(avif.mimeType, 'image/avif')

    const output = avifToLegacy({ data: avif.data })
    assert.strictEqual(output.mimeType, 'image/jpeg')
    assert.ok(hasJPEGICCMarker(output.data), 'Output JPEG should have ICC after roundtrip')
  })

  it('should preserve PNG ICC through AVIF roundtrip', () => {
    const png = readTestFile('icc', 'srgb.png')
    assert.ok(hasPNGICCChunk(png), 'Input PNG should have ICC')

    const avif = legacyToAvif({ data: png })
    assert.strictEqual(avif.mimeType, 'image/avif')

    const output = avifToLegacy({ data: avif.data })
    assert.strictEqual(output.mimeType, 'image/png')
    assert.ok(hasPNGICCChunk(output.data), 'Output PNG should have ICC after roundtrip')
  })

  it('should not add ICC when input has none', () => {
    const jpeg = readTestFile('icc', 'no-icc.jpg')
    assert.ok(!hasJPEGICCMarker(jpeg), 'Input JPEG should not have ICC')

    const avif = legacyToAvif({ data: jpeg, quality: 60 })
    const output = avifToLegacy({ data: avif.data })
    assert.strictEqual(output.mimeType, 'image/jpeg')
    assert.ok(!hasJPEGICCMarker(output.data), 'Output JPEG should not have ICC')
  })

  it('should preserve Display P3 ICC through AVIF roundtrip', () => {
    const jpeg = readTestFile('icc', 'display-p3.jpg')
    assert.ok(hasJPEGICCMarker(jpeg), 'Input JPEG should have Display P3 ICC')

    const avif = legacyToAvif({ data: jpeg, quality: 60 })
    const output = avifToLegacy({ data: avif.data })
    assert.strictEqual(output.mimeType, 'image/jpeg')
    assert.ok(hasJPEGICCMarker(output.data), 'Output JPEG should have ICC (Display P3)')
  })
})

// ========================================
// Version test
// ========================================
describe('getLibraryVersion', () => {
  it('should return a version string', () => {
    const version = getLibraryVersion()
    assert.ok(version.length > 0)
  })
})
