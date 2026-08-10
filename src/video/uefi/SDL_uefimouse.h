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

#ifndef SDL_uefimouse_h_
#define SDL_uefimouse_h_

#include "../SDL_sysvideo.h"

#include <Protocol/AbsolutePointer.h>
#include <Protocol/SimplePointer.h>

extern int UEFI_InitMouse(_THIS, struct SDL_VideoData *driverdata);
extern void UEFI_QuitMouse(_THIS);

/**
 * The blend mode used in SDL_RenderCopy() and drawing operations.
 */
typedef enum SDL_MouseType
{
    SDL_MOUSETYPE_NONE = 0,
    SDL_MOUSETYPE_ABS,
    SDL_MOUSETYPE_REL,

} SDL_MouseType;

typedef struct SDL_MouseData
{
    SDL_MouseType type;
    union
    {
        EFI_ABSOLUTE_POINTER_PROTOCOL *abs;
        EFI_SIMPLE_POINTER_PROTOCOL *rel;
    } data;
} SDL_MouseData;

#endif /* SDL_uefimouse_h_ */

/* vi: set ts=4 sw=4 expandtab: */
