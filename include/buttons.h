/**
 * @file score.h
 * @author Maximilian Groezinger
 * @date 23 July 2022
 * @brief Functions to handle the Playing State of SpaceInvadersGame
 *
 * @section licence_sec Licence
 * @verbatim
 ----------------------------------------------------------------------
 Copyright (C) Maximilian Groezinger, 2022
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ----------------------------------------------------------------------
 @endverbatim
 */
#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

//these values are not set by SDL_SCANCODES so using them for mouse buttons
typedef enum{
    MY_SCANCODE_MOUSE_LEFT = 1,
    MY_SCANCODE_MOUSE_RIGHT = 2,
    MY_SCANCODE_MOUSE_MIDDLE = 3
} MY_SCANCODE_MOUSE;

/**
 * @brief initializes Buttons
 *
 */
int buttonLockInit();

/**
 * @brief deletes Buttons after Failure
 *
 */
void buttonLockExit();

/**
 * @brief update all Button Inputs
 *
 */
void xGetButtonInput();

/**
 * @brief get counter value of button presses
 *
 */
int getButtonCounter(SDL_Scancode code);

/**
 * @brief get debounced Button Value
 *
 * @param code SDL_Scancode which button to check
 * 
 * @return //1 pressed, 0 not pressed
 */
int getDebouncedButtonState(SDL_Scancode code);

/**
 * @brief get continuos/not debounced Button Value
 *
 * @param code SDL_Scancode which button to check
 * 
 * @return //1 pressed, 0 not pressed
 */
int getContinuousButtonState(SDL_Scancode code);

/**
 * @brief get debounced Mouse Button Value
 *
 * @param code MY_SCANCODE_Mouse which button to check
 * 
 * @return //1 pressed, 0 not pressed
 */
int getDebouncedMouseState(MY_SCANCODE_MOUSE code);

/**
 * @brief reset Button Counter to 0
 *
 */
void resetButtonCounter(SDL_Scancode code);