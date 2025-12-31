//! Integration tests for nextimage.
//!
//! These tests require the native library to be installed.
//! Run with: LIBNEXTIMAGE_LIB_DIR=../lib/darwin-arm64 cargo test

use nextimage::{avif, webp};

const TEST_PNG: &[u8] = include_bytes!("../../testdata/png/red.png");

#[test]
#[ignore = "requires native library"]
fn test_webp_encode() {
    let encoder = webp::WebPEncoder::new(webp::WebPEncodeOptions::default()).unwrap();
    let webp_data = encoder.encode(TEST_PNG).unwrap();

    // Check WebP magic bytes
    assert!(webp_data.len() > 12);
    assert_eq!(&webp_data[0..4], b"RIFF");
    assert_eq!(&webp_data[8..12], b"WEBP");
}

#[test]
#[ignore = "requires native library"]
fn test_webp_encode_lossless() {
    let encoder = webp::WebPEncoder::new(webp::WebPEncodeOptions::lossless()).unwrap();
    let webp_data = encoder.encode(TEST_PNG).unwrap();

    assert!(webp_data.len() > 12);
    assert_eq!(&webp_data[0..4], b"RIFF");
}

#[test]
#[ignore = "requires native library"]
fn test_webp_roundtrip() {
    // Encode to WebP
    let encoder = webp::WebPEncoder::new(webp::WebPEncodeOptions::lossless()).unwrap();
    let webp_data = encoder.encode(TEST_PNG).unwrap();

    // Decode back to PNG
    let decoder = webp::WebPDecoder::new(webp::WebPDecodeOptions::default()).unwrap();
    let png_data = decoder.decode(&webp_data).unwrap();

    // Check PNG magic bytes
    assert!(png_data.len() > 8);
    assert_eq!(&png_data[0..8], b"\x89PNG\r\n\x1a\n");
}

#[test]
#[ignore = "requires native library"]
fn test_avif_encode() {
    let encoder = avif::AVIFEncoder::new(avif::AVIFEncodeOptions::default()).unwrap();
    let avif_data = encoder.encode(TEST_PNG).unwrap();

    // Check for AVIF/HEIF signature (ftyp box)
    assert!(avif_data.len() > 12);
    // AVIF files start with ftyp box
    assert_eq!(&avif_data[4..8], b"ftyp");
}

#[test]
#[ignore = "requires native library"]
fn test_avif_roundtrip() {
    // Encode to AVIF
    let encoder = avif::AVIFEncoder::new(avif::AVIFEncodeOptions::default()).unwrap();
    let avif_data = encoder.encode(TEST_PNG).unwrap();

    // Decode back to PNG
    let decoder = avif::AVIFDecoder::new(avif::AVIFDecodeOptions::default()).unwrap();
    let png_data = decoder.decode(&avif_data).unwrap();

    // Check PNG magic bytes
    assert!(png_data.len() > 8);
    assert_eq!(&png_data[0..8], b"\x89PNG\r\n\x1a\n");
}

#[test]
#[ignore = "requires native library"]
fn test_convenience_functions() {
    // Test webp::encode
    let webp_data = webp::encode(TEST_PNG).unwrap();
    assert!(webp_data.len() > 12);

    // Test avif::encode
    let avif_data = avif::encode(TEST_PNG).unwrap();
    assert!(avif_data.len() > 12);
}

#[test]
fn test_options_default() {
    // These don't require the native library
    let webp_opts = webp::WebPEncodeOptions::default();
    assert_eq!(webp_opts.quality, 75.0);
    assert!(!webp_opts.lossless);

    let avif_opts = avif::AVIFEncodeOptions::default();
    assert_eq!(avif_opts.quality, 60);
    assert_eq!(avif_opts.speed, 6);
}

#[test]
fn test_platform_detection() {
    let platform = nextimage::get_platform();
    assert!(nextimage::PLATFORMS.iter().any(|p| platform == *p));
}

#[test]
fn test_cache_dir() {
    let cache_dir = nextimage::get_cache_dir();
    assert!(cache_dir.is_some());
}
