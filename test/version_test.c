#include "vpx/vpx_codec.h"
#include <stdio.h>

int main(void) {
    printf("libvpx version: %s\n", vpx_codec_version_str());
    
    // Basic sanity check
    int version = vpx_codec_version();
    if (version == 0) {
        printf("ERROR: version returned 0\n");
        return 1;
    }
    
    printf("Version number: %d.%d.%d\n",
           (version >> 16) & 0xFF,
           (version >> 8) & 0xFF,
           version & 0xFF);
    
    return 0;
}