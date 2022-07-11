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


/** AsyncIO related */
#define UDP_BUFFER_SIZE 1024
#define UDP_RECEIVE_PORT 1234
#define UDP_TRANSMIT_PORT 1235

static TaskHandle_t UDPControlTask = NULL;
static SemaphoreHandle_t HandleUDP = NULL;
static SemaphoreHandle_t MpLock = NULL;
static SemaphoreHandle_t DownwardSignal = NULL;
static Invaders_t my_invaders;
static PlayerShip_t my_player_ship;
static Invader_t opponent_ship;
static image_handle_t invaders_spritesheet_image = NULL;
static spritesheet_handle_t invaders_spritesheet = NULL;
static spritesheet_handle_t barrier_spritesheet = NULL;
static spritesheet_handle_t opponent_spritesheet = NULL;
static animation_handle_t invader_animation = NULL;
static Score_t Score;
static int sprite_height;
static int sprite_width;
static Barrier_t barriers[4];
static char ch_sc = 0;
static char ch_inf = 0;
static TimerHandle_t InvaderShotTimer = NULL;
static TimerHandle_t InvaderDownwardsTimer = NULL;
static TimerHandle_t InvaderMoveTimer = NULL;



// ------------------------------------------------
aIO_handle_t udp_soc_receive = NULL, udp_soc_transmit = NULL;

typedef enum { NONE = 0, INC = 1, DEC = -1 } opponent_cmd_t;

void UDPHandler(size_t read_size, char *buffer, void *args)
{
    opponent_cmd_t next_key = NONE;
    BaseType_t xHigherPriorityTaskWoken1 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken2 = pdFALSE;
    BaseType_t xHigherPriorityTaskWoken3 = pdFALSE;

    if (xSemaphoreTakeFromISR(HandleUDP, &xHigherPriorityTaskWoken1) ==
        pdTRUE) {

        char send_command = 0;
        if (strncmp(buffer, "INC", (read_size < 3) ? read_size : 3) ==
            0) {
            next_key = INC;
            send_command = 1;
            if (xSemaphoreTake(MpLock, portMAX_DELAY) == pdTRUE){
                opponent_ship.x += 10;
                xSemaphoreGive(MpLock);
            }
        }
        else if (strncmp(buffer, "DEC",
                         (read_size < 3) ? read_size : 3) == 0) {
            next_key = DEC;
            send_command = 1;
            if (xSemaphoreTake(MpLock, portMAX_DELAY) == pdTRUE){
                opponent_ship.x += -10;
                xSemaphoreGive(MpLock);
            }
        }
        else if (strncmp(buffer, "NONE",
                         (read_size < 4) ? read_size : 4) == 0) {
            next_key = NONE;
            send_command = 1;
        }

        /*if (NextKeyQueue && send_command) {
            xQueueSendFromISR(NextKeyQueue, (void *)&next_key,
                              &xHigherPriorityTaskWoken2);
        } */
        xSemaphoreGiveFromISR(HandleUDP, &xHigherPriorityTaskWoken3);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken1 |
                           xHigherPriorityTaskWoken2 |
                           xHigherPriorityTaskWoken3);
    }
    else {
        fprintf(stderr, "[ERROR] Overlapping UDPHandler call\n");
    }
}

void vUDPControlTask(void *pvParameters)
{
    static char buf[50];
    char *addr = NULL; // Loopback
    in_port_t port = UDP_RECEIVE_PORT;
    char last_difficulty = -1;
    char difficulty = 1;

    udp_soc_receive =
        aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE, UDPHandler, NULL);

    printf("UDP socket opened on port %d\n", port);

    while (1) {
        vTaskDelay(20);
        signed int diff = my_player_ship.initial_x - opponent_ship.x;
        if (diff > 0) {
            sprintf(buf, "+%d", diff);
        }
        else {
            sprintf(buf, "-%d", -diff);
        }
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                     strlen(buf));
        if(my_player_ship.shot_active){
            sprintf(buf, "ATTACKING");
        } else {
            sprintf(buf, "PASSIVE");
        }
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                     strlen(buf));
        /*if (last_difficulty != difficulty) {
            sprintf(buf, "D%d", difficulty + 1);
            aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                         strlen(buf));
            last_difficulty = difficulty;
        }*/
    }
}


void initMpMode(){
    HandleUDP = xSemaphoreCreateMutex();
    MpLock = xSemaphoreCreateMutex();
    if (xTaskCreate(vUDPControlTask, "UdpControlTask", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &UDPControlTask) != pdPASS) {
        exit(EXIT_FAILURE); 
    }
    vTaskSuspend(UDPControlTask);
}
// ------------------------------------------------------------------------------

void initiateBarriers(){
    for(int i=0; i<4; i++){
        barriers[i].coords[0][0]=SCREEN_WIDTH*(i+1)/5;
        barriers[i].coords[0][1]=SCREEN_HEIGHT*8/10;
        barriers[i].coords[1][0]=SCREEN_WIDTH*(i+1)/5 + sprite_width;
        barriers[i].coords[1][1]=SCREEN_HEIGHT*8/10;
        barriers[i].coords[2][0]=SCREEN_WIDTH*(i+1)/5;
        barriers[i].coords[2][1]=SCREEN_HEIGHT*8/10 + sprite_height;
        barriers[i].coords[3][0]=SCREEN_WIDTH*(i+1)/5 + sprite_width;
        barriers[i].coords[3][1]=SCREEN_HEIGHT*8/10 + sprite_height;
        for(int j=0; j<4; j++){
            barriers[i].hit_cnt[j]=0;
        }
    }   
}

void initiateOpponent(){
    if (xSemaphoreTake(MpLock, portMAX_DELAY) == pdTRUE){
        opponent_spritesheet =
            tumDrawLoadSpritesheet(invaders_spritesheet_image, 4, 2);
        opponent_ship.x = SCREEN_WIDTH/2;
        opponent_ship.y = SCREEN_HEIGHT/12;
        xSemaphoreGive(MpLock); 
    }
}

void DrawOpponentShip(){
    if (xSemaphoreTake(MpLock, portMAX_DELAY) == pdTRUE){
        tumDrawSprite(opponent_spritesheet, 3, 0, opponent_ship.x, opponent_ship.y);
        xSemaphoreGive(MpLock);
    }
}

int InitiateInvaders(int keep_game_data, int level, char mp_game){
    if(!keep_game_data){
        if(mp_game){
            vTaskResume(UDPControlTask);
        } else {
            vTaskSuspend(UDPControlTask);
        }
        if(!ch_sc){
            Score.current_score = 0;
        }
        if(!ch_inf){
            my_player_ship.lives = 3;
        }
        Score.score_lock = xSemaphoreCreateMutex();
        invaders_spritesheet_image =
            tumDrawLoadImage("../resources/images/invaders_sheet.png");
        tumGetImageSize("../resources/images/invaders_sheet.png", &sprite_width, &sprite_height);
        sprite_height = roundf(sprite_height/4);
        sprite_width = roundf(sprite_width/16);
        invaders_spritesheet =
            tumDrawLoadSpritesheet(invaders_spritesheet_image, 8, 2);
        barrier_spritesheet =
            tumDrawLoadSpritesheet(invaders_spritesheet_image, 16, 4);
        invader_animation = tumDrawAnimationCreate(invaders_spritesheet);
        tumDrawAnimationAddSequence(invader_animation, "INVADER_ZERO", 0, 0,
                                    SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
        tumDrawAnimationAddSequence(invader_animation, "INVADER_ONE", 0, 2,
                                    SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
        tumDrawAnimationAddSequence(invader_animation, "INVADER_TWO", 0, 4,
                                    SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
    }
    if(!Score.high_score){
        Score.high_score = 0;
    }
    if(ch_inf){
        my_player_ship.lives = -1;
    }
    initiateBarriers();
    initiateOpponent();
    my_invaders.shot_x = 0;
    my_invaders.shot_y = 0;
    my_invaders.shot_active = 0;
    my_invaders.alive_cnt=INVADER_COLUMNS*INVADER_ROWS;
    my_invaders.speed = 1.0*(level+1); 
    my_invaders.downward_progress = 0;
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
            my_invaders.invaders[i][j].death_frame_counter=0;
            my_invaders.invaders[i][j].x=TOP_LEFT_INVADER_X + (SCREEN_WIDTH/10)*j;
            my_invaders.invaders[i][j].y=TOP_LEFT_INVADER_Y + (SCREEN_WIDTH/15)*i;
        }
    }

    my_player_ship.initial_x=SCREEN_WIDTH/2;
    my_player_ship.initial_y=SCREEN_HEIGHT*9/10;
    my_player_ship.shot_x = 0;
    my_player_ship.shot_y = 0;
    my_player_ship.shot_active = 0;
    return 0;
}

int DrawInvaders(TickType_t xLastFrameTime){
    static int current_direction = 1;
    static int next_direction = 1;
    static float speed_control = 0.2;
    static int frame_counter = 0;
    tumDrawClear(Black);
    checkBarrierHit();
    for(int i=0; i<INVADER_ROWS; i++){
        for(int j=0; j<INVADER_COLUMNS; j++){
            checkHit(&my_invaders.invaders[i][j]);
            if(my_invaders.invaders[i][j].alive){
                my_invaders.invaders[i][j].x = my_invaders.invaders[i][j].x+current_direction*(int)floor(my_invaders.speed)*speed_control;
                if(xSemaphoreTake(DownwardSignal, 0)==pdTRUE){
                    my_invaders.invaders[i][j].y = my_invaders.invaders[i][j].y + 5;
                } 
                if(my_invaders.invaders[i][j].x < SCREEN_WIDTH/12){
                    next_direction = 1;
                } else if (my_invaders.invaders[i][j].x > SCREEN_WIDTH*11/12){
                    next_direction = -1;
                }
                tumDrawAnimationDrawFrame(
                    my_invaders.invaders[i][j].sequence_handle,
                    xTaskGetTickCount() - xLastFrameTime,
                    my_invaders.invaders[i][j].x,
                    my_invaders.invaders[i][j].y);
            }
            else if(my_invaders.invaders[i][j].death_frame_counter<10){
                tumDrawSprite(invaders_spritesheet, 4, 1, my_invaders.invaders[i][j].x, my_invaders.invaders[i][j].y);
                my_invaders.invaders[i][j].death_frame_counter += 1;
            }
        }
    }
    DrawOpponentShip();
    if(my_invaders.shot_active==1 && my_invaders.shot_y < SCREEN_HEIGHT){
        my_invaders.shot_y=my_invaders.shot_y + 5;
           tumDrawSprite(invaders_spritesheet, 1, 1, my_invaders.shot_x, my_invaders.shot_y);
    } else {
        my_invaders.shot_active = 0;
        my_invaders.shot_y = 0;
    }
    if(frame_counter<2){
        speed_control = 0;
        frame_counter +=1;
    } else {
        speed_control = 1;
        frame_counter = 0;
    }
    current_direction=next_direction;
    return 0;
}

int DrawPlayerShip(){
    if(getContinuousButtonState(KEYCODE(A))){
        my_player_ship.initial_x = my_player_ship.initial_x - 2;
    }
    if(getContinuousButtonState(KEYCODE(D))){
        my_player_ship.initial_x = my_player_ship.initial_x + 2;
    }
    if(getDebouncedMouseState(MY_SCANCODE_MOUSE_LEFT)){
        if(!my_player_ship.shot_active){
            my_player_ship.shot_active = 1;
            my_player_ship.shot_y=my_player_ship.shot_y + my_player_ship.initial_y;
            my_player_ship.shot_x = my_player_ship.initial_x;
            tumSoundPlaySample(shoot); //shoot sound
        }
    }
    if(my_player_ship.shot_active==1 && my_player_ship.shot_y >0){
        my_player_ship.shot_y=my_player_ship.shot_y - 5;
        tumDrawSprite(invaders_spritesheet, 2, 1, my_player_ship.shot_x, my_player_ship.shot_y);
    } else {
        my_player_ship.shot_active = 0;
        my_player_ship.shot_y = 0;
    }
    tumDrawSprite(invaders_spritesheet, 5, 1, my_player_ship.initial_x, my_player_ship.initial_y);
    return 0;
}

int DrawBarricades(){
    for(int i=0; i<4; i++){
        if(barriers[i].hit_cnt[0]!=4){
            tumDrawSprite(barrier_spritesheet, 12 + barriers[i].hit_cnt[0], 2, barriers[i].coords[0][0], barriers[i].coords[0][1]);
        }
        if(barriers[i].hit_cnt[1]!=4){
            tumDrawSprite(barrier_spritesheet, 13 + barriers[i].hit_cnt[1], 2, barriers[i].coords[1][0], barriers[i].coords[1][1]);
        }
        if(barriers[i].hit_cnt[2]!=4){
            tumDrawSprite(barrier_spritesheet, 12 + barriers[i].hit_cnt[2], 3, barriers[i].coords[2][0], barriers[i].coords[2][1]);
        }
        if(barriers[i].hit_cnt[3]!=4){
            tumDrawSprite(barrier_spritesheet, 13 + barriers[i].hit_cnt[3], 3, barriers[i].coords[3][0], barriers[i].coords[3][1]);
        }
    }
    return 0;
}

void checkBarrierHit(){
    for(int i=0; i<4; i++){
    //for every barrier
        for (int j=0; j<4; j++){
            //for every barrier part
            if(barriers[i].hit_cnt[j] != 4){
                if(barriers[i].coords[j][0] >= my_invaders.shot_x &&
                    barriers[i].coords[j][0] - sprite_width <= my_invaders.shot_x &&
                    barriers[i].coords[j][1] + sprite_height/2 >= my_invaders.shot_y &&
                    barriers[i].coords[j][1] - sprite_height/2 <= my_invaders.shot_y){
                        
                    barriers[i].hit_cnt[j] +=2;
                    my_invaders.shot_active=0;
                    my_invaders.shot_y=0;
                }
                if(barriers[i].coords[j][0] >= my_player_ship.shot_x &&
                    barriers[i].coords[j][0] - sprite_width<= my_player_ship.shot_x &&
                    barriers[i].coords[j][1] + (sprite_height/2) >= my_player_ship.shot_y &&
                    barriers[i].coords[j][1] - (sprite_height/2) <= my_player_ship.shot_y){
                    
                    barriers[i].hit_cnt[j] +=2;
                    my_player_ship.shot_active=0;
                    my_player_ship.shot_y=0;
                } 
            }
        }
    }
}

void checkHit(Invader_t *invader){
    if(invader->alive && my_player_ship.shot_active){
        if(my_player_ship.shot_x < invader->x+10 &&
        my_player_ship.shot_x > invader->x-10 &&
        my_player_ship.shot_y < invader->y+10 &&
        my_player_ship.shot_y > invader->y-10){
            invader->alive = 0;
            my_player_ship.shot_active=0;
            my_player_ship.shot_y=0;
            my_invaders.alive_cnt += -1;
            my_invaders.speed += 0.1;
            tumSoundPlaySample(invaderkilled);
            if(my_invaders.alive_cnt == 1){
                my_invaders.speed = 10.0; 
            }
            if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){
                Score.current_score += 20 + abs(invader->type - 2)*10;
                if(Score.current_score>Score.high_score){
                    Score.high_score = Score.current_score;
                }
                xSemaphoreGive(Score.score_lock);
            }
        }
    }
        
}

void createInvaderShot(){
    int random_column;
    Invader_t* shooting_invader = NULL;
    if(!my_invaders.shot_active){
        while(!shooting_invader){
            random_column = rand() % INVADER_COLUMNS;
            for(int i=0; i<INVADER_ROWS; i++){
                if(my_invaders.invaders[i][random_column].alive){
                    shooting_invader = &my_invaders.invaders[i][random_column];
                }
            }
        }
        my_invaders.shot_active = 1;
        my_invaders.shot_x = shooting_invader->x;
        my_invaders.shot_y = shooting_invader->y;
    }
}


int checkDeath(){
    if(my_invaders.shot_active){
        if(my_invaders.shot_x < my_player_ship.initial_x+10 && my_invaders.shot_x > my_player_ship.initial_x-10
        && my_invaders.shot_y < my_player_ship.initial_y+10 && my_invaders.shot_y > my_player_ship.initial_y-10){
            tumDrawBox(my_player_ship.initial_x-40, my_player_ship.initial_y-40, 80, 80, Black);
            tumDrawSprite(invaders_spritesheet, 0, 1, my_player_ship.initial_x, my_player_ship.initial_y);
            my_player_ship.lives = my_player_ship.lives-1;
            if(my_player_ship.lives==0){
                Score.current_score=0;
                ch_inf = 0;
                ch_sc = 0;
            }
            tumSoundPlaySample(explosion);
            my_player_ship.initial_x = SCREEN_WIDTH/2;
            my_player_ship.initial_y = SCREEN_HEIGHT*9/10;
            my_invaders.shot_active=0;
            my_invaders.shot_y=0;
            return 1;
        }
    }
    return 0;
}

void checkGameOver(Invader_t *invader){
    if(invader->y > SCREEN_HEIGHT*8/10){
        my_player_ship.lives = 0;
        Score.current_score = 0;
    }
}
void moveInvadersDown(){
    xSemaphoreGive(DownwardSignal);
}

void drawScore(){
    static char current_score[40] = { 0 };
    static int current_score_width = 0;
    static char high_score[30] = { 0 };
    static int high_score_width = 0;
    if(xSemaphoreTake(Score.score_lock, portMAX_DELAY)==pdTRUE){
        sprintf(current_score, "CURRENT SCORE: %d", Score.current_score);
        sprintf(high_score, "HIGH SCORE: %d", Score.high_score);
        if (!tumGetTextSize((char *)current_score, &current_score_width, NULL)){
            tumDrawText(current_score, current_score_width/2 + 10,
                    DEFAULT_FONT_SIZE * 1.5 + 10,
                    Green);
        }
        if (!tumGetTextSize((char *)high_score, &high_score_width, NULL)){
            tumDrawText(high_score, SCREEN_WIDTH - high_score_width - 10,
                    DEFAULT_FONT_SIZE * 1.5,
                    Green);
        }
        xSemaphoreGive(Score.score_lock);
    }
}

void DrawLives(){
    static char lives_string[30] = { 0 };
    static int lives_width = 0;
    if(my_player_ship.lives >= 0){
        sprintf(lives_string, "LIVES: %d", my_player_ship.lives);
    } else {
        strncpy(lives_string, "LIVES: INFINITE", sizeof(lives_string));
    }
    if (!tumGetTextSize((char *)lives_string, &lives_width, NULL)){
        tumDrawText(lives_string, lives_width + 10,
                    SCREEN_HEIGHT - DEFAULT_FONT_SIZE * 1.5,
                    Green);
    }
}

int getPlayerLives(){
    return my_player_ship.lives;
}

int getAliveInvaders(){
    return my_invaders.alive_cnt;
}

void resetCurrentScore(){
    Score.current_score = 0;
}

int updateStartScore(){
    if(getDebouncedButtonState(KEYCODE(UP))){
        Score.current_score += 10;
        ch_sc = 1;
    } else if(getDebouncedButtonState(KEYCODE(DOWN))){
        if(Score.current_score>9){
            Score.current_score += -10;
        }
    }
    return Score.current_score;
}

int updateInfiniteLives(){
    static int infinite_lives = 0;
    if(getDebouncedButtonState(KEYCODE(O))){
        if(!infinite_lives){
            infinite_lives = 1;
            my_player_ship.lives = -1;
            ch_inf = 1;
        } else {
            infinite_lives = 0;
            ch_inf = 0;
        }
    }
    return infinite_lives;
}

int initiateTimer(){
    DownwardSignal = xSemaphoreCreateMutex();
    InvaderShotTimer = xTimerCreate("InvadershotTimer", pdMS_TO_TICKS(1000), pdTRUE, (void*)0, createInvaderShot);
    InvaderDownwardsTimer = xTimerCreate("InvaderDownwardsTimer", pdMS_TO_TICKS(3000), pdTRUE, (void*)0, moveInvadersDown);
    return 0;
}

void startTimer(){
    xTimerStart(InvaderShotTimer, 0);
    xTimerStart(InvaderDownwardsTimer, 0);
}

void increaseDownwardSpeed(){
    static int ms = 3000;
    ms += -200;
    xTimerChangePeriod(InvaderDownwardsTimer, pdMS_TO_TICKS(ms), 0);
}