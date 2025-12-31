//! WebP encoding and decoding API.
//!
//! This module provides safe, high-level Rust bindings for WebP image processing.
//!
//! # Examples
//!
//! ## Encoding an image to WebP
//!
//! ```no_run
//! use nextimage::webp::{WebPEncoder, WebPEncodeOptions};
//!
//! let png_data = std::fs::read("input.png").unwrap();
//! let encoder = WebPEncoder::new(WebPEncodeOptions::default())?;
//! let webp_data = encoder.encode(&png_data)?;
//! std::fs::write("output.webp", webp_data).unwrap();
//! # Ok::<(), nextimage::Error>(())
//! ```
//!
//! ## Decoding a WebP image
//!
//! ```no_run
//! use nextimage::webp::{WebPDecoder, WebPDecodeOptions};
//!
//! let webp_data = std::fs::read("input.webp").unwrap();
//! let decoder = WebPDecoder::new(WebPDecodeOptions::default())?;
//! let png_data = decoder.decode(&webp_data)?;
//! std::fs::write("output.png", png_data).unwrap();
//! # Ok::<(), nextimage::Error>(())
//! ```

use crate::error::{check_status, Error, Result};
use crate::ffi;

/// WebP preset for encoding optimization.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WebPPreset {
    /// Default preset
    #[default]
    Default,
    /// Digital picture, like portrait
    Picture,
    /// Outdoor photograph
    Photo,
    /// Hand or line drawing
    Drawing,
    /// Small-sized colorful images
    Icon,
    /// Text-like
    Text,
}

impl From<WebPPreset> for ffi::NextImageWebPPreset {
    fn from(preset: WebPPreset) -> Self {
        match preset {
            WebPPreset::Default => ffi::NextImageWebPPreset::Default,
            WebPPreset::Picture => ffi::NextImageWebPPreset::Picture,
            WebPPreset::Photo => ffi::NextImageWebPPreset::Photo,
            WebPPreset::Drawing => ffi::NextImageWebPPreset::Drawing,
            WebPPreset::Icon => ffi::NextImageWebPPreset::Icon,
            WebPPreset::Text => ffi::NextImageWebPPreset::Text,
        }
    }
}

/// Output format for WebP decoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WebPOutputFormat {
    /// PNG output (default)
    #[default]
    Png,
    /// JPEG output
    Jpeg,
}

impl From<WebPOutputFormat> for ffi::NextImageWebPOutputFormat {
    fn from(format: WebPOutputFormat) -> Self {
        match format {
            WebPOutputFormat::Png => ffi::NextImageWebPOutputFormat::Png,
            WebPOutputFormat::Jpeg => ffi::NextImageWebPOutputFormat::Jpeg,
        }
    }
}

/// Options for WebP encoding.
#[derive(Debug, Clone)]
pub struct WebPEncodeOptions {
    /// Quality factor (0-100). Default: 75
    pub quality: f32,
    /// Enable lossless encoding. Default: false
    pub lossless: bool,
    /// Compression method (0-6, 0=fast, 6=best). Default: 4
    pub method: i32,
    /// Preset for encoding. Default: Default
    pub preset: WebPPreset,
    /// Enable multi-threading. Default: false
    pub use_threads: bool,
    /// Use sharp RGB->YUV conversion. Default: false
    pub sharp_yuv: bool,
    /// Near-lossless quality (0-100, -1 to disable). Default: -1
    pub near_lossless: i32,
}

impl Default for WebPEncodeOptions {
    fn default() -> Self {
        Self {
            quality: 75.0,
            lossless: false,
            method: 4,
            preset: WebPPreset::Default,
            use_threads: false,
            sharp_yuv: false,
            near_lossless: -1,
        }
    }
}

impl WebPEncodeOptions {
    /// Create options for lossy encoding with the specified quality.
    pub fn lossy(quality: f32) -> Self {
        Self {
            quality,
            ..Default::default()
        }
    }

    /// Create options for lossless encoding.
    pub fn lossless() -> Self {
        Self {
            lossless: true,
            ..Default::default()
        }
    }

    fn to_ffi(&self) -> ffi::NextImageWebPEncodeOptions {
        let mut opts = unsafe {
            let mut opts = std::mem::MaybeUninit::uninit();
            ffi::nextimage_webp_default_encode_options(opts.as_mut_ptr());
            opts.assume_init()
        };

        opts.quality = self.quality;
        opts.lossless = self.lossless as i32;
        opts.method = self.method;
        opts.preset = self.preset.into();
        opts.thread_level = self.use_threads as i32;
        opts.use_sharp_yuv = self.sharp_yuv as i32;
        opts.near_lossless = self.near_lossless;

        opts
    }
}

/// Options for WebP decoding.
#[derive(Debug, Clone)]
pub struct WebPDecodeOptions {
    /// Output format (PNG or JPEG). Default: PNG
    pub output_format: WebPOutputFormat,
    /// JPEG quality for JPEG output (0-100). Default: 90
    pub jpeg_quality: i32,
    /// Enable multi-threading. Default: false
    pub use_threads: bool,
}

impl Default for WebPDecodeOptions {
    fn default() -> Self {
        Self {
            output_format: WebPOutputFormat::Png,
            jpeg_quality: 90,
            use_threads: false,
        }
    }
}

impl WebPDecodeOptions {
    fn to_ffi(&self) -> ffi::NextImageWebPDecodeOptions {
        let mut opts = unsafe {
            let mut opts = std::mem::MaybeUninit::uninit();
            ffi::nextimage_webp_default_decode_options(opts.as_mut_ptr());
            opts.assume_init()
        };

        opts.output_format = self.output_format.into();
        opts.jpeg_quality = self.jpeg_quality;
        opts.use_threads = self.use_threads as i32;

        opts
    }
}

/// WebP encoder for encoding images to WebP format.
///
/// The encoder can be reused for multiple encoding operations with the same options.
pub struct WebPEncoder {
    encoder: *mut ffi::NextImageWebPEncoder,
}

// Safety: The encoder uses thread-local storage for error messages
// and the C library is designed to be thread-safe for independent instances
unsafe impl Send for WebPEncoder {}

impl WebPEncoder {
    /// Create a new WebP encoder with the given options.
    pub fn new(options: WebPEncodeOptions) -> Result<Self> {
        let ffi_opts = options.to_ffi();
        let encoder = unsafe { ffi::nextimage_webp_encoder_create(&ffi_opts) };

        if encoder.is_null() {
            return Err(Error::EncodeFailed("failed to create encoder".to_string()));
        }

        Ok(Self { encoder })
    }

    /// Encode image data to WebP format.
    ///
    /// The input can be any supported image format (PNG, JPEG, GIF, etc.).
    /// The format is automatically detected.
    pub fn encode(&self, input: &[u8]) -> Result<Vec<u8>> {
        let mut output = ffi::NextImageBuffer::default();

        let status = unsafe {
            ffi::nextimage_webp_encoder_encode(
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

impl Drop for WebPEncoder {
    fn drop(&mut self) {
        unsafe {
            ffi::nextimage_webp_encoder_destroy(self.encoder);
        }
    }
}

/// WebP decoder for decoding WebP images.
///
/// The decoder can be reused for multiple decoding operations with the same options.
pub struct WebPDecoder {
    decoder: *mut ffi::NextImageWebPDecoder,
}

// Safety: Same as WebPEncoder
unsafe impl Send for WebPDecoder {}

impl WebPDecoder {
    /// Create a new WebP decoder with the given options.
    pub fn new(options: WebPDecodeOptions) -> Result<Self> {
        let ffi_opts = options.to_ffi();
        let decoder = unsafe { ffi::nextimage_webp_decoder_create(&ffi_opts) };

        if decoder.is_null() {
            return Err(Error::DecodeFailed("failed to create decoder".to_string()));
        }

        Ok(Self { decoder })
    }

    /// Decode WebP data to the configured output format (PNG or JPEG).
    pub fn decode(&self, webp_data: &[u8]) -> Result<Vec<u8>> {
        let mut output = ffi::NextImageDecodeBuffer::default();

        let status = unsafe {
            ffi::nextimage_webp_decoder_decode(
                self.decoder,
                webp_data.as_ptr(),
                webp_data.len(),
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

impl Drop for WebPDecoder {
    fn drop(&mut self) {
        unsafe {
            ffi::nextimage_webp_decoder_destroy(self.decoder);
        }
    }
}

/// Encode image data to WebP format using default options.
///
/// This is a convenience function for one-off encoding operations.
/// For multiple encodings, consider using [`WebPEncoder`] to reuse resources.
pub fn encode(input: &[u8]) -> Result<Vec<u8>> {
    let encoder = WebPEncoder::new(WebPEncodeOptions::default())?;
    encoder.encode(input)
}

/// Encode image data to WebP format with custom options.
pub fn encode_with_options(input: &[u8], options: WebPEncodeOptions) -> Result<Vec<u8>> {
    let encoder = WebPEncoder::new(options)?;
    encoder.encode(input)
}

/// Decode WebP data to PNG format using default options.
///
/// This is a convenience function for one-off decoding operations.
/// For multiple decodings, consider using [`WebPDecoder`] to reuse resources.
pub fn decode(webp_data: &[u8]) -> Result<Vec<u8>> {
    let decoder = WebPDecoder::new(WebPDecodeOptions::default())?;
    decoder.decode(webp_data)
}

/// Decode WebP data with custom options.
pub fn decode_with_options(webp_data: &[u8], options: WebPDecodeOptions) -> Result<Vec<u8>> {
    let decoder = WebPDecoder::new(options)?;
    decoder.decode(webp_data)
}

/// Convert GIF to WebP.
pub fn gif_to_webp(gif_data: &[u8]) -> Result<Vec<u8>> {
    gif_to_webp_with_options(gif_data, WebPEncodeOptions::default())
}

/// Convert GIF to WebP with custom options.
pub fn gif_to_webp_with_options(gif_data: &[u8], options: WebPEncodeOptions) -> Result<Vec<u8>> {
    let ffi_opts = options.to_ffi();
    let mut output = ffi::NextImageBuffer::default();

    let status = unsafe {
        ffi::nextimage_gif2webp_alloc(
            gif_data.as_ptr(),
            gif_data.len(),
            &ffi_opts,
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

/// Convert WebP to GIF.
pub fn webp_to_gif(webp_data: &[u8]) -> Result<Vec<u8>> {
    let mut output = ffi::NextImageBuffer::default();

    let status = unsafe {
        ffi::nextimage_webp2gif_alloc(
            webp_data.as_ptr(),
            webp_data.len(),
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
