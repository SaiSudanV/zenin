/*
 * Project Zenin - Universal Low-Memory 3D Graphics Engine (Z-GL)
 * Deterministic integer math rasterizer for extreme potato hardware.
 * Uses 100% integer calculations for screen coordinates to guarantee 0 FP traps.
 */

#ifndef ZENIN_ZGL_H
#define ZENIN_ZGL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "fb.h"

/* 3D Vertex representation using fixed integers for screen space */
typedef struct {
    int32_t x, y;     /* Screen space pixel coordinates */
    uint16_t depth;   /* 16-bit depth (0 to 65535) */
    uint16_t color;   /* 16-bit RGB565 vertex color */
} zgl_vertex_t;

/* 3D Triangle primitive */
typedef struct {
    zgl_vertex_t v0;
    zgl_vertex_t v1;
    zgl_vertex_t v2;
} zgl_triangle_t;

/* Initialize the Z-GL 3D Graphics subsystem */
void zgl_init(uint32_t width, uint32_t height);

/* Clear screen and 16-bit Depth / Z-Buffer */
void zgl_clear(uint16_t color);

/* Rasterize a filled 3D triangle with depth testing directly to VRAM */
void zgl_draw_triangle(const zgl_vertex_t *v0, const zgl_vertex_t *v1, const zgl_vertex_t *v2);

/* Universal GLES / Direct3D / Metal translation bridge */
void zgl_dispatch_draw_elements(uint32_t count, const zgl_triangle_t *triangles);

/* Query live 3D engine metrics */
void zgl_get_metrics(uint32_t *out_fps, uint32_t *out_poly_count, size_t *out_vram_kb);

#endif /* ZENIN_ZGL_H */
