#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>

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
#include <unistd.h>

#include "playState.h"
#include "score.h"
#include "menu.h"

#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

static SemaphoreHandle_t ScreenLock = NULL;
static TaskHandle_t DrawTask = NULL;

void vDrawFPS(void)
{
    static unsigned int periods[FPS_AVERAGE_COUNT] = { 0 };
    static unsigned int periods_total = 0;
    static unsigned int index = 0;
    static unsigned int average_count = 0;
    static TickType_t xLastWakeTime = 0, prevWakeTime = 0;
    static char str[10] = { 0 };
    static int text_width;
    int fps = 0;
    font_handle_t cur_font = tumFontGetCurFontHandle();

    if (average_count < FPS_AVERAGE_COUNT) {
        average_count++;
    }
    else {
        periods_total -= periods[index];
    }

    xLastWakeTime = xTaskGetTickCount();

    if (prevWakeTime != xLastWakeTime) {
        periods[index] =
            configTICK_RATE_HZ / (xLastWakeTime - prevWakeTime);
        prevWakeTime = xLastWakeTime;
    }
    else {
        periods[index] = 0;
    }

    periods_total += periods[index];

    if (index == (FPS_AVERAGE_COUNT - 1)) {
        index = 0;
    }
    else {
        index++;
    }

    fps = periods_total / average_count;

    tumFontSelectFontFromName(FPS_FONT);

    sprintf(str, "FPS: %2d", fps);
    if (!tumGetTextSize((char *)str, &text_width, NULL)){
        tumDrawFilledBox(SCREEN_WIDTH - text_width - 10, SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5, text_width,
                        DEFAULT_FONT_SIZE, Black);
        tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                    SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                    Green);
    }
    tumFontSelectFontFromHandle(cur_font);
    tumFontPutFontHandle(cur_font);
}

void vDrawTask(void *pvParameters)
{
    static TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 20;

    tumDrawBindThread(); // Setup Rendering handle with correct GL context

    while (1) {
        xGetButtonInput(); // Update global input
            // `buttons` is a global shared variable and as such needs to be
            // guarded with a mutex, mutex must be obtained before accessing the
            // resource and given back when you're finished. If the mutex is not
            // given back then no other task can access the reseource.
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumEventFetchEvents(FETCH_EVENT_BLOCK);
            vDrawFPS();
            tumDrawUpdateScreen();
            xSemaphoreGive(ScreenLock);
            vTaskDelayUntil(&xLastWakeTime,
                            pdMS_TO_TICKS(frameratePeriod));
        }
    }
}

void drawMenuState(int current_level){
    static char sp_game_string[40] = "SINGLEPLAYER [C]";
    static char mp_game_string[40] = "MULTIPLAYER [J]";
    static char starting_score_string[60];
    static char infinite_lives_string[30] = "";
    static char starting_level_string[40];
    static int starting_level_width = 0;
    static int sp_string_width = 0;
    static int mp_string_width = 0;
    static int starting_score_width = 0;
    static int infinite_lives_width = 0;
    updateStartScore();
    sprintf(starting_score_string, "STARTING SCORE [UP]/[DOWN]: %d", getCurrentScore());
    if(updateInfiniteLives()){
        strncpy(infinite_lives_string, "INFINITE LIVES: [O]N", sizeof(infinite_lives_string));
    } else {
        strncpy(infinite_lives_string, "INFINITE LIVES: [O]FF", sizeof(infinite_lives_string));
    }
    sprintf(starting_level_string, "STARTING LEVEL [H]IGHER/[L]OWER: %d", current_level+1);
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        if (!tumGetTextSize((char *)sp_game_string, &sp_string_width, NULL)){
            tumDrawText(sp_game_string, SCREEN_WIDTH/2 - sp_string_width/2,
                        SCREEN_HEIGHT*3/5 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        if (!tumGetTextSize((char *)mp_game_string, &mp_string_width, NULL)){
            tumDrawText(mp_game_string, SCREEN_WIDTH/2 - mp_string_width/2,
                        SCREEN_HEIGHT*2/5 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        if (!tumGetTextSize((char *)starting_level_string, &starting_level_width, NULL)){
            tumDrawText(starting_level_string, 10,
                        SCREEN_HEIGHT*17/20 - DEFAULT_FONT_SIZE,
                        Green);
        }
        if (!tumGetTextSize((char *)starting_score_string, &starting_score_width, NULL)){
            tumDrawText(starting_score_string, 10,
                        SCREEN_HEIGHT*18/20 - DEFAULT_FONT_SIZE,
                        Green);
        }
        if (!tumGetTextSize((char *)infinite_lives_string, &infinite_lives_width, NULL)){
            tumDrawText(infinite_lives_string, 10,
                        SCREEN_HEIGHT*19/20 - DEFAULT_FONT_SIZE,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawPause(){
    static char pause_string[30] = "Pause: Press P to continue...";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        DrawLives();
        if (!tumGetTextSize((char *)pause_string, &string_width, NULL)){
            tumDrawText(pause_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawAiNotRunning(){
    static char pause_string[80] = "AI not running: Start AI and then press P to continue...";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        DrawLives();
        if (!tumGetTextSize((char *)pause_string, &string_width, NULL)){
            tumDrawText(pause_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawGameOver(){
    static char new_game_string[20] = "GAME OVER";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
            tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawNextLevel(){
    static char new_game_string[40] = "Congratulations! Press E for next Level";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
            tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawStartMp(){
    static char new_game_string[40] = "STARTING NEW MULTIPLAYER GAME";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
            tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawStartSp(){
    static char new_game_string[40] = "STARTING NEW SINGLEPLAYER GAME";
    static int string_width = 0;
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        tumDrawClear(Black);
        drawScore();
        if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
            tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                        SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                        Green);
        }
        xSemaphoreGive(ScreenLock);
    }
}

void drawPlay(int current_level){
    TickType_t xLastFrameTime = xTaskGetTickCount();
    if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
        DrawInvaders(xLastFrameTime);
        drawScore();
        DrawLives();
        DrawLevel(current_level);
        if(checkDeath()){
            xSemaphoreGive(ScreenLock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            xSemaphoreTake(ScreenLock, portMAX_DELAY);
        }
        DrawPlayerShip();
        xSemaphoreGive(ScreenLock);
    }
}

int initMyDrawing(){
    if (xTaskCreate(vDrawTask, "DrawTask", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-5, &DrawTask) != pdPASS) {
        goto err_draw_task;
    }
    ScreenLock = xSemaphoreCreateMutex();
    if(!ScreenLock){
        goto err_screen_lock;
    }
    return 0;
    err_draw_task:
        vTaskDelete(DrawTask);
    err_screen_lock:
        return 1;
}




