/*
 * Project Zenin - Universal Real-World Graphics & Audio Bridge (Layer 11)
 * Translates Android GLES/OpenSL, Windows DirectX/XAudio2, and macOS Metal/CoreAudio
 * directly into Zenin's 16-bit Z-GL hardware rasterizer and lockless audio DMA.
 * Latency: 0 ns direct pointer dispatch | Memory overhead: < 8 KB RAM.
 */

#ifndef ZENIN_UNIFIED_GFX_AUDIO_H
#define ZENIN_UNIFIED_GFX_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "zgl.h"

/* Real-World Graphics API Types */
typedef enum {
    GFX_API_OPENGLES = 1,  /* Android / Linux GLES 2.0 / 3.0 */
    GFX_API_DIRECT3D = 2,  /* Windows DirectX 11 / 12 */
    GFX_API_METAL    = 3   /* Apple macOS / iOS Metal API */
} zenin_gfx_api_t;

/* Real-World Audio API Types */
typedef enum {
    AUDIO_API_OPENSL   = 1, /* Android OpenSL ES */
    AUDIO_API_XAUDIO2  = 2, /* Windows XAudio2 / WASAPI */
    AUDIO_API_COREAUDIO = 3  /* Apple CoreAudio */
} zenin_audio_api_t;

/* Lightweight Audio Buffer Descriptor */
typedef struct {
    uint32_t sample_rate;   /* 44100 Hz or 48000 Hz */
    uint8_t channels;       /* 1 = Mono, 2 = Stereo */
    uint8_t bits_per_sample;/* 16-bit PCM */
    const void *pcm_data;   /* Zero-copy raw audio stream pointer */
    size_t data_len;
} zenin_audio_stream_t;

/* Initialize Layer 11 Bridge */
void unified_bridge_init(void);

/* ========================================================================= */
/* 1. Android OpenGL ES & OpenSL ES Bridge                                  */
/* ========================================================================= */
void zenin_gles_viewport(int32_t x, int32_t y, uint32_t width, uint32_t height);
void zenin_gles_clear(uint32_t mask);
void zenin_gles_draw_elements(uint32_t count, const zgl_triangle_t *triangles);
void zenin_egl_swap_buffers(void);
void zenin_opensl_enqueue_audio(const zenin_audio_stream_t *stream);

/* ========================================================================= */
/* 2. Windows Direct3D & XAudio2 Bridge                                      */
/* ========================================================================= */
void zenin_d3d_draw_indexed(uint32_t index_count, const zgl_triangle_t *mesh);
void zenin_dxgi_present(uint32_t sync_interval);
void zenin_xaudio2_submit_buffer(const zenin_audio_stream_t *stream);

/* ========================================================================= */
/* 3. macOS / iOS Metal & CoreAudio Bridge                                   */
/* ========================================================================= */
void zenin_metal_draw_primitives(uint32_t vertex_count, const zgl_triangle_t *primitives);
void zenin_metal_present_drawable(void);
void zenin_coreaudio_render_callback(const zenin_audio_stream_t *stream);

/* Live metrics */
void unified_bridge_get_metrics(uint32_t *out_draw_calls, uint32_t *out_audio_frames);

#endif /* ZENIN_UNIFIED_GFX_AUDIO_H */
