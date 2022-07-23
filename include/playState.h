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
//game modes 
typedef enum game_mode{
    None = 0,
    SinglePlayer = 1,
    Multiplayer = 2 
}game_mode_t;


/**
 * @brief initiate game config values
 * 
 * @return 0 on success
 */
int initGameConfig();

/**
 * @brief initiate Invaders/Playership/Barriers etc
 *
 * @param keep_current_data 1 to keep current_score/lives for next level
 * 
 * @param mode Singleplayer, Multiplayer, None
 * 
 * @return 0 on success
 */
int initiateNewGame(int keep_current_data, game_mode_t mode);

/**
 * @brief initiate game timers
 * 
 * @return 0 on success
 */
int initiateTimer();

/**
 * @brief initiate Multiplayer task
 * 
 * @return 0 on success
 */
int initMpMode();

/**
 * @brief checkButton Presses for higher start score
 * 
 */
void updateStartScore();

/**
 * @brief update cheat code state for infinite lives
 * 
 * @return 1 if infinite lives enabled
 */
int updateInfiniteLives();

/**
 * @brief increase/decrease downward speed of invaders
 * 
 * @param up_down 0 slower, 1 faster
 */
void toggleDownwardSpeed(int up_down);

/**
 * @brief increase/decrease current level
 * 
 * @param up_down 0 lower level, 1 higher level
 */
void toggleCurrentLevel(char up_down);

/**
 * @brief start game timers based on current level
 * 
 */
void startTimer();

/**
 * @brief stop game timers
 * 
 */
void stopTimer();

/**
 * @brief drawInvaders and handle shots
 * 
 * @return returns 1 if player was killed
 */
int drawInvaders();

/**
 * @brief get current player lives
 * 
 * @return returns lives of player
 */
int getPlayerLives();

/**
 * @brief get amount of living invaders
 * 
 * @return returns amount of alive invaders
 */
int getAliveInvaders();

/**
 * @brief get level
 * 
 * @return returns level
 */
int getCurrentLevel();

/**
 * @brief sends RESUME to opponent
 * 
 */
void resumeMpAI();

/**
 * @brief sends HALT to opponent
 * 
 */
void pauseMpAI();

/**
 * @brief updates AI difficulty based on current level
 * 
 */
void setMpDifficulty();

/**
 * @brief checks if AI is running
 *
 * 
 * @return 1 if AI is running, 0 if AI is not sending for more than 5 seconds
 *
 */
char checkAiRunning();

/**
 * @brief exit Game Config
 *
 */
void exitGameConfig();

/**
 * @brief exit Timers
 *
 */
void exitTimers();

/**
 * @brief exit MpMode
 *
 */
void exitMpMode();