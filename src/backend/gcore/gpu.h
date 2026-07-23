#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <drm.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct GPUBackend {
    struct gbm_device *gbm;
    struct gbm_surface *surface;
    void *egl_display;
    void *egl_context;
    void *egl_surface;
    uint32_t width, height;
    uint32_t fb_id;
    int drm_fd;
    uint32_t crtc_id;
    uint32_t connector_id;
    drmModeModeInfo mode;
    bool active;
} GPUBackend;

GPUBackend *gpu_init(int drm_fd, uint32_t width, uint32_t height,
                     uint32_t crtc_id, uint32_t connector_id, drmModeModeInfo mode);
void gpu_destroy(GPUBackend *gpu);
bool gpu_begin_frame(GPUBackend *gpu);
void gpu_end_frame(GPUBackend *gpu);
void gpu_clear(GPUBackend *gpu, uint32_t color);
void gpu_draw_rect(GPUBackend *gpu, int x, int y, int w, int h, uint32_t color);
void gpu_draw_text(GPUBackend *gpu, int x, int y, const char *text, int size, uint32_t color);
uint32_t *gpu_get_framebuffer(GPUBackend *gpu);