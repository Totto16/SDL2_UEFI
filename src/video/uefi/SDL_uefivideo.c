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

#define UEFIVID_DRIVER_NAME "uefi"

SDL_FORCE_INLINE int AddUEFIDisplay(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop);

static int UEFI_VideoInit(_THIS);
static void UEFI_VideoQuit(_THIS);
static void UEFI_GetDisplayModes(_THIS, SDL_VideoDisplay *display);
static int UEFI_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static int UEFI_GetDisplayBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect);
static int UEFI_CreateWindow(_THIS, SDL_Window *window);
static void UEFI_DestroyWindow(_THIS, SDL_Window *window);

typedef struct
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
} DisplayDriverData;

typedef struct
{
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
    UINT32 Pitch;
    UINT32 Mode;
} ModeDriverData;

static SDL_PixelFormatEnum UEFI_get_SDL_Format(EFI_GRAPHICS_PIXEL_FORMAT gop_format)
{
    switch (gop_format) {
    case PixelRedGreenBlueReserved8BitPerColor:
    {
        return SDL_PIXELFORMAT_BGRX8888;
    }
    case PixelBlueGreenRedReserved8BitPerColor:
    {
        return SDL_PIXELFORMAT_RGBX8888;
    }
    default:
    {
        return SDL_PIXELFORMAT_UNKNOWN;
    }
    }
}

/* UEFI driver bootstrap functions */

static void UEFI_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->displays);
    SDL_free(device->driverdata);
    SDL_free(device);
}

static SDL_VideoDevice *UEFI_CreateDevice(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;

    /* Initialize all variables that we clean on shutdown */
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return 0;
    }

    /* Initialize internal data */
    phdata = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!phdata) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }

    device->driverdata = phdata;

    device->VideoInit = UEFI_VideoInit;
    device->VideoQuit = UEFI_VideoQuit;

    device->GetDisplayModes = UEFI_GetDisplayModes;
    device->SetDisplayMode = UEFI_SetDisplayMode;
    device->GetDisplayBounds = UEFI_GetDisplayBounds;

    device->CreateSDLWindow = UEFI_CreateWindow;
    device->DestroyWindow = UEFI_DestroyWindow;

    device->HasScreenKeyboardSupport = SDL_FALSE;
    device->StartTextInput = NULL;
    device->StopTextInput = NULL;

    device->PumpEvents = UEFI_PumpEvents;

    device->CreateWindowFramebuffer = SDL_UEFI_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = SDL_UEFI_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = SDL_UEFI_DestroyWindowFramebuffer;

    device->free = UEFI_DeleteDevice;

    device->quirk_flags = VIDEO_DEVICE_QUIRK_FULLSCREEN_ONLY;

    return device;
}

VideoBootStrap UEFI_bootstrap = { UEFIVID_DRIVER_NAME, "UEFI Video Driver", UEFI_CreateDevice, NULL /* no ShowMessageBox implementation */ };

static int UEFI_VideoInit(_THIS)
{

    SDL_VideoData *driverdata = (SDL_VideoData *)_this->driverdata;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;

    if (!gBS) {
        return SDL_SetError("gBS not set");
    }

    EFI_STATUS Status = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID **)&Gop);

    if (EFI_ERROR(Status)) {
        return SDL_SetError("UEFI GOP not available");
    }

    driverdata->Gop = Gop;

    driverdata->Width =
        Gop->Mode->Info->HorizontalResolution;

    driverdata->Height =
        Gop->Mode->Info->VerticalResolution;

    driverdata->Pitch =
        Gop->Mode->Info->PixelsPerScanLine * 4;

    driverdata->HWFrameBuffer =
        (VOID *)(UINTN)Gop->Mode->FrameBufferBase;

    driverdata->PixelFormat = Gop->Mode->Info->PixelFormat;

    AddUEFIDisplay(Gop);

    return 0;
}

static int UEFI_Init_SDL_DisplayMode(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info, OUT SDL_DisplayMode *sdl_mode, UINT32 Mode)
{
    ModeDriverData *modedata = SDL_malloc(sizeof(ModeDriverData));
    if (!modedata) {
        return 1;
    }

    SDL_zero(*sdl_mode);

    sdl_mode->w = Info->VerticalResolution;

    sdl_mode->h = Info->HorizontalResolution;
    sdl_mode->refresh_rate = 60;
    sdl_mode->format = UEFI_get_SDL_Format(Info->PixelFormat);

    if (sdl_mode->format == SDL_PIXELFORMAT_UNKNOWN) {
        SDL_free(modedata);
        return 1;
    }

    sdl_mode->driverdata = modedata;
    modedata->PixelFormat = Info->PixelFormat;
    // TODO: use Pitch and PixelsPerScanLine correctly in every function that deals with the raw framebuffer
    modedata->Pitch =
        Info->PixelsPerScanLine * 4;
    modedata->Mode = Mode;

    return 0;
}

SDL_FORCE_INLINE int
AddUEFIDisplay(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop)
{
    SDL_VideoDisplay display;
    DisplayDriverData *display_driver_data = SDL_calloc(1, sizeof(DisplayDriverData));
    if (!display_driver_data) {
        return SDL_OutOfMemory();
    }

    SDL_zero(display);

    display_driver_data->Gop = Gop;

    SDL_DisplayMode sdl_mode;
    if (UEFI_Init_SDL_DisplayMode(Gop->Mode->Info, &sdl_mode, Gop->Mode->Mode) != 0) {
        return SDL_SetError("Can't init SDL Display mode");
    }

    display.name = "UEFI GOP Full screen";
    display.desktop_mode = sdl_mode;
    display.current_mode = sdl_mode;
    display.driverdata = display_driver_data;

    return SDL_AddVideoDisplay(&display, SDL_FALSE);
}

static void UEFI_VideoQuit(_THIS)
{
    // TODO
    // i think there is nothing to do here?
}

static void UEFI_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    DisplayDriverData *displaydata = display->driverdata;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = displaydata->Gop;

    for (UINT32 Mode = 0; Mode < Gop->Mode->MaxMode; ++Mode) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info = NULL;
        UINTN SizeOfInfo = 0;

        EFI_STATUS Status = Gop->QueryMode(
            Gop,
            Mode,
            &SizeOfInfo,
            &Info);

        if (EFI_ERROR(Status)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                         "GOP Mode query failed (%u): %lld\n",
                         Mode, Status);
            continue;
        }

        SDL_DisplayMode sdl_mode;
        if (UEFI_Init_SDL_DisplayMode(Info, &sdl_mode, Mode) != 0) {
            continue;
        }

        if (!SDL_AddDisplayMode(display, &sdl_mode)) {
            SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
                         "Can't add mode (%u): SDL_AddDisplayMode failed\n",
                         Mode);
        }

        FreePool(Info);
    }
}

static int UEFI_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    DisplayDriverData *driver_data = (DisplayDriverData *)display->driverdata;
    ModeDriverData *modedata = mode->driverdata;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = driver_data->Gop;

    EFI_STATUS Status = Gop->SetMode(Gop, modedata->Mode);
    if (EFI_ERROR(Status)) {
        return SDL_SetError("GOP mode couldn't be set");
    }

    return 0;
}

static int UEFI_GetDisplayBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    DisplayDriverData *driver_data = (DisplayDriverData *)display->driverdata;
    if (!driver_data) {
        return -1;
    }
    rect->x = 0;
    rect->y = 0;
    rect->w = display->current_mode.w;
    rect->h = display->current_mode.h;

    return 0;
}

static int UEFI_CreateWindow(_THIS, SDL_Window *window)
{
    DisplayDriverData *display_data;
    SDL_WindowData *window_data = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!window_data) {
        return SDL_OutOfMemory();
    }
    display_data = (DisplayDriverData *)SDL_GetDisplayDriverData(window->display_index);
    window_data->Gop = display_data->Gop;
    window->driverdata = window_data;
    return 0;
}

static void UEFI_DestroyWindow(_THIS, SDL_Window *window)
{
    if (!window) {
        return;
    }
    SDL_free(window->driverdata);
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
