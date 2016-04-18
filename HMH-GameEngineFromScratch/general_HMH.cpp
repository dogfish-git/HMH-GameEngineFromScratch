#include "general_HMH.h"
#include <stdint.h>

#define pi 3.14159265359f

static void general_sound_output(general_sinWaveVariablesStruct *sinStruct)
{
	int16_t *SampleOut = (int16_t *)Region1;
	DWORD Region1SampleSize = Region1Size / (sinStruct->BytesPerSample * 2);
	for (DWORD sampleIndex = 0; sampleIndex < Region1SampleSize; ++sampleIndex)
	{
		sinStruct->tSinLoc = (2.0f * pi * sinStruct->counter) / (float)sinStruct->period;
		float sinWaveValue = sinf(sinStruct->tSinLoc);
		int16_t SampleValue = (int16_t)(sinStruct->volume * sinWaveValue);
		sinStruct->counter++;
		// Left
		*SampleOut++ = SampleValue;
		// Right
		*SampleOut++ = SampleValue;
	}
}

static void general_screen_render(general_bitmapBufferStruct *bitmapBuffer, int redOffset, int greenOffset)
{
	int pitch = bitmapBuffer->width * bitmapBuffer->bytesPerPixel;
	uint8_t *row = (uint8_t *)bitmapBuffer->bitMemory;

	for (int y = 0; y < bitmapBuffer->height; y++)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int x = 0; x < bitmapBuffer->width; x++)
		{
			uint8_t red = x + redOffset;
			uint8_t green = y + greenOffset;
			uint8_t blue = x + y;

			*pixel = (red << 16 | green << 8 | blue);
			pixel++;
		}
		row += pitch;
	}
}

static void general_game_update(general_bitmapBufferStruct *bitmapBuffer, int redOffset, int greenOffset)
{
	general_sound_output(soundBuffer);
	general_screen_render(bitmapBuffer, redOffset, greenOffset);
}