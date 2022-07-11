#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"
#include "buttons.h"

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput()
{
    //static int get_input[SDL_NUM_SCANCODES];
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.input, 0);
        if(debounceButton(buttons.buttons+KEYCODE(Q), buttons.input[KEYCODE(Q)])){
            exit(EXIT_SUCCESS);
        }  
        xSemaphoreGive(buttons.lock);
    }
}

int buttonLockInit(){
    for(int i=0; i< sizeof(buttons.buttons)/ sizeof(my_button_t); i++){
        buttons.buttons[i].last_debounce_time=0;
        buttons.buttons[i].counter=0;
        buttons.buttons[i].button_state=false;
        buttons.buttons[i].last_button_state=false;
    }
    buttons.lock = xSemaphoreCreateMutex();
    xSemaphoreGive(buttons.lock); // Locking mechanism
    if (!buttons.lock) {
        return 0;
    }
    else {
        return 1;
    }
}

void buttonLockExit(){
    vSemaphoreDelete(buttons.lock);
}

bool debounceButton(my_button_t* my_button, int reading){
    bool return_value = false;
    if (reading != my_button->last_button_state){
        my_button->last_debounce_time = xTaskGetTickCount();
    }
    if((xTaskGetTickCount() - my_button->last_debounce_time)>50){
        if(reading != my_button->button_state){
            my_button->button_state = reading;
            if(my_button->button_state){
                return_value = true;
                my_button->counter +=1;
            }
        }
    }
    my_button->last_button_state = reading;
    return return_value;
}

int getButtonCounter(SDL_Scancode code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        int reading = buttons.buttons[code].counter;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

int getDebouncedButtonState(SDL_Scancode code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        buttons.buttons[code].new_press = debounceButton(&buttons.buttons[code], buttons.input[code]);
        int reading = buttons.buttons[code].new_press;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

int getContinuousButtonState(SDL_Scancode code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        buttons.buttons[code].new_press = debounceButton(&buttons.buttons[code], buttons.input[code]);
        int reading = buttons.buttons[code].button_state;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

int getDebouncedMouseState(MY_SCANCODE_MOUSE code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        switch (code)
        {
        case 1:
            buttons.buttons[code].new_press = debounceButton(&buttons.buttons[code], tumEventGetMouseLeft());
            break;
        case 2:
            buttons.buttons[code].new_press = debounceButton(&buttons.buttons[code], tumEventGetMouseRight());
            break;
        case 3:
            buttons.buttons[code].new_press = debounceButton(&buttons.buttons[code], tumEventGetMouseMiddle());
            break;
        default:
            break;
        }
        int reading = buttons.buttons[code].new_press;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

void resetButtonCounter(SDL_Scancode code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        buttons.buttons[code].counter = 0;
        xSemaphoreGive(buttons.lock);
    }
}

