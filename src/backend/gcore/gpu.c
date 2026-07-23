#include "gpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

GPUBackend *gpu_init(int drm_fd, uint32_t width, uint32_t height,
                     uint32_t crtc_id, uint32_t connector_id, drmModeModeInfo mode) {
    GPUBackend *gpu = calloc(1, sizeof(GPUBackend));
    gpu->drm_fd = drm_fd;
    gpu->width = width;
    gpu->height = height;
    gpu->crtc_id = crtc_id;
    gpu->connector_id = connector_id;
    gpu->mode = mode;

    gpu->gbm = gbm_create_device(drm_fd);
    if (!gpu->gbm) { free(gpu); return NULL; }

    gpu->surface = gbm_surface_create(gpu->gbm, width, height, GBM_FORMAT_XRGB8888,
                                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gpu->surface) { gbm_device_destroy(gpu->gbm); free(gpu); return NULL; }

    EGLDisplay egl_display = eglGetDisplay(gpu->gbm);
    if (egl_display == EGL_NO_DISPLAY) {
        gbm_surface_destroy(gpu->surface);
        gbm_device_destroy(gpu->gbm);
        free(gpu);
        return NULL;
    }
    gpu->egl_display = egl_display;

    EGLint major, minor;
    if (!eglInitialize(egl_display, &major, &minor)) {
        gbm_surface_destroy(gpu->surface);
        gbm_device_destroy(gpu->gbm);
        free(gpu);
        return NULL;
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint config_attrs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config;
    EGLint num_configs;
    eglChooseConfig(egl_display, config_attrs, &config, 1, &num_configs);

    EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, ctx_attrs);
    if (egl_context == EGL_NO_CONTEXT) {
        eglTerminate(egl_display);
        gbm_surface_destroy(gpu->surface);
        gbm_device_destroy(gpu->gbm);
        free(gpu);
        return NULL;
    }
    gpu->egl_context = egl_context;

    EGLSurface egl_surface = eglCreateWindowSurface(egl_display, config, gpu->surface, NULL);
    if (egl_surface == EGL_NO_SURFACE) {
        eglDestroyContext(egl_display, egl_context);
        eglTerminate(egl_display);
        gbm_surface_destroy(gpu->surface);
        gbm_device_destroy(gpu->gbm);
        free(gpu);
        return NULL;
    }
    gpu->egl_surface = egl_surface;

    eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    gpu->active = true;
    return gpu;
}

void gpu_destroy(GPUBackend *gpu) {
    if (!gpu) return;
    eglMakeCurrent((EGLDisplay)gpu->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface((EGLDisplay)gpu->egl_display, (EGLSurface)gpu->egl_surface);
    eglDestroyContext((EGLDisplay)gpu->egl_display, (EGLContext)gpu->egl_context);
    eglTerminate((EGLDisplay)gpu->egl_display);
    gbm_surface_destroy(gpu->surface);
    gbm_device_destroy(gpu->gbm);
    free(gpu);
}

bool gpu_begin_frame(GPUBackend *gpu) {
    if (!gpu || !gpu->active) return false;
    eglMakeCurrent((EGLDisplay)gpu->egl_display, (EGLSurface)gpu->egl_surface, (EGLSurface)gpu->egl_surface, (EGLContext)gpu->egl_context);
    return true;
}

void gpu_end_frame(GPUBackend *gpu) {
    if (!gpu) return;
    eglSwapBuffers((EGLDisplay)gpu->egl_display, (EGLSurface)gpu->egl_surface);

    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gpu->surface);
    if (bo) {
        uint32_t handle = gbm_bo_get_handle(bo).u32;
        uint32_t stride = gbm_bo_get_stride(bo);
        uint32_t fb_id = 0;
        drmModeAddFB(gpu->drm_fd, gpu->width, gpu->height, 24, 32, stride, handle, &fb_id);
        if (fb_id) {
            drmModeSetCrtc(gpu->drm_fd, gpu->crtc_id, fb_id, 0, 0, &gpu->connector_id, 1, &gpu->mode);
            if (gpu->fb_id) drmModeRmFB(gpu->drm_fd, gpu->fb_id);
            gpu->fb_id = fb_id;
        }
        gbm_surface_release_buffer(gpu->surface, bo);
    }
}

void gpu_clear(GPUBackend *gpu, uint32_t color) {
    if (!gpu) return;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = (color & 0xFF) / 255.0f;
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void gpu_draw_rect(GPUBackend *gpu, int x, int y, int w, int h, uint32_t color) {
    if (!gpu) return;
    float fx = 2.0f * x / gpu->width - 1.0f;
    float fy = 1.0f - 2.0f * y / gpu->height;
    float fw = 2.0f * w / gpu->width;
    float fh = -2.0f * h / gpu->height;

    GLfloat vertices[] = { fx, fy, fx+fw, fy, fx, fy+fh, fx+fw, fy+fh };
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    float r = ((color>>16)&0xFF)/255.0f, g = ((color>>8)&0xFF)/255.0f, b = (color&0xFF)/255.0f;
    glUniform4f(0, r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
}

void gpu_draw_text(GPUBackend *gpu, int x, int y, const char *text, int size, uint32_t color) {
    (void)gpu; (void)x; (void)y; (void)text; (void)size; (void)color;
    /* GPU text rendering not yet implemented, falls back to CPU */
}

uint32_t *gpu_get_framebuffer(GPUBackend *gpu) {
    (void)gpu;
    return NULL;
}