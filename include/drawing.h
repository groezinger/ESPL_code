/**
 * @file score.h
 * @author Maximilian Groezinger
 * @date 23 July 2022
 * @brief DrawFunctions for the single States
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

/**
 * @brief draws the Menu Screen
 *
 */
void drawMenuState();

/**
 * @brief draws Pause Screen
 *
 */
void drawPause();

/**
 * @brief draws AI not Running Screen
 *
 */
void drawAiNotRunning();

/**
 * @brief draws Game Over Screen
 *
 */
void drawGameOver();

/**
 * @brief draws Next Level Screen
 *
 */
void drawNextLevel();

/**
 * @brief draws Start Multiplayer Screen
 *
 */
void drawStartMp();

/**
 * @brief draws Start Singleplayer Screen
 *
 */
void drawStartSp();

/**
 * @brief draws Playing Screen
 *
 */
void drawPlay();

/**
 * @brief initializes Drawing Task
 *
 * @return 0 on success
 */
int initMyDrawing();

/**
 * @brief exit drawing
 *
 */
void exitMyDrawing();