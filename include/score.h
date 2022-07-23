/**
 * @file score.h
 * @author Maximilian Groezinger
 * @date 23 July 2022
 * @brief Score handling functions for SpaceInvaders Game
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
 * @brief Draw Scores
 *
 * @param no_current_score does not draw current_score
 *
 */
void drawScore(char no_current_score);

/**
 * @brief get the current score value
 *
 * @return current score
 */
int getCurrentScore();

/**
 * @brief increaseScore
 *
 * @param sp_or_mp None = 0, Singleplayer = 1, Multiplayer = 2
 * to update related high score
 * @param points amount of points to increase score
 *
 */
void increaseScore(int points, char sp_or_mp);

/**
 * @brief resets current score to 0
 *
 */
void resetCurrentScore();

/**
 * @brief initializes score values and creates mutex
 *
 */
void initScore();