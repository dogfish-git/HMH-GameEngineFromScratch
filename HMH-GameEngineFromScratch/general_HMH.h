#pragma once

struct general_sinWaveVariablesStruct
{
	int SamplesPerSecond;
	int Hz;
	int counter;
};

struct general_bitmapBufferStruct
{
	void *bitMemory;
	int width;
	int height;
	int bytesPerPixel;
	int pitch;
};

static void general_screen_render(general_bitmapBufferStruct *bitmapBuffer, int redOffset, int greenOffset);
static void general_sound_output(general_sinWaveVariablesStruct *sinStruct);

static void general_game_update(general_bitmapBufferStruct *bitmapBuffer, int redOffset, int greenOffset);