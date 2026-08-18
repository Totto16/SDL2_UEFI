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

#define UEFI_SURFACE_FB    "_SDL_UEFI_Surface_FB"
#define UEFI_HW_BACKBUFFER "_SDL_UEFI_HW_BACKBUFFER"

typedef struct SDL_UEFI_HW_Backbuffer
{
    SDL_Surface *surface;
    void *data;
} SDL_UEFI_HW_Backbuffer;

SDL_FORCE_INLINE int
CopyFramebuffertoUEFI(SDL_Surface *surface_fb, SDL_UEFI_HW_Backbuffer *hw_backbuffer, SDL_VideoData *video_data, ModeDriverData *mode_data);

static int SDL_UEFI_helper_impl_get_display_mode_from_window_checked(SDL_Window *window, OUT SDL_DisplayMode *mode)
{
    int display_index = SDL_GetWindowDisplayIndex(window);

    if (display_index < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "%s: Couldn_t get the display index: %s", __func__, SDL_GetError());
        return SDL_SetError("Unable to get the display index.");
    }

    int result = SDL_GetDesktopDisplayMode(display_index, mode);

    if (result != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                     "%s: Couldn_t get the display mode for index %d: %s", __func__, display_index, SDL_GetError());
        return SDL_SetError("Unable to get the display mode for index %d", display_index);
    }

    ModeDriverData *mode_data = (ModeDriverData *)mode->driverdata;

    if (mode_data == NULL) {
        return SDL_SetError("Invalid Mode: Mode data is not initialized, display not initialized correctly.");
    }

    return 0;
}

static SDL_UEFI_HW_Backbuffer *SDL_UEFI_Create_SDL_UEFI_HW_Backbuffer(const SDL_DisplayMode *const mode, const ModeDriverData *const mode_data)
{
    SDL_UEFI_HW_Backbuffer *hw_backbuffer = (SDL_UEFI_HW_Backbuffer *)SDL_malloc(sizeof(SDL_UEFI_HW_Backbuffer));
    if (!hw_backbuffer) {
        SDL_OutOfMemory();
        return NULL;
    }

    UINT32 pitch = mode_data->PixelsPerScanLine * sizeof(uint32_t);

    void *data = SDL_malloc(pitch * mode->h);
    if (!data) {
        SDL_OutOfMemory();
        return NULL;
    }
    hw_backbuffer->data = data;

    // the format needs to be EFI_GRAPHICS_OUTPUT_BLT_PIXEL
    //     typedef struct {
    //  UINT8                        Blue;
    //  UINT8                        Green;
    //  UINT8                        Red;
    //  UINT8                        Reserved;
    // } EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

    // TODO: does this depend on endianess??
    SDL_PixelFormatEnum efi_blt_format = SDL_PIXELFORMAT_BGRX8888;

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(hw_backbuffer->data, mode->w, mode->h, SDL_BITSPERPIXEL(efi_blt_format), pitch, efi_blt_format);

    if (!surface) {
        SDL_OutOfMemory();
        return NULL;
    }
    hw_backbuffer->surface = surface;

    return hw_backbuffer;
}

static void SDL_UEFI_Destroy_SDL_UEFI_HW_Backbuffer(SDL_UEFI_HW_Backbuffer *buffer)
{
    SDL_FreeSurface(buffer->surface);
    SDL_free(buffer->data);
    SDL_free(buffer);
}

// TODO: how does sdl2 create / update / delete the framebuffer, when updating mode? do we need to do that manually or does sdl do that automatically?
int SDL_UEFI_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{

    int w, h;

    SDL_DisplayMode mode;
    int mode_result = SDL_UEFI_helper_impl_get_display_mode_from_window_checked(window, &mode);
    if (mode_result != 0) {
        return mode_result;
    }

    SDL_UEFI_DestroyWindowFramebuffer(_this, window);
    SDL_GetWindowSizeInPixels(window, &w, &h);

    SDL_Surface *surface_fb = SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(mode.format), mode.format);

    if (!surface_fb) {
        return SDL_OutOfMemory();
    }

    SDL_SetWindowData(window, UEFI_SURFACE_FB, surface_fb);

    ModeDriverData *mode_data = (ModeDriverData *)mode.driverdata;

    SDL_assert(mode_data != NULL);

    SDL_UEFI_HW_Backbuffer *hw_backbuffer = SDL_UEFI_Create_SDL_UEFI_HW_Backbuffer(&mode, mode_data);

    if (!hw_backbuffer) {
        return SDL_OutOfMemory();
    }

    SDL_SetWindowData(window, UEFI_HW_BACKBUFFER, hw_backbuffer);

    *format = mode.format;
    *pixels = surface_fb->pixels;
    *pitch = surface_fb->pitch;

    return 0;
}

int SDL_UEFI_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface_fb = (SDL_Surface *)SDL_GetWindowData(window, UEFI_SURFACE_FB);
    if (!surface_fb) {
        return SDL_SetError("%s: Unable to get the window surface.", __func__);
    }

    SDL_UEFI_HW_Backbuffer *hw_backbuffer = (SDL_UEFI_HW_Backbuffer *)SDL_GetWindowData(window, UEFI_HW_BACKBUFFER);
    if (!hw_backbuffer) {
        return SDL_SetError("%s: Unable to get the window hw backbuffer.", __func__);
    }

    SDL_VideoData *video_data = (SDL_VideoData *)_this->driverdata;

    if (surface_fb->format->BytesPerPixel != sizeof(uint32_t)) {
        return SDL_SetError("%s: Invalid BytesPerPixel: %d.", __func__, surface_fb->format->BytesPerPixel);
    }

    SDL_DisplayMode mode;
    int mode_result = SDL_UEFI_helper_impl_get_display_mode_from_window_checked(window, &mode);
    if (mode_result != 0) {
        return mode_result;
    }

    ModeDriverData *mode_data = (ModeDriverData *)mode.driverdata;

    SDL_assert(mode_data != NULL);

    return CopyFramebuffertoUEFI(surface_fb, hw_backbuffer, video_data, mode_data);
}

SDL_FORCE_INLINE SDL_Rect SDL_GetRectFromSurface(const SDL_Surface *const surface)
{
    return (SDL_Rect){
        0,
        0,
        surface->w,
        surface->h
    };
}

SDL_FORCE_INLINE int
CopyFramebuffertoUEFI(SDL_Surface *surface_fb, SDL_UEFI_HW_Backbuffer *hw_backbuffer, SDL_VideoData *video_data, ModeDriverData *mode_data)
{

    if (mode_data == NULL) {
        return SDL_SetError("Invalid Mode: Mode data is not initialized (%p)", mode_data);
    }

    SDL_Surface *surface_hw = hw_backbuffer->surface;

    // NOTE: we assert here, that the hw_backbuffer has the same width and height as the framebuffer, otherwise we forgot to update the hw_backbuffer on mode update!
    if (surface_hw->w != mode_data->HorizontalResolution || surface_hw->h != mode_data->VerticalResolution) {
        return SDL_SetError("HW Backbuffer and Framebuffer dimensions don't match: %dx%d != %ux%u", surface_hw->w, surface_hw->h, mode_data->HorizontalResolution, mode_data->VerticalResolution);
    }

    {
        SDL_Rect rect_fb = SDL_GetRectFromSurface(surface_fb);
        SDL_Rect rect_hw = SDL_GetRectFromSurface(surface_hw);

        // blit the framebuffer into the hw backbuffer, so that we can use teh GOP call to just copy that data!
        int result = SDL_BlitSurface(surface_fb, &rect_fb, surface_hw, &rect_hw);

        if (result != 0) {
            return result;
        }
    }

    // copy the pixel data from the hw buffer to the GOP
    if (SDL_MUSTLOCK(surface_hw)) {
        int err = SDL_LockSurface(surface_hw);
        if (err != 0) {
            return err;
        }
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = video_data->graphics_data.Gop;

    // TODO: is the width the pitch or just the width???
    EFI_STATUS Status = Gop->Blt(Gop, surface_hw->pixels, EfiBltBufferToVideo, 0, 0, 0, 0, surface_hw->w, surface_hw->h, 0);
    if (EFI_ERROR(Status)) {
        return SDL_SetError("UEFI GOP Blt error");
    }

    if (SDL_MUSTLOCK(surface_hw)) {
        SDL_UnlockSurface(surface_hw);
    }

    return 0;
}

void SDL_UEFI_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface_fb = (SDL_Surface *)SDL_SetWindowData(window, UEFI_SURFACE_FB, NULL);
    SDL_FreeSurface(surface_fb);

    SDL_UEFI_HW_Backbuffer *hw_backbuffer = (SDL_UEFI_HW_Backbuffer *)SDL_SetWindowData(window, UEFI_HW_BACKBUFFER, NULL);
    SDL_UEFI_Destroy_SDL_UEFI_HW_Backbuffer(hw_backbuffer);
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
