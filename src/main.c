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
#include "playState.h"
#include "FreeRtosUtility.h"

#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

#define STATE_QUEUE_LENGTH 1
#define STARTING_STATE STATE_ONE
#define STATE_DEBOUNCE_DELAY 100

typedef struct State{
    TaskHandle_t as_tasks[10];
} State_t;

static TaskHandle_t DrawTask = NULL;
static TaskHandle_t PlayerShip = NULL;
static TaskHandle_t Menu = NULL;
static TaskHandle_t InititateNewSpGame = NULL;
static TaskHandle_t InititateNewMpGame = NULL;
static TaskHandle_t GameOver = NULL;
static TaskHandle_t NextLevel = NULL;
static TaskHandle_t StateMachine = NULL;
static TaskHandle_t Pause = NULL;
static SemaphoreHandle_t ScreenLock = NULL;
static QueueHandle_t StateQueue = NULL;
static State_t MenuState;
static State_t PlayState;
static State_t InititateNewSpGameState;
static State_t InititateNewMpGameState;
static State_t GameOverState;
static State_t NextLevelState;
static State_t PauseState;
static int current_level = 0;

void changeState(volatile TaskHandle_t* current_state_tasks, TaskHandle_t* next_state_tasks)
{
    for(int i=0; i<1; i++){
        vTaskSuspend(*(current_state_tasks + i));
    }
    for(int j=0; j<1; j++){
        vTaskResume(*(next_state_tasks + j));
    }
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    MenuState.as_tasks[0]=Menu;
    PlayState.as_tasks[0]=PlayerShip;
    InititateNewSpGameState.as_tasks[0]=InititateNewSpGame;
    InititateNewMpGameState.as_tasks[0]=InititateNewMpGame;
    GameOverState.as_tasks[0]=GameOver;
    NextLevelState.as_tasks[0]=NextLevel;
    PauseState.as_tasks[0]=Pause;
    State_t current_state; 
    current_state = MenuState; // Default state
    State_t next_state;
    next_state = MenuState;

    int state_change_period = STATE_DEBOUNCE_DELAY;

    TickType_t last_change = xTaskGetTickCount();

    while (1) {
        // Handle state machine input
        if (StateQueue){
            if (xQueueReceive(StateQueue, &next_state, portMAX_DELAY) == pdTRUE) {
                if (xTaskGetTickCount() - last_change > state_change_period) {
                    changeState(current_state.as_tasks, next_state.as_tasks);
                    last_change = xTaskGetTickCount();
                    current_state = next_state;
                }
            }
        }
    }       
}


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

void vPlayerShip(void *pvParameters){   
    TickType_t xLastFrameTime = xTaskGetTickCount();
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            DrawInvaders(xLastFrameTime);
            DrawBarricades();
            drawScore();
            DrawLives();
            DrawLevel(current_level);
            if(checkDeath()){
                xSemaphoreGive(ScreenLock);
                vTaskDelay(pdMS_TO_TICKS(1000));
                xSemaphoreTake(ScreenLock, portMAX_DELAY);
            }
            if(getPlayerLives()==0){
                stopTimer();
                xSemaphoreGive(ScreenLock);
                xQueueSend(StateQueue, &GameOverState, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            DrawPlayerShip();
            xSemaphoreGive(ScreenLock);
        }
        xLastFrameTime = xTaskGetTickCount();
        if(getAliveInvaders()==0){
            current_level +=1;
            xQueueSend(StateQueue, &NextLevel, 0);
        }
        if(getDebouncedButtonState(KEYCODE(C))){
            resetCurrentScore();
            stopTimer();
            xQueueSend(StateQueue, &MenuState, 0);
        }
        if(getDebouncedButtonState(KEYCODE(P))){
            resetCurrentScore();
            stopTimer();
            xQueueSend(StateQueue, &PauseState, 0);
            vTaskDelay(20);
        }
        vTaskDelay((TickType_t)20);
    }
}

void vMenu(void *pvParameters){
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
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            sprintf(starting_score_string, "STARTING SCORE [UP]/[DOWN]: %d", updateStartScore());
            if(updateInfiniteLives()){
                strncpy(infinite_lives_string, "INFINITE LIVES: [O]N", sizeof(infinite_lives_string));
            } else {
                strncpy(infinite_lives_string, "INFINITE LIVES: [O]FF", sizeof(infinite_lives_string));
            }
            if(getDebouncedButtonState(KEYCODE(H))){
                current_level += 1;
                toggleDownwardSpeed(1);
            }
            if(getDebouncedButtonState(KEYCODE(L)) && current_level>0){
                current_level += -1;
                toggleDownwardSpeed(0);
            }
            sprintf(starting_level_string, "STARTING LEVEL [H]IGHER/[L]OWER: %d", current_level+1);
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
        if(getDebouncedButtonState(KEYCODE(C))){
            xQueueSend(StateQueue, &InititateNewSpGameState, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if(getDebouncedButtonState(KEYCODE(J))){
            xQueueSend(StateQueue, &InititateNewMpGameState, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vTaskDelay((TickType_t)20);
    }
}

void vInititateNewSpGame(void *pvParameters){
    static char new_game_string[40] = "STARTING NEW SINGLEPLAYER GAME";
    static int string_width = 0;
    while(1){
        startTimer(current_level);
        InitiateInvaders(0, current_level, 0);
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(Black);
            drawScore();
            if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
                tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                    SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                    Green);
            }
        xSemaphoreGive(ScreenLock);
        vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueSend(StateQueue, &PlayState, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void vInititateNewMpGame(void *pvParameters){
    static char new_game_string[40] = "STARTING NEW MULTIPLAYER GAME";
    static int string_width = 0;
    system("../opponents/space_invaders_opponent &>/dev/null &");
    while(1){
        startTimer(current_level);
        InitiateInvaders(0, current_level, '1');
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(Black);
            drawScore();
            if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
                tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                    SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                    Green);
            }
        xSemaphoreGive(ScreenLock);
        vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueSend(StateQueue, &PlayState, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void vGameOver(void *pvParameters){
    static char new_game_string[20] = "GAME OVER";
    static int string_width = 0;
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(Black);
            drawScore();
            if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
                tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                    SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                    Green);
            }
        xSemaphoreGive(ScreenLock);
        vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueSend(StateQueue, &MenuState, 0);
        vTaskDelay(40);
        }
    }
}

void vPause(void *pvParameters){
    static char pause_string[30] = "Pause: Press P to continue...";
    static int string_width = 0;
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(Black);
            drawScore();
            DrawLevel(current_level);
            DrawLives();
            if (!tumGetTextSize((char *)pause_string, &string_width, NULL)){
                tumDrawText(pause_string, SCREEN_WIDTH/2 - string_width/2,
                    SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                    Green);
            }
        }
        xSemaphoreGive(ScreenLock);
        if(getDebouncedButtonState(KEYCODE(P))){
            startTimer(current_level);
            xQueueSend(StateQueue, &PlayState, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void vNextLevel(void *pvParameters){
    static char new_game_string[40] = "Congratulations! Press E for next Level";
    static int string_width = 0;
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            toggleDownwardSpeed(1);
            tumDrawClear(Black);
            drawScore();
            if (!tumGetTextSize((char *)new_game_string, &string_width, NULL)){
                tumDrawText(new_game_string, SCREEN_WIDTH/2 - string_width/2,
                    SCREEN_HEIGHT/2 - DEFAULT_FONT_SIZE/2,
                    Green);
            }
            xSemaphoreGive(ScreenLock);
            if(getDebouncedButtonState(KEYCODE(E))){
                InitiateInvaders(1, current_level, 0);
                xQueueSend(StateQueue, &PlayState, 0);
            }
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    printf("Initializing: ");
    initiateTimer();
    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize drawing");
        goto err_init_drawing;
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }

    if (tumSoundInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize audio");
        goto err_init_audio;
    }

    if (!buttonLockInit()) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }
    if (xTaskCreate(vDrawTask, "DrawTask", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-5, &DrawTask) != pdPASS) {
        goto err_draw_task;
    }
    if (xTaskCreate(vPlayerShip, "PlayerShip", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &PlayerShip) != pdPASS) {
        goto err_player_ship;
    }
    if (xTaskCreate(vMenu, "Menu", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &Menu) != pdPASS) {
        goto err_menu;
    }
    if (xTaskCreate(basicSequentialStateMachine, "StateMachine", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES, &StateMachine) != pdPASS) {
        goto err_sm;
    }
    if (xTaskCreate(vInititateNewSpGame, "InititateNewSpGame", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &InititateNewSpGame) != pdPASS) {
        goto err_new_sp_game;
    }
    if (xTaskCreate(vGameOver, "GameOver", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &GameOver) != pdPASS) {
        goto err_game_over;
    }
    if (xTaskCreate(vNextLevel, "NextLevel", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &NextLevel) != pdPASS) {
        goto err_next_level;
    }
    if (xTaskCreate(vPause, "Pause", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &Pause) != pdPASS) {
        goto err_pause;
    }
    if (xTaskCreate(vInititateNewMpGame, "InititateNewMpGame", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &InititateNewMpGame) != pdPASS) {
        goto err_new_mp_game;
    }

    ScreenLock = xSemaphoreCreateMutex();
    if(!ScreenLock){
        goto err_screen_lock;
    }

    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(State_t));
    if (!StateQueue) {
        goto err_state_queue;
    }
    initMpMode();
    InitiateInvaders(0, 0, 0);
    vTaskSuspend(NextLevel);
    vTaskSuspend(PlayerShip);
    vTaskSuspend(InititateNewSpGame);
    vTaskSuspend(InititateNewMpGame);
    vTaskSuspend(GameOver);
    vTaskSuspend(Pause);
    vTaskStartScheduler();

    return EXIT_SUCCESS;

    err_state_queue:
        vSemaphoreDelete(ScreenLock);
    err_screen_lock:
        vTaskDelete(Pause);
    err_pause:
        vTaskDelete(InititateNewMpGame);
    err_new_mp_game:
        vTaskDelete(NextLevel);
    err_next_level:
        vTaskDelete(GameOver);
    err_game_over:
        vTaskDelete(InititateNewSpGame);
    err_new_sp_game:
        vTaskDelete(StateMachine);
    err_sm:
        vTaskDelete(Menu);
    err_menu:
        vTaskDelete(PlayerShip);
    err_player_ship:
        vTaskDelete(DrawTask);
    err_draw_task:
        buttonLockExit();
    err_buttons_lock:
        tumSoundExit();
    err_init_audio:
        tumEventExit();
    err_init_events:
        tumDrawExit();
    err_init_drawing:
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}