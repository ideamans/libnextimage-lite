//! Raw FFI bindings to libnextimage C library.
//!
//! These bindings are low-level and unsafe. Users should prefer the safe
//! high-level API provided by the `webp` and `avif` modules.

#![allow(dead_code)]

use std::os::raw::{c_char, c_int, c_uint, c_float};

/// Status codes returned by libnextimage functions
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImageStatus {
    Ok = 0,
    ErrorInvalidParam = -1,
    ErrorEncodeFailed = -2,
    ErrorDecodeFailed = -3,
    ErrorOutOfMemory = -4,
    ErrorUnsupported = -5,
    ErrorBufferTooSmall = -6,
}

/// Pixel format for decoded images
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImagePixelFormat {
    Rgba = 0,
    Rgb = 1,
    Bgra = 2,
    Yuv420 = 3,
    Yuv422 = 4,
    Yuv444 = 5,
}

/// Output buffer for encoded image data
#[repr(C)]
#[derive(Debug)]
pub struct NextImageBuffer {
    pub data: *mut u8,
    pub size: usize,
}

impl Default for NextImageBuffer {
    fn default() -> Self {
        Self {
            data: std::ptr::null_mut(),
            size: 0,
        }
    }
}

/// Decode buffer with plane information
#[repr(C)]
#[derive(Debug)]
pub struct NextImageDecodeBuffer {
    pub data: *mut u8,
    pub data_capacity: usize,
    pub data_size: usize,
    pub stride: usize,
    pub u_plane: *mut u8,
    pub u_capacity: usize,
    pub u_size: usize,
    pub u_stride: usize,
    pub v_plane: *mut u8,
    pub v_capacity: usize,
    pub v_size: usize,
    pub v_stride: usize,
    pub width: c_int,
    pub height: c_int,
    pub bit_depth: c_int,
    pub format: NextImagePixelFormat,
    pub owns_data: c_int,
}

impl Default for NextImageDecodeBuffer {
    fn default() -> Self {
        Self {
            data: std::ptr::null_mut(),
            data_capacity: 0,
            data_size: 0,
            stride: 0,
            u_plane: std::ptr::null_mut(),
            u_capacity: 0,
            u_size: 0,
            u_stride: 0,
            v_plane: std::ptr::null_mut(),
            v_capacity: 0,
            v_size: 0,
            v_stride: 0,
            width: 0,
            height: 0,
            bit_depth: 0,
            format: NextImagePixelFormat::Rgba,
            owns_data: 0,
        }
    }
}

// ============================================
// WebP types
// ============================================

/// WebP preset types
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImageWebPPreset {
    Default = 0,
    Picture = 1,
    Photo = 2,
    Drawing = 3,
    Icon = 4,
    Text = 5,
}

/// WebP image hint types
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImageWebPImageHint {
    Default = 0,
    Picture = 1,
    Photo = 2,
    Graph = 3,
}

/// WebP output format for decoding
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImageWebPOutputFormat {
    Png = 0,
    Jpeg = 1,
}

/// WebP encode options
#[repr(C)]
#[derive(Debug, Clone)]
pub struct NextImageWebPEncodeOptions {
    pub quality: c_float,
    pub lossless: c_int,
    pub method: c_int,
    pub preset: NextImageWebPPreset,
    pub image_hint: NextImageWebPImageHint,
    pub lossless_preset: c_int,
    pub target_size: c_int,
    pub target_psnr: c_float,
    pub segments: c_int,
    pub sns_strength: c_int,
    pub filter_strength: c_int,
    pub filter_sharpness: c_int,
    pub filter_type: c_int,
    pub autofilter: c_int,
    pub alpha_compression: c_int,
    pub alpha_filtering: c_int,
    pub alpha_quality: c_int,
    pub pass: c_int,
    pub show_compressed: c_int,
    pub preprocessing: c_int,
    pub partitions: c_int,
    pub partition_limit: c_int,
    pub emulate_jpeg_size: c_int,
    pub thread_level: c_int,
    pub low_memory: c_int,
    pub near_lossless: c_int,
    pub exact: c_int,
    pub use_delta_palette: c_int,
    pub use_sharp_yuv: c_int,
    pub qmin: c_int,
    pub qmax: c_int,
    pub keep_metadata: c_int,
    pub crop_x: c_int,
    pub crop_y: c_int,
    pub crop_width: c_int,
    pub crop_height: c_int,
    pub resize_width: c_int,
    pub resize_height: c_int,
    pub resize_mode: c_int,
    pub blend_alpha: c_uint,
    pub noalpha: c_int,
    pub allow_mixed: c_int,
    pub minimize_size: c_int,
    pub kmin: c_int,
    pub kmax: c_int,
    pub anim_loop_count: c_int,
    pub loop_compatibility: c_int,
}

/// WebP decode options
#[repr(C)]
#[derive(Debug, Clone)]
pub struct NextImageWebPDecodeOptions {
    pub output_format: NextImageWebPOutputFormat,
    pub jpeg_quality: c_int,
    pub use_threads: c_int,
    pub bypass_filtering: c_int,
    pub no_fancy_upsampling: c_int,
    pub format: NextImagePixelFormat,
    pub no_dither: c_int,
    pub dither_strength: c_int,
    pub alpha_dither: c_int,
    pub crop_x: c_int,
    pub crop_y: c_int,
    pub crop_width: c_int,
    pub crop_height: c_int,
    pub use_crop: c_int,
    pub resize_width: c_int,
    pub resize_height: c_int,
    pub use_resize: c_int,
    pub flip: c_int,
    pub alpha_only: c_int,
    pub incremental: c_int,
}

// ============================================
// AVIF types
// ============================================

/// AVIF output format for decoding
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NextImageAVIFOutputFormat {
    Png = 0,
    Jpeg = 1,
}

/// AVIF encode options
#[repr(C)]
#[derive(Debug, Clone)]
pub struct NextImageAVIFEncodeOptions {
    pub quality: c_int,
    pub quality_alpha: c_int,
    pub speed: c_int,
    pub min_quantizer: c_int,
    pub max_quantizer: c_int,
    pub min_quantizer_alpha: c_int,
    pub max_quantizer_alpha: c_int,
    pub bit_depth: c_int,
    pub yuv_format: c_int,
    pub yuv_range: c_int,
    pub enable_alpha: c_int,
    pub premultiply_alpha: c_int,
    pub tile_rows_log2: c_int,
    pub tile_cols_log2: c_int,
    pub color_primaries: c_int,
    pub transfer_characteristics: c_int,
    pub matrix_coefficients: c_int,
    pub sharp_yuv: c_int,
    pub target_size: c_int,
    pub exif_data: *const u8,
    pub exif_size: usize,
    pub xmp_data: *const u8,
    pub xmp_size: usize,
    pub icc_data: *const u8,
    pub icc_size: usize,
    pub irot_angle: c_int,
    pub imir_axis: c_int,
    pub pasp: [c_int; 2],
    pub crop: [c_int; 4],
    pub clap: [c_int; 8],
    pub clli_max_cll: c_int,
    pub clli_max_pall: c_int,
    pub timescale: c_int,
    pub keyframe_interval: c_int,
}

/// AVIF decode options
#[repr(C)]
#[derive(Debug, Clone)]
pub struct NextImageAVIFDecodeOptions {
    pub output_format: NextImageAVIFOutputFormat,
    pub jpeg_quality: c_int,
    pub use_threads: c_int,
    pub format: NextImagePixelFormat,
    pub ignore_exif: c_int,
    pub ignore_xmp: c_int,
    pub ignore_icc: c_int,
    pub image_size_limit: c_uint,
    pub image_dimension_limit: c_uint,
    pub strict_flags: c_int,
    pub chroma_upsampling: c_int,
    pub crop_x: c_int,
    pub crop_y: c_int,
    pub crop_width: c_int,
    pub crop_height: c_int,
    pub use_crop: c_int,
    pub resize_width: c_int,
    pub resize_height: c_int,
    pub use_resize: c_int,
}

// Opaque encoder/decoder types
pub enum NextImageWebPEncoder {}
pub enum NextImageWebPDecoder {}
pub enum NextImageAVIFEncoder {}
pub enum NextImageAVIFDecoder {}

extern "C" {
    // ============================================
    // Common functions
    // ============================================

    /// Free an output buffer allocated by the library
    pub fn nextimage_free_buffer(buffer: *mut NextImageBuffer);

    /// Free a decode buffer allocated by the library
    pub fn nextimage_free_decode_buffer(buffer: *mut NextImageDecodeBuffer);

    /// Get the last error message (thread-local)
    pub fn nextimage_last_error_message() -> *const c_char;

    /// Clear the last error message
    pub fn nextimage_clear_error();

    /// Get library version string
    pub fn nextimage_version() -> *const c_char;

    // ============================================
    // WebP functions
    // ============================================

    /// Initialize default WebP encode options
    pub fn nextimage_webp_default_encode_options(options: *mut NextImageWebPEncodeOptions);

    /// Initialize default WebP decode options
    pub fn nextimage_webp_default_decode_options(options: *mut NextImageWebPDecodeOptions);

    /// Encode image to WebP (library allocates output)
    pub fn nextimage_webp_encode_alloc(
        input_data: *const u8,
        input_size: usize,
        options: *const NextImageWebPEncodeOptions,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Decode WebP image (library allocates output)
    pub fn nextimage_webp_decode_alloc(
        webp_data: *const u8,
        webp_size: usize,
        options: *const NextImageWebPDecodeOptions,
        output: *mut NextImageDecodeBuffer,
    ) -> NextImageStatus;

    /// Convert GIF to WebP (library allocates output)
    pub fn nextimage_gif2webp_alloc(
        gif_data: *const u8,
        gif_size: usize,
        options: *const NextImageWebPEncodeOptions,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Convert WebP to GIF (library allocates output)
    pub fn nextimage_webp2gif_alloc(
        webp_data: *const u8,
        webp_size: usize,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Create a WebP encoder instance
    pub fn nextimage_webp_encoder_create(
        options: *const NextImageWebPEncodeOptions,
    ) -> *mut NextImageWebPEncoder;

    /// Encode with a WebP encoder instance
    pub fn nextimage_webp_encoder_encode(
        encoder: *mut NextImageWebPEncoder,
        input_data: *const u8,
        input_size: usize,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Destroy a WebP encoder instance
    pub fn nextimage_webp_encoder_destroy(encoder: *mut NextImageWebPEncoder);

    /// Create a WebP decoder instance
    pub fn nextimage_webp_decoder_create(
        options: *const NextImageWebPDecodeOptions,
    ) -> *mut NextImageWebPDecoder;

    /// Decode with a WebP decoder instance
    pub fn nextimage_webp_decoder_decode(
        decoder: *mut NextImageWebPDecoder,
        webp_data: *const u8,
        webp_size: usize,
        output: *mut NextImageDecodeBuffer,
    ) -> NextImageStatus;

    /// Destroy a WebP decoder instance
    pub fn nextimage_webp_decoder_destroy(decoder: *mut NextImageWebPDecoder);

    // ============================================
    // AVIF functions
    // ============================================

    /// Initialize default AVIF encode options
    pub fn nextimage_avif_default_encode_options(options: *mut NextImageAVIFEncodeOptions);

    /// Initialize default AVIF decode options
    pub fn nextimage_avif_default_decode_options(options: *mut NextImageAVIFDecodeOptions);

    /// Encode image to AVIF (library allocates output)
    pub fn nextimage_avif_encode_alloc(
        input_data: *const u8,
        input_size: usize,
        options: *const NextImageAVIFEncodeOptions,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Decode AVIF image (library allocates output)
    pub fn nextimage_avif_decode_alloc(
        avif_data: *const u8,
        avif_size: usize,
        options: *const NextImageAVIFDecodeOptions,
        output: *mut NextImageDecodeBuffer,
    ) -> NextImageStatus;

    /// Create an AVIF encoder instance
    pub fn nextimage_avif_encoder_create(
        options: *const NextImageAVIFEncodeOptions,
    ) -> *mut NextImageAVIFEncoder;

    /// Encode with an AVIF encoder instance
    pub fn nextimage_avif_encoder_encode(
        encoder: *mut NextImageAVIFEncoder,
        input_data: *const u8,
        input_size: usize,
        output: *mut NextImageBuffer,
    ) -> NextImageStatus;

    /// Destroy an AVIF encoder instance
    pub fn nextimage_avif_encoder_destroy(encoder: *mut NextImageAVIFEncoder);

    /// Create an AVIF decoder instance
    pub fn nextimage_avif_decoder_create(
        options: *const NextImageAVIFDecodeOptions,
    ) -> *mut NextImageAVIFDecoder;

    /// Decode with an AVIF decoder instance
    pub fn nextimage_avif_decoder_decode(
        decoder: *mut NextImageAVIFDecoder,
        avif_data: *const u8,
        avif_size: usize,
        output: *mut NextImageDecodeBuffer,
    ) -> NextImageStatus;

    /// Destroy an AVIF decoder instance
    pub fn nextimage_avif_decoder_destroy(decoder: *mut NextImageAVIFDecoder);
}
