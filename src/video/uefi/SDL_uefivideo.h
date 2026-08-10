/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
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
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifndef SDL_uefivideo_h_
#define SDL_uefivideo_h_

#include <Uefi.h>

#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>

#include "../SDL_sysvideo.h"

typedef struct
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

    VOID *HWFrameBuffer;
} SDL_VideoData;

typedef struct SDL_WindowData
{
    SDL_VideoData *video_ref;
} SDL_WindowData;

typedef struct
{
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT32 PixelsPerScanLine;
    UINT32 ModeIdx;
} ModeDriverData;

#endif /* SDL_uefivideo_h_ */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
