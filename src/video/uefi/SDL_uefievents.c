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

#include "SDL_uefivideo.h"

#include "SDL_uefievents.h"
#include "SDL_uefimouse.h"

#include <SDL_keyboard_c.h>
#include <SDL_scancode.h>
#include <stdbool.h>

int UEFI_InitKeyboard(_THIS, SDL_VideoData *driverdata)
{

    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *InputEx;

    if (!gBS) {
        return SDL_SetError("gBS not set");
    }

    EFI_STATUS Status = gBS->LocateProtocol(
        &gEfiSimpleTextInputExProtocolGuid,
        NULL,
        (VOID **)&InputEx);

    if (EFI_ERROR(Status)) {
        return SDL_SetError("UEFI SimpleTextInputEx Protocol not available");
    }

    Status = InputEx->Reset(InputEx, false);

    if (EFI_ERROR(Status)) {
        return SDL_SetError("Input device doesn't work properly");
    }

    // use SetState() to support incomplete keystrokes
    // The SetState() function allows the input device hardware to have state settings adjusted. By calling the SetState() function with the EFI_KEY_STATE_EXPOSED bit active in the KeyToggleState parameter, this will enable the ReadKeyStrokeEx function to return incomplete keystrokes such as the holding down of certain keys which are expressed as a part of KeyState when there is no Key data.
    EFI_KEY_TOGGLE_STATE KeyToggleState = EFI_KEY_STATE_EXPOSED;
    Status = InputEx->SetState(InputEx, &KeyToggleState);

    if (EFI_ERROR(Status)) {
        return SDL_SetError("Input device doesn't work properly: can't receive more detailed key states");
    }

    driverdata->InputEx = InputEx;
    return 0;
}

void UEFI_QuitKeyboard(_THIS)
{
    // NOOP
    // TODO: is this really a noop?
}

static SDL_Scancode
UEFI_ScanCodeToSDL(UINT16 ScanCode)
{
    switch (ScanCode) {
    case SCAN_NULL:
        return SDL_SCANCODE_UNKNOWN;
    //
    case SCAN_UP:
        return SDL_SCANCODE_UP;
    case SCAN_DOWN:
        return SDL_SCANCODE_DOWN;
    case SCAN_LEFT:
        return SDL_SCANCODE_LEFT;
    case SCAN_RIGHT:
        return SDL_SCANCODE_RIGHT;
    //
    case SCAN_HOME:
        return SDL_SCANCODE_HOME;
    case SCAN_END:
        return SDL_SCANCODE_END;
    case SCAN_INSERT:
        return SDL_SCANCODE_INSERT;
    case SCAN_DELETE:
        return SDL_SCANCODE_DELETE;
    case SCAN_PAGE_UP:
        return SDL_SCANCODE_PAGEUP;
    case SCAN_PAGE_DOWN:
        return SDL_SCANCODE_PAGEDOWN;
    //
    case SCAN_ESC:
        return SDL_SCANCODE_ESCAPE;
    //
    case SCAN_F1:
        return SDL_SCANCODE_F1;
    case SCAN_F2:
        return SDL_SCANCODE_F2;
    case SCAN_F3:
        return SDL_SCANCODE_F3;
    case SCAN_F4:
        return SDL_SCANCODE_F4;
    case SCAN_F5:
        return SDL_SCANCODE_F5;
    case SCAN_F6:
        return SDL_SCANCODE_F6;
    case SCAN_F7:
        return SDL_SCANCODE_F7;
    case SCAN_F8:
        return SDL_SCANCODE_F8;
    case SCAN_F9:
        return SDL_SCANCODE_F9;
    case SCAN_F10:
        return SDL_SCANCODE_F10;
    case SCAN_F11:
        return SDL_SCANCODE_F11;
    case SCAN_F12:
        return SDL_SCANCODE_F12;
    case SCAN_F13:
        return SDL_SCANCODE_F13;
    case SCAN_F14:
        return SDL_SCANCODE_F14;
    case SCAN_F15:
        return SDL_SCANCODE_F15;
    case SCAN_F16:
        return SDL_SCANCODE_F16;
    case SCAN_F17:
        return SDL_SCANCODE_F17;
    case SCAN_F18:
        return SDL_SCANCODE_F18;
    case SCAN_F19:
        return SDL_SCANCODE_F19;
    case SCAN_F20:
        return SDL_SCANCODE_F20;
    case SCAN_F21:
        return SDL_SCANCODE_F21;
    case SCAN_F22:
        return SDL_SCANCODE_F22;
    case SCAN_F23:
        return SDL_SCANCODE_F23;
    case SCAN_F24:
        return SDL_SCANCODE_F24;
    //
    case SCAN_MUTE:
        return SDL_SCANCODE_MUTE;
    case SCAN_VOLUME_UP:
        return SDL_SCANCODE_VOLUMEUP;
    case SCAN_VOLUME_DOWN:
        return SDL_SCANCODE_VOLUMEDOWN;
    case SCAN_BRIGHTNESS_UP:
        return SDL_SCANCODE_BRIGHTNESSUP;
    case SCAN_BRIGHTNESS_DOWN:
        return SDL_SCANCODE_BRIGHTNESSDOWN;
    case SCAN_SUSPEND:
        return SDL_SCANCODE_SLEEP;
    case SCAN_HIBERNATE:
        return SDL_SCANCODE_SLEEP;
    case SCAN_TOGGLE_DISPLAY:
        return SDL_SCANCODE_UNKNOWN;
    case SCAN_RECOVERY:
        return SDL_SCANCODE_EJECT;
    case SCAN_EJECT:
        return SDL_SCANCODE_EJECT;

    default:
        return SDL_SCANCODE_UNKNOWN;
    }
}

static SDL_Keymod
UEFI_GetModifiers(const EFI_KEY_STATE *const KeyState)
{
    SDL_Keymod mod = KMOD_NONE;

    UINT32 shift_state = KeyState->KeyShiftState;

    if ((shift_state & EFI_SHIFT_STATE_VALID) == 0) {
        goto process_toggle_state;
    }

    if ((shift_state & EFI_RIGHT_SHIFT_PRESSED) != 0) {
        mod |= KMOD_RSHIFT;
    }

    if ((shift_state & EFI_LEFT_SHIFT_PRESSED) != 0) {
        mod |= KMOD_LSHIFT;
    }

    if ((shift_state & EFI_RIGHT_CONTROL_PRESSED) != 0) {
        mod |= KMOD_RCTRL;
    }

    if ((shift_state & EFI_LEFT_CONTROL_PRESSED) != 0) {
        mod |= KMOD_LCTRL;
    }

    if ((shift_state & EFI_RIGHT_ALT_PRESSED) != 0) {
        mod |= KMOD_RALT;
    }

    if ((shift_state & EFI_LEFT_ALT_PRESSED) != 0) {
        mod |= KMOD_LALT;
    }

    if ((shift_state & EFI_RIGHT_LOGO_PRESSED) != 0) {
        mod |= KMOD_RGUI;
    }

    if ((shift_state & EFI_LEFT_LOGO_PRESSED) != 0) {
        mod |= KMOD_LGUI;
    }

    if ((shift_state & EFI_MENU_KEY_PRESSED) != 0) {
        // TODO
        //  do nothing for now
    }

    if ((shift_state & EFI_SYS_REQ_PRESSED) != 0) {
        // TODO
        //  do nothing for now
    }

process_toggle_state:
    UINT32 toggle_state = KeyState->KeyToggleState;

    if ((toggle_state & EFI_TOGGLE_STATE_VALID) == 0) {
        goto return_mode_state;
    }

    if ((toggle_state & EFI_SCROLL_LOCK_ACTIVE) != 0) {
        mod |= KMOD_SCROLL;
    }

    if ((toggle_state & EFI_NUM_LOCK_ACTIVE) != 0) {
        mod |= KMOD_NUM;
    }

    if ((toggle_state & EFI_CAPS_LOCK_ACTIVE) != 0) {
        mod |= KMOD_CAPS;
    }

return_mode_state:

    return mod;
}

#define UEFI_MAX_KEY_TEXT_LENGTH 8

static void UEFI_Push_Key_Event(
    const EFI_KEY_DATA *const KeyData)
{

    // see https://uefi.org/specs/UEFI/2.9_A/12_Protocols_Console_Support.html#efi-simple-text-input-ex-protocol-readkeystrokeex
    // on the meaning of states and values

    SDL_Keymod mode = UEFI_GetModifiers(&(KeyData->KeyState));

    SDL_SetModState(mode);

    // Printable character.
    if (KeyData->Key.UnicodeChar != 0) {
        char text[UEFI_MAX_KEY_TEXT_LENGTH + 1];

        char *end = SDL_UCS4ToUTF8(
            (Uint32)KeyData->Key.UnicodeChar,
            text);

        if (end > &text[UEFI_MAX_KEY_TEXT_LENGTH]) {

            SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                         "Error in reading key text, can't convert UTF-16 to UTF-8\n");
            return;
            return;
        }

        *end = '\0';

        SDL_SendKeyboardText(text);

        // TODO: does uefi support keymaps or locales??
        // NOTE: uses US ASCII, maybe we should set keymaps, but how, does uefi expose some sort of language?
        SDL_SendKeyboardUnicodeKey(KeyData->Key.UnicodeChar);

        return;
    }

    SDL_Scancode scancode = UEFI_ScanCodeToSDL(KeyData->Key.ScanCode);

    // Special key.

    if (scancode != SDL_SCANCODE_UNKNOWN) {
        // TODO: when to release this? atm i am just sending a release event immediately afterwards
        SDL_SendKeyboardKey(
            SDL_PRESSED,
            scancode);

        SDL_SendKeyboardKey(
            SDL_RELEASED,
            scancode);
    }
}

void UEFI_PumpEvents(_THIS)
{
    SDL_VideoData *driverdata = (SDL_VideoData *)_this->driverdata;

    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *InputEx = driverdata->InputEx;

    { // check for keystrokes

        EFI_KEY_DATA KeyData;

        EFI_STATUS Status = InputEx->ReadKeyStrokeEx(InputEx, &KeyData);

        if (Status == EFI_NOT_READY) {
            // no keypress ready
            goto end_keypress_detect;
        }

        if (EFI_ERROR(Status)) {
            SDL_LogError(SDL_LOG_CATEGORY_INPUT,
                         "Error in reading key stroke: %lld\n",
                         Status);
            goto end_keypress_detect;
        }

        UEFI_Push_Key_Event(&KeyData);
    }
end_keypress_detect:

    // TODO
}

#endif /* SDL_VIDEO_DRIVER_UEFI */

/* vi: set sts=4 ts=4 sw=4 expandtab: */
