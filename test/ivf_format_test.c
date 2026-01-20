#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

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
int read_frame_header(FILE *fp, uint32_t *frame_size, uint64_t *timestamp) {
    ivf_frame_header_t frame_header;
    
    if (fread(&frame_header, 1, IVF_FRAME_HEADER_SIZE, fp) != IVF_FRAME_HEADER_SIZE) {
        return 0;
    }
    
    *frame_size = frame_header.frame_size;
    *timestamp = frame_header.timestamp;
    
    return 1;
}

// Test a single IVF file
int test_ivf_file(const char *filename) {
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
    
    printf("Testing: %s\n", filename);
    printf("  Codec: %.4s\n", header.fourcc);
    printf("  Size: %dx%d\n", header.width, header.height);
    printf("  Frames: %d\n", header.frame_count);
    
    clock_t start_time = clock();
    int frames_read = 0;
    
    // Test reading frame headers
    for (uint32_t i = 0; i < header.frame_count; i++) {
        uint32_t frame_size = 0;
        uint64_t timestamp = 0;
        
        if (!read_frame_header(fp, &frame_size, &timestamp)) {
            fprintf(stderr, "Error: Failed to read frame %d header\n", i);
            break;
        }
        
        // Skip frame data
        if (fseek(fp, frame_size, SEEK_CUR) != 0) {
            fprintf(stderr, "Error: Failed to skip frame %d data\n", i);
            break;
        }
        
        frames_read++;
    }
    
    clock_t end_time = clock();
    double read_time = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;
    
    fclose(fp);
    
    if (frames_read == header.frame_count) {
        printf("  Result: PASS (read %d frame headers in %.0fms)\n", frames_read, read_time);
        return 1;
    } else {
        printf("  Result: FAIL (read %d/%d frame headers)\n", frames_read, header.frame_count);
        return 0;
    }
}

// Run all test files
int run_all_tests() {
    const char *test_files[] = {
        "test/videos/vp9_320x240_30fps.ivf",
        "test/videos/vp9_64x64_tiny.ivf",
        "test/videos/vp9_720p.ivf",
        "test/videos/vp9_odd_dimensions.ivf",
        "test/videos/vp9_high_quality.ivf",
        "test/videos/vp9_low_quality.ivf",
        "test/videos/vp9_single_frame.ivf",
        "test/videos/vp9_60fps.ivf",
        "test/videos/vp8_320x240_30fps.ivf",
        "test/videos/vp8_640x480.ivf",
        "test/videos/vp9_colorbars.ivf",
        "test/videos/vp9_solid_blue.ivf",
        "test/videos/vp9_noise.ivf",
        NULL
    };
    
    printf("=== IVF Container Format Test Suite ===\n");
    printf("This test verifies IVF file format parsing without decoding.\n\n");
    
    int passed = 0;
    int total = 0;
    
    for (int i = 0; test_files[i] != NULL; i++) {
        // Extract filename for display
        const char *filename = strrchr(test_files[i], '/');
        if (filename) filename++; else filename = test_files[i];
        
        printf("[");
        fflush(stdout);
        
        if (test_ivf_file(test_files[i])) {
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
        return test_ivf_file(argv[1]) ? 0 : 1;
    }
}