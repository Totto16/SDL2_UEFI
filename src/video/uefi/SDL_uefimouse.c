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
#include <events/SDL_mouse_c.h>

int UEFI_InitMouse(_THIS, SDL_VideoData *driverdata)
{

    SDL_MouseData *mouse_data = &(driverdata->mouse_data);

    *mouse_data = (SDL_MouseData){ .type = SDL_MOUSETYPE_NONE, .data = { NULL } };

    // prefer absolute over simple / rel, if none could be found, ignore it
    // abs is more accurate, as relative may drift, when absolute is accurate
    // TODO: do that ^^^^

    if (!gBS) {
        return SDL_SetError("gBS not set");
    }

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

        *mouse_data = (SDL_MouseData){                            //
                                       .type = SDL_MOUSETYPE_REL, //
                                       .data = {                  //
                                                 .rel = (SDL_MouseStateRel){
                                                     .protocol = SimpleMouse,
                                                     .left_button = false,
                                                     .right_button = false,
                                                 } }

        };
        return 0;
    }

try_abs_mouse:
    // TODO: atm we only use the relative

    return 0;

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

static void UEFI_PumpMouseEventsAbs(EFI_ABSOLUTE_POINTER_PROTOCOL *abs)
{
    // TODO
}

#define MOUSE_ID 0

static void UEFI_Push_Mouse_Event_Rel(SDL_MouseStateRel *const rel_state,
                                      const EFI_SIMPLE_POINTER_STATE *const State)
{

    const EFI_SIMPLE_POINTER_MODE *const Mode = rel_state->protocol->Mode;

    // TODO: use window for sending events, pay attention to mode changes, or the window size changes, or should we disallow mode changes after the window creation ?!!?

    // TODO: supply window
    SDL_Window *window = NULL;

    if (Mode->LeftButton) {
        if (rel_state->left_button != State->LeftButton) {
            SDL_SendMouseButton(window, MOUSE_ID, State->LeftButton ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT);
            rel_state->left_button = State->LeftButton;
        }
    }

    if (Mode->RightButton) {
        if (rel_state->right_button != State->RightButton) {
            SDL_SendMouseButton(window, MOUSE_ID, State->RightButton ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_RIGHT);
            rel_state->right_button = State->RightButton;
        }
    }

    if (Mode->ResolutionZ != 0) {
        // treat the Z axis as scroll wheel X axis
        // TODO: maybe use the resolution here?
        if (State->RelativeMovementZ != 0) {
            SDL_SendMouseWheel(window, MOUSE_ID, (float)State->RelativeMovementZ, 0.0F, SDL_MOUSEWHEEL_NORMAL);
        }
    }

    int x_mov = 0;
    int y_mov = 0;

    bool movement = false;

    if (Mode->ResolutionX != 0 && State->RelativeMovementX != 0) {
        x_mov = State->RelativeMovementX;
        movement = true;
    }

    if (Mode->ResolutionY != 0 && State->RelativeMovementY != 0) {
        y_mov = State->RelativeMovementY;
        movement = true;
    }

    if (!movement) {
        return;
    }

    SDL_SendMouseMotion(window, MOUSE_ID, (int)true, x_mov, y_mov);
}

static void UEFI_PumpMouseEventsRel(SDL_MouseStateRel *const rel)
{

    EFI_SIMPLE_POINTER_STATE State;

    EFI_STATUS Status = rel->protocol->GetState(rel->protocol, &State);

    if (Status == EFI_NOT_READY) {
        // no mouse movement ready
        return;
    }

    if (EFI_ERROR(Status)) {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "Error in reading mouse state: %lld\n",
                     Status);
        return;
    }

    UEFI_Push_Mouse_Event_Rel(rel, &State);
}

void UEFI_PumpMouseEvents(_THIS)
{

    SDL_VideoData *driverdata = (SDL_VideoData *)_this->driverdata;

    SDL_MouseData *const mouse_data = &(driverdata->mouse_data);

    switch (mouse_data->type) {
    case SDL_MOUSETYPE_NONE:
    {
        // Nothing to do
        return;
    }

    case SDL_MOUSETYPE_ABS:
    {
        UEFI_PumpMouseEventsAbs(mouse_data->data.abs);
        return;
    }

    case SDL_MOUSETYPE_REL:
    {
        UEFI_PumpMouseEventsRel(&(mouse_data->data.rel));
        return;
    }
    default:
    {
        SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                     "Invalid mouse input type: %d\n",
                     mouse_data->type);
        return;
    }
    }
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
