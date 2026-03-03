/**
 * Type Definitions for libnextimage Lite API
 */

/**
 * Status codes returned by libnextimage functions
 */
export enum NextImageStatus {
  OK = 0,
  ERROR_INVALID_PARAM = -1,
  ERROR_ENCODE_FAILED = -2,
  ERROR_DECODE_FAILED = -3,
  ERROR_OUT_OF_MEMORY = -4,
  ERROR_UNSUPPORTED = -5,
  ERROR_BUFFER_TOO_SMALL = -6,
}

/**
 * Helper: Get error message for status code
 */
export function getStatusMessage(status: NextImageStatus): string {
  switch (status) {
    case NextImageStatus.OK:
      return 'Success'
    case NextImageStatus.ERROR_INVALID_PARAM:
      return 'Invalid parameter'
    case NextImageStatus.ERROR_ENCODE_FAILED:
      return 'Encoding failed'
    case NextImageStatus.ERROR_DECODE_FAILED:
      return 'Decoding failed'
    case NextImageStatus.ERROR_OUT_OF_MEMORY:
      return 'Out of memory'
    case NextImageStatus.ERROR_UNSUPPORTED:
      return 'Unsupported operation'
    case NextImageStatus.ERROR_BUFFER_TOO_SMALL:
      return 'Buffer too small'
    default:
      return 'Unknown error'
  }
}

/**
 * Custom error class for libnextimage operations
 */
export class NextImageError extends Error {
  constructor(
    public status: NextImageStatus,
    message?: string,
  ) {
    super(message || getStatusMessage(status))
    this.name = 'NextImageError'
  }
}
