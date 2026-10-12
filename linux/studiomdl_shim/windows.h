// Just enough of <windows.h> for the SDK's studiomdl (bmpread.c / lbmlib.c) on Linux.
// See build_studiomdl.sh.
#pragma once
#define _WINDOWS_
#include <stdint.h>
#include <string.h>

typedef uint8_t BYTE;
typedef uint8_t UCHAR;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint32_t ULONG;
typedef int32_t LONG;

#pragma pack(push, 1)
typedef struct { WORD bfType; DWORD bfSize; WORD bfReserved1, bfReserved2; DWORD bfOffBits; } BITMAPFILEHEADER;
typedef struct
{
	DWORD biSize;
	LONG biWidth, biHeight;
	WORD biPlanes, biBitCount;
	DWORD biCompression, biSizeImage;
	LONG biXPelsPerMeter, biYPelsPerMeter;
	DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct { BYTE rgbtBlue, rgbtGreen, rgbtRed; } RGBTRIPLE;
#pragma pack(pop)

#define BI_RGB 0
#define MAKEWORD( a, b ) ( (WORD)( ( (BYTE)( a ) ) | ( (WORD)( (BYTE)( b ) ) ) << 8 ) )
