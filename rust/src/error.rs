//! Error types for nextimage.

use crate::ffi::NextImageStatus;
use std::ffi::CStr;
use thiserror::Error;

/// Error type for nextimage operations.
#[derive(Error, Debug)]
pub enum Error {
    /// Invalid parameter passed to the function
    #[error("invalid parameter: {0}")]
    InvalidParam(String),

    /// Encoding failed
    #[error("encode failed: {0}")]
    EncodeFailed(String),

    /// Decoding failed
    #[error("decode failed: {0}")]
    DecodeFailed(String),

    /// Out of memory
    #[error("out of memory")]
    OutOfMemory,

    /// Unsupported operation or format
    #[error("unsupported: {0}")]
    Unsupported(String),

    /// Buffer too small
    #[error("buffer too small")]
    BufferTooSmall,

    /// Library not initialized or not found
    #[error("library not available: {0}")]
    LibraryNotAvailable(String),

    /// Download failed
    #[error("download failed: {0}")]
    DownloadFailed(String),

    /// IO error
    #[error("io error: {0}")]
    Io(#[from] std::io::Error),

    /// Unknown error
    #[error("unknown error: status={0}")]
    Unknown(i32),
}

/// Result type for nextimage operations.
pub type Result<T> = std::result::Result<T, Error>;

impl Error {
    /// Create an error from a NextImageStatus code.
    pub(crate) fn from_status(status: NextImageStatus) -> Self {
        let message = get_last_error_message();

        match status {
            NextImageStatus::Ok => unreachable!("Ok status should not be converted to error"),
            NextImageStatus::ErrorInvalidParam => {
                Error::InvalidParam(message.unwrap_or_else(|| "invalid parameter".to_string()))
            }
            NextImageStatus::ErrorEncodeFailed => {
                Error::EncodeFailed(message.unwrap_or_else(|| "encode failed".to_string()))
            }
            NextImageStatus::ErrorDecodeFailed => {
                Error::DecodeFailed(message.unwrap_or_else(|| "decode failed".to_string()))
            }
            NextImageStatus::ErrorOutOfMemory => Error::OutOfMemory,
            NextImageStatus::ErrorUnsupported => {
                Error::Unsupported(message.unwrap_or_else(|| "unsupported".to_string()))
            }
            NextImageStatus::ErrorBufferTooSmall => Error::BufferTooSmall,
        }
    }
}

/// Get the last error message from the library.
fn get_last_error_message() -> Option<String> {
    unsafe {
        let ptr = crate::ffi::nextimage_last_error_message();
        if ptr.is_null() {
            None
        } else {
            Some(CStr::from_ptr(ptr).to_string_lossy().into_owned())
        }
    }
}

/// Check status and convert to Result.
pub(crate) fn check_status(status: NextImageStatus) -> Result<()> {
    if status == NextImageStatus::Ok {
        Ok(())
    } else {
        Err(Error::from_status(status))
    }
}
