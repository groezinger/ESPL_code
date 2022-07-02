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

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"
#include "buttons.h"
#include <unistd.h>

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)
#define PI 3.141
#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR
#define FPS_AVERAGE_COUNT 50
#define FPS_FONT "IBMPlexSans-Bold.ttf"

#define STATE_QUEUE_LENGTH 1
#define STARTING_STATE STATE_ONE
#define STATE_DEBOUNCE_DELAY 300

typedef struct State{
    TaskHandle_t as_tasks[10];
} State_t;

static TaskHandle_t DrawTask = NULL;
static TaskHandle_t PlayerShip = NULL;
static TaskHandle_t Menu = NULL;
static TaskHandle_t StateMachine = NULL;
static SemaphoreHandle_t ScreenLock = NULL;
static QueueHandle_t StateQueue = NULL;
static State_t MenuState;
static State_t PlayState;

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
                printf("until here it works");
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
                        DEFAULT_FONT_SIZE, White);
        tumDrawText(str, SCREEN_WIDTH - text_width - 10,
                    SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                    Skyblue);
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
        evaluateButtons(); //evaluate Pressed Buttons
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
    image_handle_t invaders_spritesheet_image =
        tumDrawLoadImage("../resources/images/invaders_sheet.png");
    spritesheet_handle_t invaders_spritesheet =
        tumDrawLoadSpritesheet(invaders_spritesheet_image, 8, 2);
    animation_handle_t invader_animation =
       tumDrawAnimationCreate(invaders_spritesheet);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_ONE", 0, 0,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_TWO", 0, 2,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_THREE", 0, 4,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    sequence_handle_t invader_one_sequence =
        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_ONE",
                                            1000);
    sequence_handle_t invader_two_sequence =
        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_TWO",
                                            1000);
    sequence_handle_t invader_three_sequence =
        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_THREE",
                                            1000);     
    TickType_t xLastFrameTime = xTaskGetTickCount();
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(Black);
            tumDrawAnimationDrawFrame(
                    invader_one_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH*1/5, SCREEN_HEIGHT/5);
            tumDrawAnimationDrawFrame(
                    invader_one_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH*2/5, SCREEN_HEIGHT/5);
            tumDrawAnimationDrawFrame(
                    invader_one_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH*3/5, SCREEN_HEIGHT/5);
            tumDrawAnimationDrawFrame(
                    invader_one_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH*4/5, SCREEN_HEIGHT/5);
            tumDrawAnimationDrawFrame(
                    invader_two_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH/2-50, SCREEN_HEIGHT/2);
            tumDrawAnimationDrawFrame(
                    invader_three_sequence,
                    xTaskGetTickCount() - xLastFrameTime,
                    SCREEN_WIDTH/2+50, SCREEN_HEIGHT/2);
            tumDrawSprite(invaders_spritesheet, 5, 1, SCREEN_WIDTH/2, SCREEN_HEIGHT/2+50);
            xSemaphoreGive(ScreenLock);
        }
        xLastFrameTime = xTaskGetTickCount();
        if(getButtonState(KEYBOARD_C)){
            xQueueSend(StateQueue, &MenuState, 0);
        }
        vTaskDelay((TickType_t)20);
    }
}

void vMenu(void *pvParameters){
    while(1){
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawClear(White);
            xSemaphoreGive(ScreenLock);
        }
        if(getButtonState(KEYBOARD_C)){
            xQueueSend(StateQueue, &PlayState, 0);
        }
        vTaskDelay((TickType_t)20);
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
    if (xTaskCreate(vDrawTask, "DrawTask", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-5, &DrawTask) != pdPASS) {
        goto err_draw_task;
    }
    if (xTaskCreate(vPlayerShip, "PlayerShip", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES, &PlayerShip) != pdPASS) {
        goto err_player_ship;
    }
    if (xTaskCreate(vMenu, "Menu", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES, &Menu) != pdPASS) {
        goto err_menu;
    }
    if (xTaskCreate(basicSequentialStateMachine, "StateMachine", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES, &StateMachine) != pdPASS) {
        goto err_sm;
    }

    ScreenLock = xSemaphoreCreateMutex();
    if(!ScreenLock){
        goto err_screen_lock;
    }

    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(State_t));
    if (!StateQueue) {
        goto err_state_queue;
    }
   
    vTaskSuspend(PlayerShip);
    vTaskStartScheduler();

    return EXIT_SUCCESS;

    err_state_queue:
        vSemaphoreDelete(ScreenLock);
    err_screen_lock:
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