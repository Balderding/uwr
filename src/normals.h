/* Normal maps made from the textures: the stones stand out of the mortar.
 *
 * The textures show masonry the same way throughout: dark mortar lines (darker than what is round them)
 * between lighter stones, each stone painted lit from the top left. So: mortar = pixels clearly darker
 * than their surroundings; a stone's height grows with the distance from the nearest mortar and levels
 * off (rounded edges), plus a little relief from the speckle painted inside it. The map is made at 4x
 * the texture's size, so the edges are smooth rather than stepped.
 *
 * Files: data\textures\walls_normal\NNN.png (256 x 256) and floors_normal\NNN.png (128 x 128), RGBA:
 * RGB = the normal (OpenGL convention: red right, green up, blue out of the wall), A = the height
 * (0 mortar, 255 the face of a stone). Made from data\textures\walls|floors\NNN.png when missing, so a
 * repainted texture gets a new map when its old map is deleted. */
#pragma once
#include "base.h"

#define NORMAL_SCALE 4
/* the relief's depth, the same in the world for every texture (2.2 texels of a 64-texel wall): a stone's face
 * stands this far out of its mortar */
#define NORMAL_DEPTH_TILES 0.035f

/* rgba: w x h, texels_per_tile of it in the world (64 walls, 32 floors); out: (w*4) x (h*4) RGBA.
 * bevel: texels over which a stone rises from its mortar. */
void normals_make(const u8 *rgba, int w, int h, float texels_per_tile, float bevel, u8 *out, float *heights);
/* the relief as geometry (data\textures\walls_mesh, floors_mesh\NNN.obj): the full detail (a height every half
 * texel) simplified to what the eye needs; heights w x h (0..1), units = the texture's size in the world's
 * texture coordinates (64 walls, 32 floors) */
bool relief_mesh_write(const char *path, const float *heights, int w, int h, float units);
/* writes the normal maps that are missing; returns how many, or -1 when a texture cannot be read */
int normals_missing(const char *data_dir);
