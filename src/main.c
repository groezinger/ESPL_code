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
#include "score.h"
#include "drawing.h"
#include "FreeRtosUtility.h"

#define STATE_QUEUE_LENGTH 1
#define STATE_DEBOUNCE_DELAY 100
#define STATE_SWITCH_DELAY 50

typedef struct State{
    TaskHandle_t as_task;
} State_t;
static TaskHandle_t Play = NULL;
static TaskHandle_t Menu = NULL;
static TaskHandle_t InititateNewSpGame = NULL;
static TaskHandle_t InititateNewMpGame = NULL;
static TaskHandle_t GameOver = NULL;
static TaskHandle_t NextLevel = NULL;
static TaskHandle_t StateMachine = NULL;
static TaskHandle_t Pause = NULL;
static TaskHandle_t AiNotRunning = NULL;
static QueueHandle_t StateQueue = NULL;
static State_t MenuState;
static State_t PlayState;
static State_t InititateNewSpGameState;
static State_t InititateNewMpGameState;
static State_t GameOverState;
static State_t NextLevelState;
static State_t PauseState;
static State_t AiNotRunningState;

void changeState(TaskHandle_t* current_state_task, TaskHandle_t* next_state_task)
{
    vTaskSuspend(*current_state_task);
    vTaskDelay(STATE_SWITCH_DELAY);
    vTaskResume(*next_state_task);
}

/*
 * Example basic state machine with sequential states
 */
void basicSequentialStateMachine(void *pvParameters)
{
    MenuState.as_task=Menu;
    PlayState.as_task=Play;
    InititateNewSpGameState.as_task=InititateNewSpGame;
    InititateNewMpGameState.as_task=InititateNewMpGame;
    GameOverState.as_task=GameOver;
    NextLevelState.as_task=NextLevel;
    PauseState.as_task=Pause;
    AiNotRunningState.as_task=AiNotRunning;
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
                    changeState(&current_state.as_task, &next_state.as_task);
                    last_change = xTaskGetTickCount();
                    current_state = next_state;
                }
            }
        }
    }       
}

void vPlay(void *pvParameters){   
    while(1){
        drawPlay();
        if(checkAiRunning() == 0){
            stopTimer();
            xQueueSend(StateQueue, &AiNotRunningState, 0);
            vTaskDelay(200);
        }
        if(getPlayerLives()==0){
            stopTimer();
            pauseMpAI();
            xQueueSend(StateQueue, &GameOverState, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if(getAliveInvaders()==0){
            stopTimer();
            pauseMpAI();
            xQueueSend(StateQueue, &NextLevel, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if(getDebouncedButtonState(KEYCODE(M))){
            resetCurrentScore();
            stopTimer();
            pauseMpAI();
            xQueueSend(StateQueue, &MenuState, 0);
        }
        if(getDebouncedButtonState(KEYCODE(P))){
            stopTimer();
            pauseMpAI();
            xQueueSend(StateQueue, &PauseState, 0);
            vTaskDelay(20);
        }
        vTaskDelay((TickType_t)20);
    }
}

void vMenu(void *pvParameters){
    while(1){
        if(getDebouncedButtonState(KEYCODE(H))){
            //current_level += 1;
            toggleCurrentLevel(1);
            toggleDownwardSpeed(1);
        }
        if(getDebouncedButtonState(KEYCODE(L))){
            //current_level += -1;
            toggleDownwardSpeed(0);
            toggleCurrentLevel(0);
        }
        drawMenuState();     
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
    while(1){
        startTimer();
        initiateInvaders(0, SinglePlayer);
        drawStartSp();
        vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueSend(StateQueue, &PlayState, 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vInititateNewMpGame(void *pvParameters){
    while(1){
        startTimer();
        resumeMpAI();
        setMpDifficulty();
        initiateInvaders(0, Multiplayer);
        drawStartMp();
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(checkAiRunning()){
            xQueueSend(StateQueue, &PlayState, 0);
        } else {
            xQueueSend(StateQueue, &AiNotRunningState, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vGameOver(void *pvParameters){
    while(1){
        drawGameOver();
        vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueSend(StateQueue, &MenuState, 0);
        vTaskDelay(40);
    }
}

void vPause(void *pvParameters){
    while(1){
        drawPause();
        if(getDebouncedButtonState(KEYCODE(P))){
            startTimer();
            resumeMpAI();
            xQueueSend(StateQueue, &PlayState, 0);
        }
        if(getDebouncedButtonState(KEYCODE(M))){
            resetCurrentScore();
            xQueueSend(StateQueue, &MenuState, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void vAiNotRunning(void *pvParameters){
    while(1){
        drawAiNotRunning();
        if(getDebouncedButtonState(KEYCODE(M))){
            resetCurrentScore();
            pauseMpAI();
            xQueueSend(StateQueue, &MenuState, 0);
        }
        if(getDebouncedButtonState(KEYCODE(P)) && checkAiRunning()){
            startTimer();
            setMpDifficulty();
            xQueueSend(StateQueue, &PlayState, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void vNextLevel(void *pvParameters){
    while(1){
        drawNextLevel();
        if(getDebouncedButtonState(KEYCODE(E))){
            //current_level +=1;
            toggleCurrentLevel(1);
            startTimer();
            resumeMpAI();
            toggleDownwardSpeed(1);
            setMpDifficulty();
            initiateInvaders(1, None);
            vTaskDelay(20);
            xQueueSend(StateQueue, &PlayState, 0);
        }
        vTaskDelay(20);
    }
}

int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

    printf("Initializing: ");
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
    if (xTaskCreate(vPlay, "Play", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES, &Play) != pdPASS) {
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
    if (xTaskCreate(vAiNotRunning, "AiNotRunning", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &AiNotRunning) != pdPASS) {
        goto err_ai_not_running;
    }
    if (xTaskCreate(vInititateNewMpGame, "InititateNewMpGame", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &InititateNewMpGame) != pdPASS) {
        goto err_new_mp_game;
    }

    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(State_t));
    if (!StateQueue) {
        goto err_state_queue;
    }
    initGameConfig();
    initMyDrawing();
    initiateTimer();
    initScore(); 
    initMpMode();
    initiateInvaders(0, None);
    vTaskSuspend(NextLevel);
    vTaskSuspend(Play);
    vTaskSuspend(InititateNewSpGame);
    vTaskSuspend(InititateNewMpGame);
    vTaskSuspend(GameOver);
    vTaskSuspend(Pause);
    vTaskSuspend(AiNotRunning);
    vTaskStartScheduler();

    return EXIT_SUCCESS;

    err_state_queue:
        vTaskDelete(AiNotRunning);
    err_ai_not_running:
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
        vTaskDelete(Play);
    err_player_ship:
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