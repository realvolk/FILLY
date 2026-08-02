#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    gcore_init_font(NULL, 14);
    
    PixelBuffer pb;
    pb.width = 640; pb.height = 480; pb.stride = 640;
    pb.pixels = calloc(pb.width * pb.height, sizeof(uint32_t));
    
    // Draw solid red rectangle
    for (int row = 100; row < 200; row++) {
        uint32_t *line = pb.pixels + row * pb.width;
        for (int col = 100; col < 300; col++) {
            line[col] = gcore_rgba(255, 0, 0, 255);
        }
    }
    
    FILE *f = fopen("pixel_test.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", pb.width, pb.height);
    for (int i = 0; i < pb.width * pb.height; i++) {
        uint32_t p = pb.pixels[i];
        fputc((p >> 16) & 0xFF, f);
        fputc((p >> 8) & 0xFF, f);
        fputc(p & 0xFF, f);
    }
    fclose(f);
    
    free(pb.pixels);
    gcore_shutdown_font();
    printf("Wrote pixel_test.ppm\n");
    return 0;
}