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

static Invaders_t my_invaders;
static PlayerShip_t my_player_ship;
static image_handle_t invaders_spritesheet_image;
static spritesheet_handle_t invaders_spritesheet;
static animation_handle_t invader_animation;
static int shot_y = 0;
static int shot_x = 0;
static int shot_active = 0;

int InitiateInvaders(){
    invaders_spritesheet_image =
        tumDrawLoadScaledImage("../resources/images/invaders_sheet.png", 5);
    invaders_spritesheet =
        tumDrawLoadSpritesheet(invaders_spritesheet_image, 8, 2);
    animation_handle_t invader_animation =
       tumDrawAnimationCreate(invaders_spritesheet);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_ZERO", 0, 0,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_ONE", 0, 2,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    tumDrawAnimationAddSequence(invader_animation, "INVADER_TWO", 0, 4,
                                SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    my_invaders.alive_cnt=40;
    my_invaders.speed = 0; 
    for(int i=0; i<INVADER_ROWS; i++){
        for(int j=0; j<INVADER_COLUMNS; j++){
            if (i==0){
                my_invaders.invaders[i][j].type=0;
                my_invaders.invaders[i][j].sequence_handle =
                    tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_ZERO", 1000);
            } else if (i < 3){
                my_invaders.invaders[i][j].type=1;
                my_invaders.invaders[i][j].sequence_handle =
                    tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_ONE", 1000);
            } else{
                my_invaders.invaders[i][j].type=2;
                my_invaders.invaders[i][j].sequence_handle =
                    tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_TWO", 1000);
            }
            my_invaders.invaders[i][j].alive=1;
            my_invaders.invaders[i][j].initial_x=TOP_LEFT_INVADER_X + (SCREEN_WIDTH/10)*j;
            my_invaders.invaders[i][j].initial_y=TOP_LEFT_INVADER_Y + (SCREEN_WIDTH/15)*i;
        }
    }

    my_player_ship.initial_x=SCREEN_WIDTH/2;
    my_player_ship.initial_y=SCREEN_HEIGHT*9/10;
    return 0;
}

int DrawInvaders(TickType_t xLastFrameTime){
    static float sin_value = 0.0;
    tumDrawClear(Black);
    for(int i=0; i<INVADER_ROWS; i++){
        for(int j=0; j<INVADER_COLUMNS; j++){
            tumDrawAnimationDrawFrame(
                my_invaders.invaders[i][j].sequence_handle,
                xTaskGetTickCount() - xLastFrameTime,
                my_invaders.invaders[i][j].initial_x+10*sin(sin_value),
                my_invaders.invaders[i][j].initial_y);
        }
    }
    sin_value += 0.05 + my_invaders.speed;
    return 0;
}

int DrawPlayerShip(){
    if(getContinuousButtonState(KEYBOARD_A)){
        my_player_ship.initial_x = my_player_ship.initial_x - 2;
    }
    if(getContinuousButtonState(KEYBOARD_D)){
        my_player_ship.initial_x = my_player_ship.initial_x + 2;
    }
    if(getDebouncedButtonState(MOUSE_LEFT)){
        if(!shot_active){
            shot_active = 1;
            shot_y=shot_y + my_player_ship.initial_y;
            shot_x = my_player_ship.initial_x;
        }
    }
    if(shot_active==1 && shot_y >0){
        shot_y=shot_y - 5;
        tumDrawSprite(invaders_spritesheet, 2, 1, shot_x, shot_y);
    } else {
        shot_active = 0;
        shot_y = 0;
    }

    tumDrawSprite(invaders_spritesheet, 5, 1, my_player_ship.initial_x, my_player_ship.initial_y);
    return 0;
}

int DrawBarricades(){
    tumDrawSprite(invaders_spritesheet, 6, 1, SCREEN_WIDTH/5, SCREEN_HEIGHT*8/10);
    tumDrawSprite(invaders_spritesheet, 6, 1, SCREEN_WIDTH*2/5, SCREEN_HEIGHT*8/10);
    tumDrawSprite(invaders_spritesheet, 6, 1, SCREEN_WIDTH*3/5, SCREEN_HEIGHT*8/10);
    tumDrawSprite(invaders_spritesheet, 6, 1, SCREEN_WIDTH*4/5, SCREEN_HEIGHT*8/10);
    return 0;
}