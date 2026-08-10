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

#include "SDL_uefievents.h"
#include "SDL_uefimouse.h"
#include "SDL_uefivideo.h"

#include <SDL_keyboard_c.h>
#include <SDL_scancode.h>
#include <stdbool.h>

int UEFI_InitMouse(_THIS, SDL_VideoData *driverdata)
{

    SDL_MouseData *mouse_data = &(driverdata->mouse_data);

    *mouse_data = (SDL_MouseData){ .type = SDL_MOUSETYPE_NONE, .data = { NULL } };

    // prefer simple / rel over absolute, if none could be found, ignore it

    EFI_SIMPLE_POINTER_PROTOCOL *SimpleMouse;

    EFI_STATUS Status = gBS->LocateProtocol(
        &gEfiSimplePointerProtocolGuid,
        NULL,
        (VOID **)&SimpleMouse);

    if (!EFI_ERROR(Status)) {

        Status = SimpleMouse->Reset(SimpleMouse, false);

        if (EFI_ERROR(Status)) {
            goto try_abs_mouse;
        }

        *mouse_data = (SDL_MouseData){ .type = SDL_MOUSETYPE_REL, .data = { .rel = SimpleMouse } };
        return 0;
    }

try_abs_mouse:

    EFI_ABSOLUTE_POINTER_PROTOCOL *AbsMouse;

    Status = gBS->LocateProtocol(
        &gEfiAbsolutePointerProtocolGuid,
        NULL,
        (VOID **)&AbsMouse);

    if (!EFI_ERROR(Status)) {

        Status = AbsMouse->Reset(AbsMouse, false);

        if (EFI_ERROR(Status)) {
            goto all_mouse_failed;
        }

        *mouse_data = (SDL_MouseData){ .type = SDL_MOUSETYPE_ABS, .data = { .abs = AbsMouse } };
        return 0;
    }

all_mouse_failed:

    return 0;
}

void UEFI_QuitMouse(_THIS)
{
    // NOOP
    // TODO: is this really a noop?
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
