//! AVIF encoding and decoding API.
//!
//! This module provides safe, high-level Rust bindings for AVIF image processing.
//!
//! # Examples
//!
//! ## Encoding an image to AVIF
//!
//! ```no_run
//! use nextimage::avif::{AVIFEncoder, AVIFEncodeOptions};
//!
//! let png_data = std::fs::read("input.png").unwrap();
//! let encoder = AVIFEncoder::new(AVIFEncodeOptions::default())?;
//! let avif_data = encoder.encode(&png_data)?;
//! std::fs::write("output.avif", avif_data).unwrap();
//! # Ok::<(), nextimage::Error>(())
//! ```
//!
//! ## Decoding an AVIF image
//!
//! ```no_run
//! use nextimage::avif::{AVIFDecoder, AVIFDecodeOptions};
//!
//! let avif_data = std::fs::read("input.avif").unwrap();
//! let decoder = AVIFDecoder::new(AVIFDecodeOptions::default())?;
//! let png_data = decoder.decode(&avif_data)?;
//! std::fs::write("output.png", png_data).unwrap();
//! # Ok::<(), nextimage::Error>(())
//! ```

use crate::error::{check_status, Error, Result};
use crate::ffi;

/// Output format for AVIF decoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum AVIFOutputFormat {
    /// PNG output (default)
    #[default]
    Png,
    /// JPEG output
    Jpeg,
}

impl From<AVIFOutputFormat> for ffi::NextImageAVIFOutputFormat {
    fn from(format: AVIFOutputFormat) -> Self {
        match format {
            AVIFOutputFormat::Png => ffi::NextImageAVIFOutputFormat::Png,
            AVIFOutputFormat::Jpeg => ffi::NextImageAVIFOutputFormat::Jpeg,
        }
    }
}

/// YUV format for AVIF encoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum YuvFormat {
    /// YUV 4:4:4 - Best quality, largest size
    #[default]
    Yuv444,
    /// YUV 4:2:2 - Good quality
    Yuv422,
    /// YUV 4:2:0 - Smallest size, most compatible
    Yuv420,
    /// Monochrome (grayscale)
    Yuv400,
}

impl YuvFormat {
    fn to_ffi(self) -> i32 {
        match self {
            YuvFormat::Yuv444 => 0,
            YuvFormat::Yuv422 => 1,
            YuvFormat::Yuv420 => 2,
            YuvFormat::Yuv400 => 3,
        }
    }
}

/// Options for AVIF encoding.
#[derive(Debug, Clone)]
pub struct AVIFEncodeOptions {
    /// Quality factor (0-100). Default: 60
    pub quality: i32,
    /// Alpha quality (0-100, -1 to use same as quality). Default: -1
    pub quality_alpha: i32,
    /// Encoding speed (0-10, 0=slowest/best, 10=fastest). Default: 6
    pub speed: i32,
    /// Bit depth (8, 10, or 12). Default: 8
    pub bit_depth: i32,
    /// YUV format. Default: Yuv444
    pub yuv_format: YuvFormat,
    /// Use sharp RGB->YUV conversion. Default: false
    pub sharp_yuv: bool,
    /// Enable alpha channel. Default: true
    pub enable_alpha: bool,
}

impl Default for AVIFEncodeOptions {
    fn default() -> Self {
        Self {
            quality: 60,
            quality_alpha: -1,
            speed: 6,
            bit_depth: 8,
            yuv_format: YuvFormat::Yuv444,
            sharp_yuv: false,
            enable_alpha: true,
        }
    }
}

impl AVIFEncodeOptions {
    /// Create options with the specified quality.
    pub fn with_quality(quality: i32) -> Self {
        Self {
            quality,
            ..Default::default()
        }
    }

    /// Create options optimized for small file size.
    pub fn small_size() -> Self {
        Self {
            quality: 50,
            speed: 4,
            yuv_format: YuvFormat::Yuv420,
            ..Default::default()
        }
    }

    /// Create options optimized for high quality.
    pub fn high_quality() -> Self {
        Self {
            quality: 80,
            speed: 2,
            yuv_format: YuvFormat::Yuv444,
            ..Default::default()
        }
    }

    fn to_ffi(&self) -> ffi::NextImageAVIFEncodeOptions {
        let mut opts = unsafe {
            let mut opts = std::mem::MaybeUninit::uninit();
            ffi::nextimage_avif_default_encode_options(opts.as_mut_ptr());
            opts.assume_init()
        };

        opts.quality = self.quality;
        opts.quality_alpha = self.quality_alpha;
        opts.speed = self.speed;
        opts.bit_depth = self.bit_depth;
        opts.yuv_format = self.yuv_format.to_ffi();
        opts.sharp_yuv = self.sharp_yuv as i32;
        opts.enable_alpha = self.enable_alpha as i32;

        opts
    }
}

/// Options for AVIF decoding.
#[derive(Debug, Clone)]
pub struct AVIFDecodeOptions {
    /// Output format (PNG or JPEG). Default: PNG
    pub output_format: AVIFOutputFormat,
    /// JPEG quality for JPEG output (0-100). Default: 90
    pub jpeg_quality: i32,
    /// Enable multi-threading. Default: false
    pub use_threads: bool,
}

impl Default for AVIFDecodeOptions {
    fn default() -> Self {
        Self {
            output_format: AVIFOutputFormat::Png,
            jpeg_quality: 90,
            use_threads: false,
        }
    }
}

impl AVIFDecodeOptions {
    fn to_ffi(&self) -> ffi::NextImageAVIFDecodeOptions {
        let mut opts = unsafe {
            let mut opts = std::mem::MaybeUninit::uninit();
            ffi::nextimage_avif_default_decode_options(opts.as_mut_ptr());
            opts.assume_init()
        };

        opts.output_format = self.output_format.into();
        opts.jpeg_quality = self.jpeg_quality;
        opts.use_threads = self.use_threads as i32;

        opts
    }
}

/// AVIF encoder for encoding images to AVIF format.
///
/// The encoder can be reused for multiple encoding operations with the same options.
pub struct AVIFEncoder {
    encoder: *mut ffi::NextImageAVIFEncoder,
}

// Safety: The encoder uses thread-local storage for error messages
// and the C library is designed to be thread-safe for independent instances
unsafe impl Send for AVIFEncoder {}

impl AVIFEncoder {
    /// Create a new AVIF encoder with the given options.
    pub fn new(options: AVIFEncodeOptions) -> Result<Self> {
        let ffi_opts = options.to_ffi();
        let encoder = unsafe { ffi::nextimage_avif_encoder_create(&ffi_opts) };

        if encoder.is_null() {
            return Err(Error::EncodeFailed("failed to create encoder".to_string()));
        }

        Ok(Self { encoder })
    }

    /// Encode image data to AVIF format.
    ///
    /// The input can be any supported image format (PNG, JPEG, etc.).
    /// The format is automatically detected.
    pub fn encode(&self, input: &[u8]) -> Result<Vec<u8>> {
        let mut output = ffi::NextImageBuffer::default();

        let status = unsafe {
            ffi::nextimage_avif_encoder_encode(
                self.encoder,
                input.as_ptr(),
                input.len(),
                &mut output,
            )
        };

        check_status(status)?;

        let result = unsafe {
            std::slice::from_raw_parts(output.data, output.size).to_vec()
        };

        unsafe {
            ffi::nextimage_free_buffer(&mut output);
        }

        Ok(result)
    }
}

impl Drop for AVIFEncoder {
    fn drop(&mut self) {
        unsafe {
            ffi::nextimage_avif_encoder_destroy(self.encoder);
        }
    }
}

/// AVIF decoder for decoding AVIF images.
///
/// The decoder can be reused for multiple decoding operations with the same options.
pub struct AVIFDecoder {
    decoder: *mut ffi::NextImageAVIFDecoder,
}

// Safety: Same as AVIFEncoder
unsafe impl Send for AVIFDecoder {}

impl AVIFDecoder {
    /// Create a new AVIF decoder with the given options.
    pub fn new(options: AVIFDecodeOptions) -> Result<Self> {
        let ffi_opts = options.to_ffi();
        let decoder = unsafe { ffi::nextimage_avif_decoder_create(&ffi_opts) };

        if decoder.is_null() {
            return Err(Error::DecodeFailed("failed to create decoder".to_string()));
        }

        Ok(Self { decoder })
    }

    /// Decode AVIF data to the configured output format (PNG or JPEG).
    pub fn decode(&self, avif_data: &[u8]) -> Result<Vec<u8>> {
        let mut output = ffi::NextImageDecodeBuffer::default();

        let status = unsafe {
            ffi::nextimage_avif_decoder_decode(
                self.decoder,
                avif_data.as_ptr(),
                avif_data.len(),
                &mut output,
            )
        };

        check_status(status)?;

        let result = unsafe {
            std::slice::from_raw_parts(output.data, output.data_size).to_vec()
        };

        unsafe {
            ffi::nextimage_free_decode_buffer(&mut output);
        }

        Ok(result)
    }
}

impl Drop for AVIFDecoder {
    fn drop(&mut self) {
        unsafe {
            ffi::nextimage_avif_decoder_destroy(self.decoder);
        }
    }
}

/// Encode image data to AVIF format using default options.
///
/// This is a convenience function for one-off encoding operations.
/// For multiple encodings, consider using [`AVIFEncoder`] to reuse resources.
pub fn encode(input: &[u8]) -> Result<Vec<u8>> {
    let encoder = AVIFEncoder::new(AVIFEncodeOptions::default())?;
    encoder.encode(input)
}

/// Encode image data to AVIF format with custom options.
pub fn encode_with_options(input: &[u8], options: AVIFEncodeOptions) -> Result<Vec<u8>> {
    let encoder = AVIFEncoder::new(options)?;
    encoder.encode(input)
}

/// Decode AVIF data to PNG format using default options.
///
/// This is a convenience function for one-off decoding operations.
/// For multiple decodings, consider using [`AVIFDecoder`] to reuse resources.
pub fn decode(avif_data: &[u8]) -> Result<Vec<u8>> {
    let decoder = AVIFDecoder::new(AVIFDecodeOptions::default())?;
    decoder.decode(avif_data)
}

/// Decode AVIF data with custom options.
pub fn decode_with_options(avif_data: &[u8], options: AVIFDecodeOptions) -> Result<Vec<u8>> {
    let decoder = AVIFDecoder::new(options)?;
    decoder.decode(avif_data)
}
