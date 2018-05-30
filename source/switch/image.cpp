#include <string.h>

#include "image.h"

#include "stb_image.h"

void imageLoad(Image* image, const char* filename) {
	int channelsInFile = 4;
	image->data = stbi_load(filename, &image->width, &image->height, &channelsInFile, 4);
}

void imageDeinit(Image* image) { stbi_image_free(image->data); }

void imageDraw(u8* fb, u16 fbWidth, u16 fbHeight, Image* image, int x, int y, int u, int v, int imageWidth, int imageHeight) {
	if (fb == NULL) {
		return;
	}

	if (imageWidth == 0) imageWidth = image->width;
	if (imageHeight == 0) imageHeight = image->height;

	if (x + imageWidth < 0 || y + imageHeight < 0 || x >= fbWidth || y >= fbHeight) {
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

	if (x + imageWidth >= fbWidth) {
		imageWidth = fbWidth - x;
	}

	if (y + imageHeight >= fbHeight) {
		imageHeight = fbHeight - y;
	}

	u8* fbAddr = fb + (x + y * fbWidth) * 4;
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
		fbAddr = fbLineStart + fbWidth * 4;
	}
}
