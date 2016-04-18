
#include <stdint.h>

#include "general_HMH.h"
#include "general_HMH.cpp"

#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>



static bool stillRunning = false;
static bool soundIsPlaying = false;

#define pi 3.14159265359f

//control/keyboard functions expanded below WinMain for spacing convenience
void win32_getXInputLib();
void win32_xInputControls();
LRESULT win32_keyboardInput_function(HWND, UINT, WPARAM, LPARAM);

// xinput stub functions. Keeps program from crashing when no controller is plugged in
#define X_INPUT_GET_STATE(name) DWORD WINAPI name (DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name (DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// directsound declaration. Ff no directsound library is found, program continues
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

struct sinWaveVariablesStruct
{
	int SamplesPerSecond = 48000;
	int BitsPerSample = 16;
	int BytesPerSample = BitsPerSample / 8;
	int BufferSize = SamplesPerSecond * BytesPerSample * 2;
	int Hzcounter = 0;
	int Hz = 256;
	int counter = 0;
	int period = SamplesPerSecond / Hz;
	int volume = 3000;
	float tSinLoc = 0;
	int latencySampleCount = SamplesPerSecond / 30;
};

static LPDIRECTSOUNDBUFFER win32_initDSound(HWND winHandle, int SamplesPerSecond, int BitsPerSample, int BufferSize)
{
	// Load dsound.dll 
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (DSoundLibrary)
	{
		// Get DirectSound object and set cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		WAVEFORMATEX WaveFormat = {};
		WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
		WaveFormat.nChannels = 2;
		WaveFormat.nSamplesPerSec = SamplesPerSecond;
		WaveFormat.wBitsPerSample = BitsPerSample;
		WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
		WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
		WaveFormat.cbSize = 0;

		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			// Create primary buffer
			if (SUCCEEDED(DirectSound->SetCooperativeLevel(winHandle, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER PrimaryBuffer;

				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugStringA("Primary buffer format success\n");
					}
					else
					{
						// Diagnostic
						OutputDebugStringA("Primary buffer format Fail\n");
					}

				}
				else
				{
					// Diagnostic
					OutputDebugStringA("DirectSound CreateSoundBuffer (Primary buffer) failed\n");
				}
			}
			else
			{
				// Diagnostic
				OutputDebugStringA("DirectSound SetCooperativeLevel failed \n");

			}
			// Create secondary buffer
			DSBUFFERDESC SecondaryBufferDescription = {};
			SecondaryBufferDescription.dwSize = sizeof(SecondaryBufferDescription);
			SecondaryBufferDescription.dwFlags = 0;
			SecondaryBufferDescription.dwBufferBytes = BufferSize;
			SecondaryBufferDescription.dwReserved;
			SecondaryBufferDescription.lpwfxFormat = &WaveFormat;
			SecondaryBufferDescription.guid3DAlgorithm;

			LPDIRECTSOUNDBUFFER SecondaryBuffer;
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&SecondaryBufferDescription, &SecondaryBuffer, 0)))
			{
				OutputDebugStringA("Secondary buffer creation success \n");
				return SecondaryBuffer;
			}
			else
			{
				// Diagnostic
				OutputDebugStringA("Secondary buffer creation failed \n");
			}

		}
		else
		{
			// Diagnostic
			OutputDebugStringA("DirectSoundCreate failed\n");
		}

	}
	else
	{
		// Diagnostic
		OutputDebugStringA("DSoundLibrary failed\n");
	}
}

static void win32_playSoundSinWave(LPDIRECTSOUNDBUFFER soundBuffer, sinWaveVariablesStruct *sinStruct)
{
	DWORD PlayCursor = 0;
	DWORD WriteCursor = 0;
	DWORD ByteToLock = (sinStruct->counter * sinStruct->BytesPerSample * 2) % sinStruct->BufferSize;
	DWORD BytesToWrite = 0;

	if (SUCCEEDED(soundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
	{
		DWORD targetCursor = (PlayCursor + (sinStruct->latencySampleCount * sinStruct->BytesPerSample * 2)) % sinStruct->BufferSize;
		if (ByteToLock == PlayCursor)
		{
			if (!soundIsPlaying)
			{
				BytesToWrite = sinStruct->BufferSize;
				soundIsPlaying = true;
			}
		}
		else if (ByteToLock > targetCursor)
		{
			BytesToWrite = sinStruct->BufferSize - ByteToLock;
			BytesToWrite += targetCursor;
		}
		else
		{
			BytesToWrite = targetCursor - ByteToLock;
		}

		void *Region1;
		DWORD Region1Size = 0;
		void *Region2;
		DWORD Region2Size = 0;

		if (SUCCEEDED(soundBuffer->Lock(
			ByteToLock,
			BytesToWrite,
			&Region1,
			&Region1Size,
			&Region2,
			&Region2Size,
			0
			)))
		{

			// Region 1 for circular buffer
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

			// Region 2 for circular buffer
			SampleOut = (int16_t *)Region2;
			DWORD Region2SampleSize = Region2Size / (sinStruct->BytesPerSample * 2);
			for (DWORD sampleIndex = 0; sampleIndex < Region2SampleSize; ++sampleIndex)
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
			soundBuffer->Unlock(Region1,
				Region1Size,
				Region2,
				Region2Size);
		}
		else
		{
			//OutputDebugStringA ("Sound buffer lock failed\n ");
		}
	}
	else
	{
		OutputDebugStringA("getCurrentPosition in sound buffer failed\n ");
	}
}

static struct win32_BitmapBufferStruct
{
	BITMAPINFO info;
	void *bitMemory;
	int width;
	int height;
	int bytesPerPixel;
	int pitch;
};

static win32_BitmapBufferStruct win32_globalBitmapBuffer;

void win32_WM_SIZE_funtion(HWND window, win32_BitmapBufferStruct *bitmapBuffer)
{
	if (bitmapBuffer->bitMemory)
	{
		VirtualFree(bitmapBuffer->bitMemory, 0, MEM_RELEASE);
	}

	RECT clientRect;
	GetClientRect(window, &clientRect);

	bitmapBuffer->bytesPerPixel = 4;
	bitmapBuffer->width = clientRect.right - clientRect.left;
	bitmapBuffer->height = clientRect.bottom - clientRect.top;
	bitmapBuffer->info.bmiHeader.biSize = sizeof(bitmapBuffer->info.bmiHeader);
	bitmapBuffer->info.bmiHeader.biWidth = bitmapBuffer->width;
	bitmapBuffer->info.bmiHeader.biHeight = -bitmapBuffer->height;
	bitmapBuffer->info.bmiHeader.biPlanes = 1;
	bitmapBuffer->info.bmiHeader.biBitCount = 32;
	bitmapBuffer->info.bmiHeader.biCompression = BI_RGB;

	int pixelMemoryAlloc = bitmapBuffer->width * bitmapBuffer->height * bitmapBuffer->bytesPerPixel;

	bitmapBuffer->bitMemory = VirtualAlloc(0,
		pixelMemoryAlloc,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
		);
}

void win32_WM_PAINT_funtion (HWND window, win32_BitmapBufferStruct bitmapBuffer)
{
	PAINTSTRUCT paint;
	HDC deviceContext = BeginPaint(window, &paint);

	RECT clientRect;
	GetClientRect(window, &clientRect);

	int winWidth = clientRect.right - clientRect.left;
	int winHeight = clientRect.bottom - clientRect.top;

	StretchDIBits(deviceContext,
		0,
		0,
		bitmapBuffer.width,
		bitmapBuffer.height,
		0,
		0,
		winWidth,
		winHeight,
		bitmapBuffer.bitMemory,
		&bitmapBuffer.info,
		DIB_RGB_COLORS,
		SRCCOPY
		);

	EndPaint(window, &paint);
}

void gradient(HWND window, win32_BitmapBufferStruct bitmapBuffer, general_bitmapBufferStruct *general_bitmapBuffer)
{
	HDC deviceContext = GetDC(window);

	RECT clientRect;
	GetClientRect(window, &clientRect);

	int winWidth = clientRect.right - clientRect.left;
	int winHeight = clientRect.bottom - clientRect.top;

	StretchDIBits(deviceContext,
		0,
		0,
		general_bitmapBuffer->width,
		general_bitmapBuffer->height,
		0,
		0,
		winWidth,
		winHeight,
		general_bitmapBuffer->bitMemory,
		&bitmapBuffer.info,
		DIB_RGB_COLORS,
		SRCCOPY
		);

	ReleaseDC(window, deviceContext);
}


LRESULT CALLBACK windowsProc(HWND windowHandle,
	UINT message,
	WPARAM  wParam,
	LPARAM  lParam
	)
{
	LRESULT result = 0;
	switch (message)
	{
	case WM_SIZE:
	{
		win32_WM_SIZE_funtion(windowHandle, &win32_globalBitmapBuffer);
	}break;

	case WM_DESTROY:
	{
		stillRunning = false;
	}break;

	case WM_CLOSE:
	{
		stillRunning = false;
	}break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		result = win32_keyboardInput_function(windowHandle,
			message,
			wParam,
			lParam);
	}break;

	case WM_ACTIVATEAPP:
	{

	}break;

	case WM_PAINT:
	{
		win32_WM_PAINT_funtion(windowHandle, win32_globalBitmapBuffer);
	}break;

	default:
	{
		result = DefWindowProcA(windowHandle,
			message,
			wParam,
			lParam
			);
	}break;
	}
	return result;
}

int CALLBACK WinMain(HINSTANCE handle,
	HINSTANCE prevHandle,
	LPSTR commandLine,
	int howWinShown
	)
{
	WNDCLASSEXA windClass = {};
	windClass.cbSize = sizeof(WNDCLASSEXA);
	windClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	windClass.lpfnWndProc = windowsProc;
	windClass.cbClsExtra = 0;
	windClass.cbWndExtra = 0;
	windClass.hInstance = handle;
	windClass.hIcon = NULL;
	windClass.hCursor = NULL;
	windClass.hbrBackground = NULL;
	windClass.lpszMenuName = NULL;
	windClass.lpszClassName = "GameEngineFromScratch";
	windClass.hIconSm = NULL;


	if (RegisterClassExA(&windClass))
	{
		HWND winHandle = CreateWindowExA(0,
			windClass.lpszClassName,
			"GameEngineFromScratch",
			WS_VISIBLE | WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			handle,
			0
			);

		if (winHandle)
		{
			sinWaveVariablesStruct sinStruct;
			LPDIRECTSOUNDBUFFER soundBuffer = win32_initDSound(winHandle, sinStruct.SamplesPerSecond, sinStruct.BitsPerSample, sinStruct.BufferSize);
			win32_playSoundSinWave(soundBuffer, &sinStruct);
			soundBuffer->Play(0, 0, DSBPLAY_LOOPING);

			LARGE_INTEGER perfFreq;
			QueryPerformanceFrequency(&perfFreq);
			int perfCountFreq = perfFreq.QuadPart;
			LARGE_INTEGER lastCounter;
			QueryPerformanceCounter(&lastCounter);
			uint64_t lastCycleCount = __rdtsc();

			int redOffset = 0;
			int greenOffset = 0;

			stillRunning = true;
			win32_getXInputLib();

			while (stillRunning)
			{
				MSG message;
				while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) // "Peek", as "get" uses a wait
				{
					if (message.message == WM_QUIT)
					{
						stillRunning = false;
					}
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}
				general_bitmapBufferStruct generalBuffer = {};

				generalBuffer.bitMemory = win32_globalBitmapBuffer.bitMemory;
				generalBuffer.width = win32_globalBitmapBuffer.width;
				generalBuffer.height = win32_globalBitmapBuffer.height;
				generalBuffer.bytesPerPixel = win32_globalBitmapBuffer.bytesPerPixel;
				generalBuffer.pitch = win32_globalBitmapBuffer.pitch;

				general_game_update(&generalBuffer, redOffset, greenOffset);


				win32_playSoundSinWave(soundBuffer, &sinStruct);
				gradient(winHandle, win32_globalBitmapBuffer, &generalBuffer);
				redOffset++;
				greenOffset++;
				win32_xInputControls();


				// query end of loop for performance
				uint64_t endCycleCount = __rdtsc();
				LARGE_INTEGER endCounter;
				QueryPerformanceCounter(&endCounter);
				int64_t cyclesElapsed = endCycleCount - lastCycleCount;
				int64_t counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
				int32_t microSecsPerFrame = (int32_t)((1000 * counterElapsed) / perfCountFreq);
				int32_t framesPerSec = (int32_t)(perfCountFreq / counterElapsed);
				int32_t megaCyclesPerFrame = (int32_t)(cyclesElapsed / (1000 * 1000));
				char bufferForFPS[512];
				// To be deleted, bad function
				wsprintfA(bufferForFPS, "Ms/frame: %d - FPS: %d - Megacycles/frame: %d\n", microSecsPerFrame, framesPerSec, megaCyclesPerFrame);
				OutputDebugStringA(bufferForFPS);
				lastCycleCount = endCycleCount;
				lastCounter = endCounter;
			}
		}
	}
	return 0;
}

LRESULT win32_keyboardInput_function(
	HWND windowHandle,
	UINT message,
	WPARAM wParam, 
	LPARAM  lParam)
{
	bool keyIsDown = ((lParam & (1 << 31)) == 0);
	bool keyWasDown = ((lParam & (1 << 30)) != 0);


	if (keyIsDown != keyWasDown)
	{
		switch (wParam)
		{
			/********************* Outside Keys (sans alt) *********************/
		case 0x0d: //Enter Key
		{
			OutputDebugStringA("Enter Key\n");
		}break;

		case 0x20: //Spacebar
		{
			OutputDebugStringA("Spacebar\n");
		}break;

		/********************* Left/Up/Right/Down Arrow Keys *********************/
		case 0x25: //Left Arrow
		{
			OutputDebugStringA("Left Arrow\n");
		}break;

		case 0x26: // Up Arrow
		{
			OutputDebugStringA("Up Arrow\n");
		}break;

		case 0x27: //Right Arrow
		{
			OutputDebugStringA("Right Arrow\n");
		}break;

		case 0x28: //Down Arrow
		{
			OutputDebugStringA("Down Arrow\n");
		}break;

		/********************* 0 - 9 Keys *********************/
		case 0x30: //0
		{
			OutputDebugStringA("0\n");
		}break;

		case 0x31: //1
		{
			OutputDebugStringA("1\n");
		}break;

		case 0x32: //2
		{
			OutputDebugStringA("2\n");
		}break;

		case 0x33: //3
		{
			OutputDebugStringA("3\n");
		}break;

		case 0x34: //4
		{
			OutputDebugStringA("4\n");
		}break;

		case 0x35: //5
		{
			OutputDebugStringA("5\n");
		}break;

		case 0x36: //6
		{
			OutputDebugStringA("6\n");
		}break;

		case 0x37: //7
		{
			OutputDebugStringA("7\n");
		}break;

		case 0x38: //8
		{
			OutputDebugStringA("8\n");
		}break;

		case 0x39: //9
		{
			OutputDebugStringA("9\n");
		}break;

		/********************* A - Z Keys *********************/
		case 0x41: //A
		{
			OutputDebugStringA("A\n");
		}break;

		case 0x42: //B
		{
			OutputDebugStringA("B\n");
		}break;

		case 0x43: //C
		{
			OutputDebugStringA("C\n");
		}break;

		case 0x44: //D
		{
			OutputDebugStringA("D\n");
		}break;

		case 0x45: //E
		{
			OutputDebugStringA("E\n");
		}break;

		case 0x46: //F
		{
			OutputDebugStringA("F\n");
		}break;

		case 0x47: //G
		{
			OutputDebugStringA("G\n");
		}break;

		case 0x48: //H
		{
			OutputDebugStringA("H\n");
		}break;

		case 0x49: //I
		{
			OutputDebugStringA("I\n");
		}break;

		case 0x4A: //J
		{
			OutputDebugStringA("J\n");
		}break;

		case 0x4B: //K
		{
			OutputDebugStringA("K\n");
		}break;

		case 0x4C: //L
		{
			OutputDebugStringA("L\n");
		}break;

		case 0x4D: //M
		{
			OutputDebugStringA("M\n");
		}break;

		case 0x4E: //N
		{
			OutputDebugStringA("N\n");
		}break;

		case 0x4F: //O
		{
			OutputDebugStringA("O\n");
		}break;

		case 0x50: //P
		{
			OutputDebugStringA("P\n");
		}break;

		case 0x51: //Q
		{
			OutputDebugStringA("Q\n");
		}break;

		case 0x52: //R
		{
			OutputDebugStringA("R\n");
		}break;

		case 0x53: //S
		{
			OutputDebugStringA("S\n");
		}break;

		case 0x54: //T
		{
			OutputDebugStringA("T\n");
		}break;

		case 0x55: //U
		{
			OutputDebugStringA("U\n");
		}break;

		case 0x56: //V
		{
			OutputDebugStringA("V\n");
		}break;

		case 0x57: //W
		{
			OutputDebugStringA("W\n");
		}break;

		case 0x58: //X
		{
			OutputDebugStringA("X\n");
		}break;

		case 0x59: //Y
		{
			OutputDebugStringA("Y\n");
		}break;

		case 0x5A: //Z
		{
			OutputDebugStringA("Z\n");
		}break;

		default:
		{
			LRESULT result = DefWindowProcA(windowHandle,
				message,
				wParam,
				lParam
				);
			return result;
		}break;
		}
	}
}

void win32_xInputControls()
{
	for (DWORD indexOfControllers = 0; indexOfControllers < XUSER_MAX_COUNT; indexOfControllers++)
	{
		XINPUT_STATE controllerState;
		if (XInputGetState(indexOfControllers, &controllerState) == ERROR_SUCCESS)
			// Controller is connected
		{
			bool buttonUp = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
			bool buttonDown = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
			bool buttonLeft = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
			bool buttonRight = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);

			bool buttonA = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0);
			bool buttonB = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0);
			bool buttonX = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0);
			bool buttonY = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0);

			bool buttonStart = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0);
			bool buttonBack = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0);

			bool buttonLeftThumb = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
			bool buttonRightThumb = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);
			bool buttonLeftShoulder = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0);
			bool buttonRightShoulder = ((controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);

			uint8_t triggerLeft = controllerState.Gamepad.bLeftTrigger;
			uint8_t triggerRight = controllerState.Gamepad.bRightTrigger;

			int16_t thumbLeftX = controllerState.Gamepad.sThumbLX;
			int16_t thumbLeftY = controllerState.Gamepad.sThumbLY;
			int16_t thumbRightX = controllerState.Gamepad.sThumbRX;
			int16_t thumbRightY = controllerState.Gamepad.sThumbRY;

			if (buttonA)
				OutputDebugStringA("button A\n");

		}
		else
			// Controller is not connected	
		{


		}
	}
}

void win32_getXInputLib()
{
	HMODULE xInputLib = LoadLibraryA("xinput1_4.dll");
	if (!xInputLib)
	{
		HMODULE xInputLib = LoadLibraryA("xinput9_1_0.dll");
		if (!xInputLib)
		{
			HMODULE xInputLib = LoadLibraryA("xinput1_3.dll");
		}
		else
		{
			OutputDebugStringA("xinput library did not load\n");
		}
	}

	if (xInputLib)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(xInputLib, "XInputGetState");
		if (!XInputGetState)
		{
			XInputGetState = XInputGetStateStub;
		}
		XInputSetState = (x_input_set_state *)GetProcAddress(xInputLib, "XInputSetState");
		if (!XInputSetState)
		{
			XInputSetState = XInputSetStateStub;
		}
	}
	else
	{
		OutputDebugStringA("xinput1_3.dll or xinput1_4.dll not found\n");
	}
}