#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
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

// Read IVF file header
int read_ivf_header(FILE *fp, ivf_header_t *header) {
    if (fread(header, 1, IVF_HEADER_SIZE, fp) != IVF_HEADER_SIZE) {
        return 0;
    }
    
    // Validate signature
    if (memcmp(header->signature, IVF_SIGNATURE, 4) != 0) {
        return 0;
    }
    
    return 1;
}

// Read frame header and data
int read_frame(FILE *fp, uint8_t **frame_data, uint32_t *frame_size, uint64_t *timestamp) {
    ivf_frame_header_t frame_header;
    
    if (fread(&frame_header, 1, IVF_FRAME_HEADER_SIZE, fp) != IVF_FRAME_HEADER_SIZE) {
        return 0;
    }
    
    *frame_size = frame_header.frame_size;
    *timestamp = frame_header.timestamp;
    
    *frame_data = (uint8_t*)malloc(*frame_size);
    if (!*frame_data) {
        return 0;
    }
    
    if (fread(*frame_data, 1, *frame_size, fp) != *frame_size) {
        free(*frame_data);
        return 0;
    }
    
    return 1;
}

// Decode a single IVF file
int decode_ivf_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open file: %s\n", filename);
        return 0;
    }
    
    // Read header
    ivf_header_t header;
    if (!read_ivf_header(fp, &header)) {
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
    
    // Initialize decoder
    vpx_codec_ctx_t codec;
    vpx_codec_dec_cfg_t cfg = {0};
    cfg.threads = 4;
    
    vpx_codec_err_t res = vpx_codec_dec_init(&codec, codec_iface, &cfg, 0);
    if (res != VPX_CODEC_OK) {
        fprintf(stderr, "Error: Decoder init failed: %s\n", vpx_codec_error(&codec));
        fclose(fp);
        return 0;
    }
    
    // Print file info
    printf("Decoding: %s\n", filename);
    printf("  Codec: %.4s\n", header.fourcc);
    printf("  Size: %dx%d\n", header.width, header.height);
    printf("  Frames: %d\n", header.frame_count);
    
    clock_t start_time = clock();
    int decoded_frames = 0;
    int failed_frames = 0;
    
    // Decode frames
    for (uint32_t i = 0; i < header.frame_count; i++) {
        uint8_t *frame_data = NULL;
        uint32_t frame_size = 0;
        uint64_t timestamp = 0;
        
        if (!read_frame(fp, &frame_data, &frame_size, &timestamp)) {
            fprintf(stderr, "Error: Failed to read frame %d\n", i);
            failed_frames++;
            break;
        }
        
        // Decode frame
        res = vpx_codec_decode(&codec, frame_data, frame_size, NULL, 0);
        if (res != VPX_CODEC_OK) {
            fprintf(stderr, "Error: Decode failed at frame %d: %s\n", 
                    i, vpx_codec_error_detail(&codec));
            failed_frames++;
        } else {
            decoded_frames++;
            
            // Get decoded frame(s)
            vpx_codec_iter_t iter = NULL;
            vpx_image_t *img;
            while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
                // Frame decoded successfully
            }
        }
        
        free(frame_data);
    }
    
    clock_t end_time = clock();
    double decode_time = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;
    
    // Cleanup
    vpx_codec_destroy(&codec);
    fclose(fp);
    
    // Report results
    if (failed_frames == 0 && decoded_frames == header.frame_count) {
        printf("  Result: PASS (%d frames decoded in %.0fms)\n", decoded_frames, decode_time);
        return 1;
    } else {
        printf("  Result: FAIL (%d/%d frames decoded, %d failed)\n", 
               decoded_frames, header.frame_count, failed_frames);
        return 0;
    }
}

// Run all test files
int run_all_tests() {
    const char *test_files[] = {
        "videos/vp9_320x240_30fps.ivf",
        "videos/vp9_64x64_tiny.ivf",
        "videos/vp9_720p.ivf",
        "videos/vp9_odd_dimensions.ivf",
        "videos/vp9_high_quality.ivf",
        "videos/vp9_low_quality.ivf",
        "videos/vp9_single_frame.ivf",
        "videos/vp9_60fps.ivf",
        "videos/vp8_320x240_30fps.ivf",
        "videos/vp8_640x480.ivf",
        "videos/vp9_colorbars.ivf",
        "videos/vp9_solid_blue.ivf",
        "videos/vp9_noise.ivf",
        NULL
    };
    
    printf("=== libvpx Decode Test Suite ===\n\n");
    
    int passed = 0;
    int total = 0;
    
    for (int i = 0; test_files[i] != NULL; i++) {
        // Extract filename for display
        const char *filename = strrchr(test_files[i], '/');
        if (filename) filename++; else filename = test_files[i];
        
        printf("[");
        fflush(stdout);
        
        if (decode_ivf_file(test_files[i])) {
            printf("PASS] %s\n", filename);
            passed++;
        } else {
            printf("FAIL] %s\n", filename);
        }
        total++;
    }
    
    printf("\n=== Results: %d/%d passed ===\n", passed, total);
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
        return decode_ivf_file(argv[1]) ? 0 : 1;
    }
}