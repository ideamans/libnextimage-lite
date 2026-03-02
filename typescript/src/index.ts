/**
 * libnextimage Light v2 - Simplified image conversion API
 */

// Export types
export { NextImageStatus, NextImageError, getStatusMessage } from './types'

// Export library utilities
export { getLibraryVersion, getPlatform, getLibraryFileName, getLibraryPath, clearLibraryPathCache } from './library'

// Export Light API v2 conversion functions
export { legacyToWebp, webpToLegacy, legacyToAvif, avifToLegacy } from './light'

// Export Light API types
export type { ConvertInput, ConvertOutput } from './light'
