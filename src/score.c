#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"
#include "buttons.h"
#include "score.h"


typedef struct Score{
    int high_score_sp;
    int high_score_mp;
    int current_score;
    SemaphoreHandle_t score_lock;
} Score_t;

static Score_t Score;


void initScore(){
    Score.current_score = 0;
    Score.high_score_mp = 0;
    Score.high_score_sp = 0;
    Score.score_lock = xSemaphoreCreateMutex();
}

void resetCurrentScore(){
    if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){
        Score.current_score = 0;
        xSemaphoreGive(Score.score_lock);
    }
}

void increaseScore(int points, char sp_or_mp){ //1 SP, 2MP, 0 MenuSpecialCase
    if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){ 
        if(Score.current_score + points >= 0){
            Score.current_score = Score.current_score + points;
        }
        if(Score.current_score >= Score.high_score_sp && sp_or_mp == 1){
            Score.high_score_sp = Score.current_score;
        } else if( Score.current_score >= Score.high_score_mp && sp_or_mp == 2){
            Score.high_score_mp = Score.current_score;
        }
        xSemaphoreGive(Score.score_lock);
    }
}

int getCurrentScore(){
    int return_value = 0;
    if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){
        return_value = Score.current_score;
        xSemaphoreGive(Score.score_lock);
    }
    return return_value;
}

void drawScore(char no_current_score){
    static char current_score[40] = { 0 };
    static int current_score_width = 0;
    static char high_score_sp[30] = { 0 };
    static int high_score_sp_width = 0;
    static char high_score_mp[30] = { 0 };
    static int high_score_mp_width = 0;
    if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){
        sprintf(current_score, "CURRENT SCORE: %d", Score.current_score);
        sprintf(high_score_sp, "HIGH SCORE SP: %d", Score.high_score_sp);
        sprintf(high_score_mp, "HIGH SCORE MP: %d", Score.high_score_mp);
        if(!no_current_score){
            if (!tumGetTextSize((char *)current_score, &current_score_width, NULL)){
                tumDrawText(current_score, SCREEN_WIDTH/2 - (current_score_width/2),
                        DEFAULT_FONT_SIZE * 1.5,
                        Green);
            }
        }
        if (!tumGetTextSize((char *)high_score_sp, &high_score_sp_width, NULL)){
            tumDrawText(high_score_sp, SCREEN_WIDTH - high_score_sp_width - 10,
                    DEFAULT_FONT_SIZE * 1.5,
                    Green);
        }
        if (!tumGetTextSize((char *)high_score_mp, &high_score_mp_width, NULL)){
            tumDrawText(high_score_mp, SCREEN_WIDTH - high_score_mp_width - 10,
                    DEFAULT_FONT_SIZE * 1.5 *2,
                    Green);
        }
        xSemaphoreGive(Score.score_lock);
    }
}