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
static my_button_t my_used_buttons[9];

void xGetButtonInput()
{
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

int buttonLockInit(){
    for(int i=0; i< sizeof(my_used_buttons)/ sizeof(my_button_t); i++){
        my_used_buttons[i].last_debounce_time=0;
        my_used_buttons[i].counter=0;
        my_used_buttons[i].button_state=false;
        my_used_buttons[i].last_button_state=false;
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

void evaluateButtons(){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        my_used_buttons[KEYBOARD_A].new_press = debounceButton(&my_used_buttons[KEYBOARD_A], buttons.buttons[KEYCODE(A)]);
        my_used_buttons[KEYBOARD_B].new_press = debounceButton(&my_used_buttons[KEYBOARD_B], buttons.buttons[KEYCODE(B)]);
        my_used_buttons[KEYBOARD_C].new_press = debounceButton(&my_used_buttons[KEYBOARD_C], buttons.buttons[KEYCODE(C)]);
        my_used_buttons[KEYBOARD_D].new_press = debounceButton(&my_used_buttons[KEYBOARD_D], buttons.buttons[KEYCODE(D)]);
        my_used_buttons[KEYBOARD_E].new_press = debounceButton(&my_used_buttons[KEYBOARD_E], buttons.buttons[KEYCODE(E)]);
        my_used_buttons[KEYBOARD_J].new_press = debounceButton(&my_used_buttons[KEYBOARD_J], buttons.buttons[KEYCODE(J)]);
        my_used_buttons[KEYBOARD_K].new_press = debounceButton(&my_used_buttons[KEYBOARD_K], buttons.buttons[KEYCODE(K)]);
        my_used_buttons[KEYBOARD_L].new_press = debounceButton(&my_used_buttons[KEYBOARD_L], buttons.buttons[KEYCODE(L)]);
        if(debounceButton(my_used_buttons+KEYBOARD_Q, buttons.buttons[KEYCODE(Q)])){
            exit(EXIT_SUCCESS);
        }              
    }
    if(debounceButton(&my_used_buttons[MOUSE_LEFT], tumEventGetMouseLeft())
                || debounceButton(&my_used_buttons[MOUSE_MIDDLE], tumEventGetMouseMiddle()) 
                ||  debounceButton(&my_used_buttons[MOUSE_RIGHT], tumEventGetMouseRight())){
        my_used_buttons[KEYBOARD_A].counter = 0;
        my_used_buttons[KEYBOARD_B].counter = 0;
        my_used_buttons[KEYBOARD_C].counter = 0;
        my_used_buttons[KEYBOARD_D].counter = 0;
    }
    xSemaphoreGive(buttons.lock); 
}

int getButtonCounter(MY_CODES code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        int reading = my_used_buttons[code].counter;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

int getButtonState(MY_CODES code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        int reading = my_used_buttons[code].new_press;
        xSemaphoreGive(buttons.lock);
        return reading;
    }
    return 0;
}

void resetButtonCounter(MY_CODES code){
    if (xSemaphoreTake(buttons.lock, portMAX_DELAY) == pdTRUE) {
        my_used_buttons[code].counter = 0;
        xSemaphoreGive(buttons.lock);
    }
}

