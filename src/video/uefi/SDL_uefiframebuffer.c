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

#ifdef SDL_VIDEO_DRIVER_UEFI

#include "../SDL_sysvideo.h"
#include "SDL_uefiframebuffer_c.h"
#include "SDL_uefivideo.h"

#define UEFI_SURFACE "_SDL_UEFISurface"

typedef struct
{
    int width, height;
} Dimensions;

SDL_FORCE_INLINE void CopyFramebuffertoUEFI_16(uint16_t *dest, const Dimensions dest_dim, const uint16_t *source, const Dimensions source_dim);
SDL_FORCE_INLINE void CopyFramebuffertoUEFI_24(uint8_t *dest, const Dimensions dest_dim, const uint8_t *source, const Dimensions source_dim);
SDL_FORCE_INLINE void CopyFramebuffertoUEFI_32(uint32_t *dest, const Dimensions dest_dim, const uint32_t *source, const Dimensions source_dim);
SDL_FORCE_INLINE int GetDestOffset(int x, int y, int dest_width);
SDL_FORCE_INLINE int GetSourceOffset(int x, int y, int source_width);
SDL_FORCE_INLINE void FlushUEFIBuffer(const void *buffer, uint32_t bufsize, EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop);

int SDL_UEFI_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_Surface *framebuffer;
    SDL_DisplayMode mode;
    int w, h;

    SDL_UEFI_DestroyWindowFramebuffer(_this, window);

    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);
    SDL_GetWindowSizeInPixels(window, &w, &h);
    framebuffer = SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BYTESPERPIXEL(mode.format), mode.format);

    if (!framebuffer) {
        return SDL_OutOfMemory();
    }

    SDL_SetWindowData(window, UEFI_SURFACE, framebuffer);
    *format = mode.format;
    *pixels = framebuffer->pixels;
    *pitch = framebuffer->pitch;
    return 0;
}

static void *UEFI_get_Framebuffer(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop, OUT uint32_t *width, OUT uint32_t *height)
{
    void *FrameBuffer =
        (VOID *)(UINTN)Gop->Mode->FrameBufferBase;

    *width =
        Gop->Mode->Info->HorizontalResolution;

    *height =
        Gop->Mode->Info->VerticalResolution;

    return FrameBuffer;
}

int SDL_UEFI_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *drv_data = (SDL_WindowData *)window->driverdata;
    SDL_Surface *surface;
    uint32_t width, height;
    void *framebuffer;
    uint32_t bufsize;

    surface = (SDL_Surface *)SDL_GetWindowData(window, UEFI_SURFACE);
    if (!surface) {
        return SDL_SetError("%s: Unable to get the window surface.", __func__);
    }

    // TODO: use Pitch and PixelsPerScanLine correctly in every function that deals with the raw framebuffer
    /* Get the UEFI internal framebuffer and its size */
    framebuffer = UEFI_get_Framebuffer(drv_data->Gop, &width, &height);

    bufsize = width * height * 4;

    if (surface->format->BytesPerPixel == 2)
        CopyFramebuffertoUEFI_16(framebuffer, (Dimensions){ width, height },
                                 surface->pixels, (Dimensions){ surface->w, surface->h });
    else if (surface->format->BytesPerPixel == 3)
        CopyFramebuffertoUEFI_24(framebuffer, (Dimensions){ width, height },
                                 surface->pixels, (Dimensions){ surface->w, surface->h });
    else
        CopyFramebuffertoUEFI_32(framebuffer, (Dimensions){ width, height },
                                 surface->pixels, (Dimensions){ surface->w, surface->h });
    FlushUEFIBuffer(framebuffer, bufsize, drv_data->Gop);

    return 0;
}

SDL_FORCE_INLINE void
CopyFramebuffertoUEFI_16(uint16_t *dest, const Dimensions dest_dim, const uint16_t *source, const Dimensions source_dim)
{
    int rows = SDL_min(dest_dim.width, source_dim.height);
    int cols = SDL_min(dest_dim.height, source_dim.width);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const uint16_t *s = source + GetSourceOffset(x, y, source_dim.width);
            uint16_t *d = dest + GetDestOffset(x, y, dest_dim.width);
            *d = *s;
        }
    }
}

SDL_FORCE_INLINE void
CopyFramebuffertoUEFI_24(uint8_t *dest, const Dimensions dest_dim, const uint8_t *source, const Dimensions source_dim)
{
    int rows = SDL_min(dest_dim.width, source_dim.height);
    int cols = SDL_min(dest_dim.height, source_dim.width);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const uint8_t *s = source + GetSourceOffset(x, y, source_dim.width) * 3;
            uint8_t *d = dest + GetDestOffset(x, y, dest_dim.width) * 3;
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
        }
    }
}

SDL_FORCE_INLINE void
CopyFramebuffertoUEFI_32(uint32_t *dest, const Dimensions dest_dim, const uint32_t *source, const Dimensions source_dim)
{
    int rows = SDL_min(dest_dim.width, source_dim.height);
    int cols = SDL_min(dest_dim.height, source_dim.width);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const uint32_t *s = source + GetSourceOffset(x, y, source_dim.width);
            uint32_t *d = dest + GetDestOffset(x, y, dest_dim.width);
            *d = *s;
        }
    }
}

SDL_FORCE_INLINE int
GetDestOffset(int x, int y, int dest_width)
{
    return dest_width - y - 1 + dest_width * x;
}

SDL_FORCE_INLINE int
GetSourceOffset(int x, int y, int source_width)
{
    return x + y * source_width;
}

SDL_FORCE_INLINE void
FlushUEFIBuffer(const void *buffer, uint32_t bufsize, EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop)
{
    // TODO: check if this really is a no-op
    //  no-op
}

void SDL_UEFI_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;
    surface = (SDL_Surface *)SDL_SetWindowData(window, UEFI_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
