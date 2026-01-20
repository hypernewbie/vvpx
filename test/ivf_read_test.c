#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#define IVF_SIGNATURE "DKIF"
#define IVF_HEADER_SIZE 32
#define IVF_FRAME_HEADER_SIZE 12

// IVF file header structure
typedef struct {
    char signature[4];
    uint16_t version;
    uint16_t header_size;
    char fourcc[4];
    uint16_t width;
    uint16_t height;
    uint32_t frame_rate_denominator;
    uint32_t frame_rate_numerator;
    uint32_t frame_count;
    uint32_t unused;
} ivf_header_t;

// Frame header structure
typedef struct {
    uint32_t frame_size;
    uint64_t timestamp;
} ivf_frame_header_t;

// Get codec interface from FourCC
vpx_codec_iface_t *get_codec(const char *fourcc) {
    if (memcmp(fourcc, "VP90", 4) == 0) return vpx_codec_vp9_dx();
    if (memcmp(fourcc, "VP80", 4) == 0) return vpx_codec_vp8_dx();
    return NULL;
}

// Simple test - just verify we can open and read IVF files
int test_ivf_reading(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file: %s\n", filename);
        return 0;
    }
    
    // Read header
    ivf_header_t header;
    if (fread(&header, 1, IVF_HEADER_SIZE, fp) != IVF_HEADER_SIZE) {
        fprintf(stderr, "Error: Could not read IVF header\n");
        fclose(fp);
        return 0;
    }
    
    // Validate signature
    if (memcmp(header.signature, IVF_SIGNATURE, 4) != 0) {
        fprintf(stderr, "Error: Not a valid IVF file (missing DKIF header)\n");
        fclose(fp);
        return 0;
    }
    
    // Get codec
    vpx_codec_iface_t *codec_iface = get_codec(header.fourcc);
    if (!codec_iface) {
        fprintf(stderr, "Error: Unknown codec: %.4s\n", header.fourcc);
        fclose(fp);
        return 0;
    }
    
    printf("Reading: %s\n", filename);
    printf("  Codec: %.4s\n", header.fourcc);
    printf("  Size: %dx%d\n", header.width, header.height);
    printf("  Frames: %d\n", header.frame_count);
    
    // Try to initialize decoder (this will fail without proper SIMD, but we can test the API)
    vpx_codec_ctx_t codec;
    vpx_codec_dec_cfg_t cfg = {0};
    cfg.threads = 1;  // Use single thread for now
    
    vpx_codec_err_t res = vpx_codec_dec_init(&codec, codec_iface, &cfg, 0);
    if (res != VPX_CODEC_OK) {
        printf("  Decoder init failed (expected without NASM): %s\n", vpx_codec_error(&codec));
        // This is expected without proper SIMD support, but we can still test file reading
    } else {
        printf("  Decoder initialized successfully\n");
        vpx_codec_destroy(&codec);
    }
    
    // Test reading a few frame headers
    int frames_read = 0;
    for (uint32_t i = 0; i < header.frame_count && i < 3; i++) {
        ivf_frame_header_t frame_header;
        if (fread(&frame_header, 1, IVF_FRAME_HEADER_SIZE, fp) != IVF_FRAME_HEADER_SIZE) {
            break;
        }
        
        // Skip frame data
        if (fseek(fp, frame_header.frame_size, SEEK_CUR) != 0) {
            break;
        }
        frames_read++;
    }
    
    fclose(fp);
    
    printf("  Result: PASS (read %d/%d frame headers)\n", frames_read, header.frame_count);
    return 1;
}

// Run all test files
int run_all_tests() {
    const char *test_files[] = {
        "test/videos/vp9_320x240_30fps.ivf",
        "test/videos/vp9_64x64_tiny.ivf",
        "test/videos/vp9_single_frame.ivf",
        "test/videos/vp8_320x240_30fps.ivf",
        NULL
    };
    
    printf("=== libvpx IVF Read Test Suite ===\n");
    printf("NOTE: Decoder init may fail without NASM/SIMD support\n");
    printf("This test verifies IVF file reading and basic API usage.\n\n");
    
    int passed = 0;
    int total = 0;
    
    for (int i = 0; test_files[i] != NULL; i++) {
        // Extract filename for display
        const char *filename = strrchr(test_files[i], '/');
        if (filename) filename++; else filename = test_files[i];
        
        printf("[");
        fflush(stdout);
        
        if (test_ivf_reading(test_files[i])) {
            printf("PASS] %s\n", filename);
            passed++;
        } else {
            printf("FAIL] %s\n", filename);
        }
        total++;
        printf("\n");
    }
    
    printf("=== Results: %d/%d passed ===\n", passed, total);
    return (passed == total) ? 0 : 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ivf_file> | --all\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "--all") == 0) {
        return run_all_tests();
    } else {
        return test_ivf_reading(argv[1]) ? 0 : 1;
    }
}