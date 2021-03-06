/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "image.h"

/*
 * Work-around for conflicts between windows.h and jpeglib.h.
 *
 * If ADDRESS_TAG_BIT is defined then BaseTsd.h has been included and INT32
 * has been defined with a typedef, so we must define XMD_H to prevent the JPEG
 * header from defining it again.
 *
 * Next up, jmorecfg.h defines the 'boolean' type as int, which conflicts with
 * the standard Windows 'boolean' definition as byte.
 *
 * @see http://www.asmail.be/msg0054688232.html
 */

#if defined(_WIN32)
typedef byte boolean;
#define HAVE_BOOLEAN
#endif

#if defined(_WIN32) && defined(ADDRESS_TAG_BIT) && !defined(XMD_H)
#define XMD_H
#define VTK_JPEG_XMD_H
#endif
#include <jpeglib.h>
#if defined(VTK_JPEG_XMD_H)
#undef VTK_JPEG_XMD_H
#undef XMD_H
#endif

#define IMG_PALETTE "pics/colormap"

img_palette_t img_palette;
static _Bool img_palette_initialized;

// RGBA color masks
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000

// image formats, tried in this order
static const char *img_formats[] = { "tga", "png", "jpg", "wal", "pcx", NULL };

/**
 * @brief A helper which mangles a .wal file into an SDL_Surface suitable for
 * OpenGL uploads and other basic manipulations.
 */
static _Bool Img_LoadWal(const char *path, SDL_Surface **surf) {
	void *buf;

	*surf = NULL;

	if (Fs_Load(path, &buf) == -1)
		return false;

	d_wal_t *wal = (d_wal_t *) buf;

	wal->width = LittleLong(wal->width);
	wal->height = LittleLong(wal->height);

	wal->offsets[0] = LittleLong(wal->offsets[0]);

	if (!img_palette_initialized) // lazy-load palette if necessary
		Img_InitPalette();

	size_t size = wal->width * wal->height;
	uint32_t *p = (uint32_t *) malloc(size * sizeof(uint32_t));

	const byte *b = (byte *) wal + wal->offsets[0];
	for (size_t i = 0; i < size; i++) { // convert to 32bpp RGBA via palette
		if (b[i] == 255) // transparent
			p[i] = 0;
		else
			p[i] = img_palette[b[i]];
	}

	Fs_Free(buf);

	// create the RGBA surface
	if ((*surf = SDL_CreateRGBSurfaceFrom(p, wal->width, wal->height, 32, 0,
			RMASK, GMASK, BMASK, AMASK))) {

		// trick SDL into freeing the pixel data with the surface
		(*surf)->flags &= ~SDL_PREALLOC;
	}

	return *surf != NULL;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface.
 */
static _Bool Img_LoadTypedImage(const char *name, const char *type, SDL_Surface **surf) {
	char path[MAX_QPATH];
	void *buf;
	int64_t len;

	g_snprintf(path, sizeof(path), "%s.%s", name, type);

	if (!g_strcmp0(type, "wal")) { // special case for .wal files
		return Img_LoadWal(path, surf);
	}

	*surf = NULL;

	if ((len = Fs_Load(path, &buf)) != -1) {

		SDL_RWops *rw;
		if ((rw = SDL_RWFromMem(buf, len))) {

			SDL_Surface *s;
			if ((s = IMG_LoadTyped_RW(rw, 0, (char *) type))) {

				if (!g_str_has_prefix(path, IMG_PALETTE)) {
					*surf = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
					SDL_FreeSurface(s);
				} else {
					*surf = s;
				}
			}
			SDL_FreeRW(rw);
		}
		Fs_Free(buf);
	}

	return *surf != NULL;
}

/**
 * @brief Loads the specified image from the game filesystem and populates
 * the provided SDL_Surface. Image formats are tried in the order they appear
 * in TYPES.
 */
_Bool Img_LoadImage(const char *name, SDL_Surface **surf) {

	int32_t i = 0;
	while (img_formats[i]) {
		if (Img_LoadTypedImage(name, img_formats[i++], surf))
			return true;
	}

	return false;
}

/**
 * @brief Initializes the 8bit color palette required for .wal texture loading.
 */
void Img_InitPalette(void) {
	SDL_Surface *surf;

	if (!Img_LoadTypedImage(IMG_PALETTE, "pcx", &surf))
		return;

	for (size_t i = 0; i < lengthof(img_palette); i++) {
		const byte r = surf->format->palette->colors[i].r;
		const byte g = surf->format->palette->colors[i].g;
		const byte b = surf->format->palette->colors[i].b;

		const uint32_t v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		img_palette[i] = LittleLong(v);
	}

	img_palette[lengthof(img_palette) - 1] &= LittleLong(0xffffff); // 255 is transparent

	SDL_FreeSurface(surf);

	img_palette_initialized = true;
}

/**
 * @brief Returns RGB components of the specified color in the specified result array.
 */
void Img_ColorFromPalette(uint8_t c, vec_t *res) {

	if (!img_palette_initialized) // lazy-load palette if necessary
		Img_InitPalette();

	const uint32_t color = img_palette[c];

	res[0] = (color >> 0 & 255) / 255.0;
	res[1] = (color >> 8 & 255) / 255.0;
	res[2] = (color >> 16 & 255) / 255.0;
}

/**
 * @brief Write pixel data to a JPEG file.
 */
_Bool Img_WriteJPEG(const char *path, byte *data, uint32_t width, uint32_t height, int32_t quality) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
	FILE *f;

	const char *real_path = Fs_RealPath(path);

	if (!(f = fopen(real_path, "wb"))) {
		Com_Print("Failed to open to %s\n", real_path);
		return false;
	}

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo, f);

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);

	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	const uint32_t stride = width * 3; // bytes per scanline

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &data[(cinfo.image_height - cinfo.next_scanline - 1) * stride];
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);

	jpeg_destroy_compress(&cinfo);

	fclose(f);
	return true;
}
