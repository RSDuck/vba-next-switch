#include <malloc.h>
#include <string.h>

#include <arm_neon.h>

#include "zoom.h"

#define TMP_BUFFERS 4

static void *zoomTmpBuffers[TMP_BUFFERS];
static int zoomTmpBufferSize[TMP_BUFFERS];
static int zoomTmpBuffersNext = 0;

void zoomInit(int maxWidth, int maxHeight) {
	for (int i = 0; i < TMP_BUFFERS; i++) {
		zoomTmpBuffers[i] = NULL;
		zoomTmpBufferSize[i] = 0;
	}
}
void zoomDeinit() {
	for (int i = 0; i < TMP_BUFFERS; i++) free(zoomTmpBuffers[i]);
}

static inline void *tmpBufferAlloc(size_t size) {
	if (zoomTmpBufferSize[zoomTmpBuffersNext] < size) {
		zoomTmpBuffers[zoomTmpBuffersNext] = realloc(zoomTmpBuffers[zoomTmpBuffersNext], size);
		zoomTmpBufferSize[zoomTmpBuffersNext] = size;
	}
	return zoomTmpBuffers[zoomTmpBuffersNext++];
}
static inline void tmpBuffersReset() { zoomTmpBuffersNext = 0; }

/*

	The following image scaling function is based of SDL_gfx, but detached from SDL and other dependencies
		https://sourceforge.net/projects/sdlgfx/

	Relevant file:
		https://sourceforge.net/p/sdlgfx/code/HEAD/tree/SDL_rotozoom.c

	What follows is the license header of SDL_rotozoom.c:
*/
/*

SDL_rotozoom.c: rotozoomer, zoomer and shrinker for 32bit or 8bit surfaces

Copyright (C) 2001-2012  Andreas Schiffler

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.

Andreas Schiffler -- aschiffler at ferzkopp dot net

*/
int zoomSurfaceRGBA(Surface *src, Surface *dst, int flipx, int flipy, int smooth) {
	int x, y, sx, sy, ssx, ssy, *sax, *say, *csax, *csay, *salast, csx, csy, ex, ey, cx, cy, sstep, sstepx, sstepy;
	ColorRGBA *c00, *c01, *c10, *c11;
	ColorRGBA *sp, *csp, *dp;
	int spixelgap, spixelw, spixelh, dgap, t1, t2;

	/*
	 * Allocate memory for row/column increments
	 */
	if ((sax = (int *)tmpBufferAlloc((dst->w + 1) * sizeof(uint32_t))) == NULL) {
		return (-1);
	}
	if ((say = (int *)tmpBufferAlloc((dst->h + 1) * sizeof(uint32_t))) == NULL) {
		return (-1);
	}

	/*
	 * Precalculate row increments
	 */
	spixelw = (src->w - 1);
	spixelh = (src->h - 1);
	if (smooth) {
		sx = (int)(65536.0 * (float)spixelw / (float)(dst->w - 1));
		sy = (int)(65536.0 * (float)spixelh / (float)(dst->h - 1));
	} else {
		sx = (int)(65536.0 * (float)(src->w) / (float)(dst->w));
		sy = (int)(65536.0 * (float)(src->h) / (float)(dst->h));
	}

	/* Maximum scaled source size */
	ssx = (src->w << 16) - 1;
	ssy = (src->h << 16) - 1;

	/* Precalculate horizontal row increments */
	csx = 0;
	csax = sax;
	for (x = 0; x <= dst->w; x++) {
		*csax = csx;
		csax++;
		csx += sx;

		/* Guard from overflows */
		if (csx > ssx) {
			csx = ssx;
		}
	}

	/* Precalculate vertical row increments */
	csy = 0;
	csay = say;
	for (y = 0; y <= dst->h; y++) {
		*csay = csy;
		csay++;
		csy += sy;

		/* Guard from overflows */
		if (csy > ssy) {
			csy = ssy;
		}
	}

	sp = (ColorRGBA *)src->pixels;
	dp = (ColorRGBA *)dst->pixels;
	dgap = dst->pitch - dst->w * 4;
	spixelgap = src->pitch / 4;

	if (flipx) sp += spixelw;
	if (flipy) sp += (spixelgap * spixelh);

	/*
	 * Switch between interpolating and non-interpolating code
	 */
	if (smooth) {
		/*
		 * Interpolating Zoom
		 */
		csay = say;
		for (y = 0; y < dst->h; y++) {
			csp = sp;
			csax = sax;

			for (x = 0; x < dst->w; x++) {
				/*
				 * Setup color source pointers
				 */
				ex = (*csax & 0xffff);
				ey = (*csay & 0xffff);
				cx = (*csax >> 16);
				cy = (*csay >> 16);
				sstepx = cx < spixelw;
				sstepy = cy < spixelh;
				c00 = sp;
				c01 = sp;
				c10 = sp;
				if (sstepy) {
					if (flipy) {
						c10 -= spixelgap;
					} else {
						c10 += spixelgap;
					}
				}
				c11 = c10;
				if (sstepx) {
					if (flipx) {
						c01--;
						c11--;
					} else {
						c01++;
						c11++;
					}
				}

				/*
				 * Draw and interpolate colors
				 */
				t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
				t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
				dp->r = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
				t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
				dp->g = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
				t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
				dp->b = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
				t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
				dp->a = (((t2 - t1) * ey) >> 16) + t1;
				/*
				 * Advance source pointer x
				 */
				salast = csax;
				csax++;
				sstep = (*csax >> 16) - (*salast >> 16);
				if (flipx) {
					sp -= sstep;
				} else {
					sp += sstep;
				}

				/*
				 * Advance destination pointer x
				 */
				dp++;
			}
			/*
			 * Advance source pointer y
			 */
			salast = csay;
			csay++;
			sstep = (*csay >> 16) - (*salast >> 16);
			sstep *= spixelgap;
			if (flipy) {
				sp = csp - sstep;
			} else {
				sp = csp + sstep;
			}

			/*
			 * Advance destination pointer y
			 */
			dp = (ColorRGBA *)((uint8_t *)dp + dgap);
		}
	} else {
		/*
		 * Non-Interpolating Zoom
		 */
		csay = say;
		for (y = 0; y < dst->h; y++) {
			csp = sp;
			csax = sax;
			for (x = 0; x < dst->w; x++) {
				/*
				 * Draw
				 */
				*dp = *sp;

				/*
				 * Advance source pointer x
				 */
				salast = csax;
				csax++;
				sstep = (*csax >> 16) - (*salast >> 16);
				if (flipx) sstep = -sstep;
				sp += sstep;

				/*
				 * Advance destination pointer x
				 */
				dp++;
			}
			/*
			 * Advance source pointer y
			 */
			salast = csay;
			csay++;
			sstep = (*csay >> 16) - (*salast >> 16);
			sstep *= spixelgap;
			if (flipy) sstep = -sstep;
			sp = csp + sstep;

			/*
			 * Advance destination pointer y
			 */
			dp = (ColorRGBA *)((uint8_t *)dp + dgap);
		}
	}

	tmpBuffersReset();

	return (0);
}

/*

	Bilinear Scaling functions based of the Ne10 project
		https://github.com/projectNe10/Ne10

	Relevant file:
		https://github.com/projectNe10/Ne10/blob/master/modules/imgproc/NE10_resize.c

	Why don't use the **Neon** version?
		- I tried it once and it didn't yield any performance boost, so I stuck with the C version

	What's following is the license of the Ne10 project:
*/

/*
 *  Copyright 2013-16 ARM Limited and Contributors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of ARM Limited nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY ARM LIMITED AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL ARM LIMITED AND CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define INTER_RESIZE_COEF_BITS 11
#define INTER_RESIZE_COEF_SCALE (1 << 11)
#define NE10_MAX_ESIZE 16

static inline uint32_t ne10_align_size(int32_t sz, int32_t n) { return (sz + n - 1) & -n; }

static inline int32_t ne10_floor(float a) { return (((a) >= 0) ? ((int32_t)a) : ((int32_t)a - 1)); }

static inline int32_t ne10_clip(int32_t x, int32_t a, int32_t b) { return (x >= a ? (x < b ? x : b - 1) : a); }

static inline uint8_t ne10_cast_op(int32_t val) {
	int32_t bits = INTER_RESIZE_COEF_BITS * 2;
	int32_t SHIFT = bits;
	int32_t DELTA = 1 << (bits - 1);
	int32_t temp = MIN(255, MAX(0, (val + DELTA) >> SHIFT));
	return (uint8_t)(temp);
};

static void ne10_img_hresize_linear_c(const uint8_t **src, int32_t **dst, int32_t count, const int32_t *xofs, const int16_t *alpha,
				      int32_t swidth, int32_t dwidth, int32_t cn, int32_t xmin, int32_t xmax) {
	int32_t dx, k;

	int32_t dx0 = 0;

	// for (k = 0; k <= count - 2; k++)
	if (count == 2) {
		k = 0;
		const uint8_t *S0 = src[k], *S1 = src[k + 1];
		int32_t *D0 = dst[k], *D1 = dst[k + 1];
		for (dx = dx0; dx < xmax; dx++) {
			int32_t sx = xofs[dx];
			int32_t a0 = alpha[dx * 2], a1 = alpha[dx * 2 + 1];
			int32_t t0 = S0[sx] * a0 + S0[sx + cn] * a1;
			int32_t t1 = S1[sx] * a0 + S1[sx + cn] * a1;
			D0[dx] = t0;
			D1[dx] = t1;
		}

		for (; dx < dwidth; dx++) {
			int32_t sx = xofs[dx];
			D0[dx] = (int32_t)S0[sx] * INTER_RESIZE_COEF_SCALE;
			D1[dx] = (int32_t)S1[sx] * INTER_RESIZE_COEF_SCALE;
		}
	}

	// for (; k < count; k++)
	if (count == 1) {
		k = 0;
		const uint8_t *S = src[k];
		int32_t *D = dst[k];
		for (dx = 0; dx < xmax; dx++) {
			int32_t sx = xofs[dx];
			D[dx] = S[sx] * alpha[dx * 2] + S[sx + cn] * alpha[dx * 2 + 1];
		}

		for (; dx < dwidth; dx++) D[dx] = (int32_t)S[xofs[dx]] * INTER_RESIZE_COEF_SCALE;
	}
}

static void ne10_img_vresize_linear_c(const int32_t **src, uint8_t *dst, const int16_t *beta, int32_t width) {
	int32_t b0 = beta[0], b1 = beta[1];
	const int32_t *S0 = src[0], *S1 = src[1];

	int32_t x = 0;
	for (; x <= width - 4; x += 4) {
		int32_t t0, t1;
		t0 = S0[x] * b0 + S1[x] * b1;
		t1 = S0[x + 1] * b0 + S1[x + 1] * b1;
		dst[x] = ne10_cast_op(t0);
		dst[x + 1] = ne10_cast_op(t1);
		t0 = S0[x + 2] * b0 + S1[x + 2] * b1;
		t1 = S0[x + 3] * b0 + S1[x + 3] * b1;
		dst[x + 2] = ne10_cast_op(t0);
		dst[x + 3] = ne10_cast_op(t1);
	}

	for (; x < width; x++) dst[x] = ne10_cast_op(S0[x] * b0 + S1[x] * b1);
}

static void ne10_img_resize_generic_linear_c(uint8_t *src, uint8_t *dst, const int32_t *xofs, const int16_t *_alpha, const int32_t *yofs,
					     const int16_t *_beta, int32_t xmin, int32_t xmax, int32_t ksize, int32_t srcw, int32_t srch,
					     int32_t srcstep, int32_t dstw, int32_t dsth, int32_t channels) {
	const int16_t *alpha = _alpha;
	const int16_t *beta = _beta;
	int32_t cn = channels;
	srcw *= cn;
	dstw *= cn;

	int32_t bufstep = (int32_t)ne10_align_size(dstw, 16);
	int32_t dststep = (int32_t)ne10_align_size(dstw, 4);

	int32_t *buffer_ = (int32_t *)tmpBufferAlloc(bufstep * ksize * sizeof(int32_t));

	const uint8_t *srows[NE10_MAX_ESIZE];
	int32_t *rows[NE10_MAX_ESIZE];
	int32_t prev_sy[NE10_MAX_ESIZE];
	int32_t k, dy;
	xmin *= cn;
	xmax *= cn;

	for (k = 0; k < ksize; k++) {
		prev_sy[k] = -1;
		rows[k] = (int32_t *)buffer_ + bufstep * k;
	}

	// image resize is a separable operation. In case of not too strong
	for (dy = 0; dy < dsth; dy++, beta += ksize) {
		int32_t sy0 = yofs[dy], k, k0 = ksize, k1 = 0, ksize2 = ksize / 2;

		for (k = 0; k < ksize; k++) {
			int32_t sy = ne10_clip(sy0 - ksize2 + 1 + k, 0, srch);
			for (k1 = MAX(k1, k); k1 < ksize; k1++) {
				if (sy == prev_sy[k1])  // if the sy-th row has been computed already, reuse it.
				{
					if (k1 > k) memcpy(rows[k], rows[k1], bufstep * sizeof(rows[0][0]));
					break;
				}
			}
			if (k1 == ksize) k0 = MIN(k0, k);  // remember the first row that needs to be computed
			srows[k] = (const uint8_t *)(src + srcstep * sy);
			prev_sy[k] = sy;
		}

		if (k0 < ksize) ne10_img_hresize_linear_c(srows + k0, rows + k0, ksize - k0, xofs, alpha, srcw, dstw, cn, xmin, xmax);

		ne10_img_vresize_linear_c((const int32_t **)rows, (uint8_t *)(dst + dststep * dy), beta, dstw);
	}
}

static void ne10_img_resize_cal_offset_linear(int32_t *xofs, int16_t *ialpha, int32_t *yofs, int16_t *ibeta, int32_t *xmin, int32_t *xmax,
					      int32_t ksize, int32_t ksize2, int32_t srcw, int32_t srch, int32_t dstw, int32_t dsth,
					      int32_t channels) {
	float inv_scale_x = (float)dstw / srcw;
	float inv_scale_y = (float)dsth / srch;

	int32_t cn = channels;
	float scale_x = 1. / inv_scale_x;
	float scale_y = 1. / inv_scale_y;
	int32_t k, sx, sy, dx, dy;

	float fx, fy;

	float cbuf[NE10_MAX_ESIZE];

	for (dx = 0; dx < dstw; dx++) {
		fx = (float)((dx + 0.5) * scale_x - 0.5);
		sx = ne10_floor(fx);
		fx -= sx;

		if (sx < ksize2 - 1) {
			*xmin = dx + 1;
			if (sx < 0) fx = 0, sx = 0;
		}

		if (sx + ksize2 >= srcw) {
			*xmax = MIN(*xmax, dx);
			if (sx >= srcw - 1) fx = 0, sx = srcw - 1;
		}

		for (k = 0, sx *= cn; k < cn; k++) xofs[dx * cn + k] = sx + k;

		cbuf[0] = 1.f - fx;
		cbuf[1] = fx;

		for (k = 0; k < ksize; k++) ialpha[dx * cn * ksize + k] = (int16_t)(cbuf[k] * INTER_RESIZE_COEF_SCALE);
		for (; k < cn * ksize; k++) ialpha[dx * cn * ksize + k] = ialpha[dx * cn * ksize + k - ksize];
	}

	for (dy = 0; dy < dsth; dy++) {
		fy = (float)((dy + 0.5) * scale_y - 0.5);
		sy = ne10_floor(fy);
		fy -= sy;

		yofs[dy] = sy;

		cbuf[0] = 1.f - fy;
		cbuf[1] = fy;

		for (k = 0; k < ksize; k++) ibeta[dy * ksize + k] = (int16_t)(cbuf[k] * INTER_RESIZE_COEF_SCALE);
	}
}

/**
 * @brief image resize of 8-bit data.
 * @param[out]  *dst                  point to the destination image
 * @param[in]   dst_width             width of destination image
 * @param[in]   dst_height            height of destination image
 * @param[in]   *src                  point to the source image
 * @param[in]   src_width             width of source image
 * @param[in]   src_height            height of source image
 * @param[in]   src_stride            stride of source buffer
 * @return none.
 * The function implements image resize
 */
void zoomResizeBilinear_RGB8888(uint8_t *dst, uint32_t dst_width, uint32_t dst_height, uint8_t *src, uint32_t src_width,
				uint32_t src_height, uint32_t src_stride) {
	int32_t dstw = dst_width;
	int32_t dsth = dst_height;
	int32_t srcw = src_width;
	int32_t srch = src_height;

	int32_t cn = 4;

	int32_t xmin = 0;
	int32_t xmax = dstw;
	int32_t width = dstw * cn;
	float fx, fy;

	int32_t ksize = 0, ksize2;
	ksize = 2;
	ksize2 = ksize / 2;

	uint8_t *buffer_ = (uint8_t *)tmpBufferAlloc((width + dsth) * (sizeof(int32_t) + sizeof(float) * ksize));

	int32_t *xofs = (int32_t *)buffer_;
	int32_t *yofs = xofs + width;
	int16_t *ialpha = (int16_t *)(yofs + dsth);
	int16_t *ibeta = ialpha + width * ksize;

	ne10_img_resize_cal_offset_linear(xofs, ialpha, yofs, ibeta, &xmin, &xmax, ksize, ksize2, srcw, srch, dstw, dsth, cn);

	ne10_img_resize_generic_linear_c(src, dst, xofs, ialpha, yofs, ibeta, xmin, xmax, ksize, srcw, srch, src_stride, dstw, dsth, cn);

	tmpBuffersReset();
}
