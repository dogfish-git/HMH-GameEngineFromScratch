#include <Windows.h>
#include <stdint.h>

static bool stillRunning = false;


struct bitStruct
{
	 BITMAPINFO info;
	 void *bitMemory;
	 int width;
	 int height;
	 int bytesPerPixel = 4;
	 int pitch;
};

static bitStruct globalBitStruct;

void funcForWM_SIZE (HWND window)
{
	if (globalBitStruct.bitMemory)
	{
		VirtualFree (globalBitStruct.bitMemory, 0, MEM_RELEASE);
	}

	RECT clientRect;
	GetClientRect (window, &clientRect);

	globalBitStruct.width = clientRect.right - clientRect.left;
	globalBitStruct.height = clientRect.bottom - clientRect.top;
	globalBitStruct.info.bmiHeader.biSize = sizeof (globalBitStruct.info.bmiHeader);
	globalBitStruct.info.bmiHeader.biWidth = globalBitStruct.width;
	globalBitStruct.info.bmiHeader.biHeight = -globalBitStruct.height;
	globalBitStruct.info.bmiHeader.biPlanes = 1;
	globalBitStruct.info.bmiHeader.biBitCount = 32;
	globalBitStruct.info.bmiHeader.biCompression = BI_RGB;

	int pixelMemoryAlloc = globalBitStruct.width * globalBitStruct.height * globalBitStruct.bytesPerPixel;

	globalBitStruct.bitMemory = VirtualAlloc (0,
							  pixelMemoryAlloc,
							  MEM_COMMIT,
							  PAGE_READWRITE
							  );
}

void funcForWM_PAINT (HWND window)
{
	PAINTSTRUCT paint;
	HDC deviceContext = BeginPaint (window, &paint);

	RECT clientRect;
	GetClientRect (window, &clientRect);

	int winWidth = clientRect.right - clientRect.left;
	int winHeight = clientRect.bottom - clientRect.top;

	StretchDIBits (deviceContext,
				   0,
				   0,
				   globalBitStruct.width,
				   globalBitStruct.height,
				   0,
				   0,
				   winWidth,
				   winHeight,
				   globalBitStruct.bitMemory,
				   &globalBitStruct.info,
				   DIB_RGB_COLORS,
				   SRCCOPY
				   );

	EndPaint (window, &paint);
}

void gradient (HWND window, int xOffset, int yOffset)
{
	HDC deviceContext = GetDC (window);

	RECT clientRect;
	GetClientRect (window, &clientRect);

	int winWidth = clientRect.right - clientRect.left;
	int winHeight = clientRect.bottom - clientRect.top;

	int pitch = globalBitStruct.width * globalBitStruct.bytesPerPixel;
	uint8_t *row = (uint8_t *) globalBitStruct.bitMemory;

	for (int y = 0; y < globalBitStruct.height; y++)
	{
		uint32_t *pixel = (uint32_t *) row;
		for (int x = 0; x < globalBitStruct.width; x++)
		{
			uint8_t red = x + xOffset;
			uint8_t green = y + yOffset;
			uint8_t blue = x+y;

			*pixel = (red << 16 | green << 8 | blue);
			pixel++;
		}
		row += pitch;
	}

	StretchDIBits (deviceContext,
				   0,
				   0,
				   globalBitStruct.width,
				   globalBitStruct.height,
				   0,
				   0,
				   winWidth,
				   winHeight,
				   globalBitStruct.bitMemory,
				   &globalBitStruct.info,
				   DIB_RGB_COLORS,
				   SRCCOPY
				   );

	ReleaseDC (window, deviceContext);
}



LRESULT CALLBACK windowsProc (HWND windowHandle,
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
			funcForWM_SIZE (windowHandle);
		}break;

		case WM_DESTROY:
		{
			stillRunning = false;
		}break;

		case WM_CLOSE:
		{
			stillRunning = false;
		}break;

		case WM_ACTIVATEAPP:
		{

		}break;
		
		case WM_PAINT:
		{
			funcForWM_PAINT (windowHandle);
		}break;
		
		default:
		{
			result = DefWindowProcA (windowHandle,
									 message,
									 wParam,
									 lParam
									 );
		}break;
	}
	return result;
}


int CALLBACK WinMain (HINSTANCE handle, 
					  HINSTANCE prevHandle, 
					  LPSTR commandLine, 
					  int howWinShown
					  )
{
	WNDCLASSEXA windClass = {};
	windClass.cbSize = sizeof (WNDCLASSEXA);
	windClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	windClass.lpfnWndProc = windowsProc;
	windClass.cbClsExtra = 0;
	windClass.cbWndExtra = 0;
	windClass.hInstance = handle;
	windClass.hIcon = NULL;
	windClass.hCursor = NULL;
	windClass.hbrBackground = NULL;
	windClass.lpszMenuName = NULL;
	windClass.lpszClassName = "projectAlpha";
	windClass.hIconSm = NULL;

	int x = 0;
	int y = 0;


	if (RegisterClassExA (&windClass))
	{
		HWND winHandle = CreateWindowExA (0,
										  windClass.lpszClassName,
										  "Project Alpha",
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
			stillRunning = true;
			while (stillRunning)
			{
				MSG message;
				while (PeekMessageA (&message, 0, 0, 0, PM_REMOVE)) // "Peek", as "get" uses a wait
				{
					if (message.message == WM_QUIT)
					{
						stillRunning = false;
					}
					TranslateMessage (&message);
					DispatchMessageA (&message);
				}
				gradient (winHandle, x, y);
				x++;
				y++;
			}
		}
	}
	else
	{

	}
	return 0;
}