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

SDL_FORCE_INLINE int
CopyFramebuffertoUEFI(SDL_Surface *surface, SDL_VideoData *video_data, ModeDriverData *mode_data);

// TODO: how does sdl2 create / update / delete teh framebuffer, when updating mode? do we need to do that manually or does sdl do that automatically?
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

int SDL_UEFI_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface = (SDL_Surface *)SDL_GetWindowData(window, UEFI_SURFACE);
    if (!surface) {
        return SDL_SetError("%s: Unable to get the window surface.", __func__);
    }

    SDL_VideoData *video_data = (SDL_VideoData *)_this->driverdata;

    if (surface->format->BytesPerPixel != sizeof(uint32_t)) {
        return SDL_SetError("%s: Invalid BytesPerPixel: %d.", __func__, surface->format->BytesPerPixel);
    }

    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);

    ModeDriverData *mode_data = mode.driverdata;

    return CopyFramebuffertoUEFI(surface, video_data, mode_data);
}

SDL_FORCE_INLINE int
CopyFramebuffertoUEFI(SDL_Surface *surface, SDL_VideoData *video_data, ModeDriverData *mode_data)
{

    // NOTE: we assert here, that the surface has the same width and height as the framebuffer, otherwise we forgot to update the surface on mode update!
    if (surface->w != mode_data->HorizontalResolution || surface->h != mode_data->VerticalResolution) {
        return SDL_SetError("%s: Surface and Framebuffer dimensions don't match: %dx%d != %ux%u", __func__, surface->w, surface->h, mode_data->HorizontalResolution, mode_data->VerticalResolution);
    }

    if (SDL_MUSTLOCK(surface)) {
        int err = SDL_LockSurface(surface);
        if (err != 0) {
            return err;
        }
    }

    size_t ValidLineSize = mode_data->HorizontalResolution * sizeof(UINT32);

    for (UINT32 y = 0; y < mode_data->VerticalResolution; ++y) {
        const uint32_t *source = ((uint32_t *)surface->pixels) + (y * surface->w);
        uint32_t *dest = ((uint32_t *)video_data->HWFrameBuffer) + (y * mode_data->PixelsPerScanLine);
        SDL_memcpy(dest, source, ValidLineSize);
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }

    return 0;
}

void SDL_UEFI_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface;
    surface = (SDL_Surface *)SDL_SetWindowData(window, UEFI_SURFACE, NULL);
    SDL_FreeSurface(surface);
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
