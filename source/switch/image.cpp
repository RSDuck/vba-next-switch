#include <string.h>

#include "image.h"

#include "stb_image.h"

void imageLoad(Image* image, const char* filename) {
	int channelsInFile = 4;
	image->data = stbi_load(filename, &image->width, &image->height, &channelsInFile, 4);
}

void imageDeinit(Image* image) { stbi_image_free(image->data); }

#define CLAMP(v, s, t) ((((v) > (t) ? (t) : (v))) < (s) ? (s) : (v))

void imageDraw(Image* image, int x, int y, int u, int v, int imageWidth, int imageHeight) {
	extern u8* currentFB;
	extern u32 currentFBWidth;
	extern u32 currentFBHeight;

	if (imageWidth == 0) imageWidth = image->width;
	if (imageHeight == 0) imageHeight = image->height;

	u = CLAMP(u, 0, image->width);
	v = CLAMP(v, 0, image->height);
	imageWidth = CLAMP(imageWidth, 0, image->width - u);
	imageHeight = CLAMP(imageHeight, 0, image->height - v);

	if (x + imageWidth < 0 || y + imageHeight < 0 || x >= currentFBWidth || y >= currentFBHeight) {
		return;
	}

	if (x < 0) {
		imageWidth += x;
		u -= x;
	}

	if (y < 0) {
		imageHeight += y;
		v -= x;
	}

	if (x + imageWidth >= currentFBWidth) {
		imageWidth = currentFBWidth - x;
	}

	if (y + imageHeight >= currentFBHeight) {
		imageHeight = currentFBHeight - y;
	}

	u8* fbAddr = currentFB + (x + y * currentFBWidth) * 4;
	u8* imageAddr = image->data + (u + v * image->width) * 4;
	for (int j = 0; j < imageHeight; j++) {
		u8* fbLineStart = fbAddr;

		for (int i = 0; i < imageWidth; i++) {
			u8 alpha = imageAddr[3];
			u8 oneMinusAlpha = 255 - alpha;

			fbAddr[0] = (imageAddr[0] * alpha) / 255 + (fbAddr[0] * oneMinusAlpha) / 255;
			fbAddr[1] = (imageAddr[1] * alpha) / 255 + (fbAddr[1] * oneMinusAlpha) / 255;
			fbAddr[2] = (imageAddr[2] * alpha) / 255 + (fbAddr[2] * oneMinusAlpha) / 255;
			fbAddr[3] = 0xff;

			imageAddr += 4;
			fbAddr += 4;
		}
		imageAddr += u * 4;
		fbAddr = fbLineStart + currentFBWidth * 4;
	}
}
