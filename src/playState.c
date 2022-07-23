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


/** AsyncIO related */
#define UDP_BUFFER_SIZE 1024
#define UDP_RECEIVE_PORT 1234
#define UDP_TRANSMIT_PORT 1235

#define DEFAULT_RAND 10
#define INVADER_ROWS 5
#define INVADER_COLUMNS 8
#define TOP_LEFT_INVADER_X SCREEN_WIDTH/(INVADER_COLUMNS+1)
#define TOP_LEFT_INVADER_Y SCREEN_HEIGHT/4
#define OPPONENT_SHIP_HEIGHT SCREEN_HEIGHT/6
#define AI_MAX_RESPONSE_TIME 5000
#define LIFE_INCREASE_SCORE 700
#define DEFAULT_PLAYER_LIVES 3
#define BOTTOM_LINE_HEIGHT SCREEN_HEIGHT*19/20
#define DEFAULT_INVADER_EXPLOSION_FRAMES 10
#define DEFAULT_ANIMATION_PERIOD 1000

typedef struct Game{
    game_mode_t mode;
    char infinite_lives;
    char increased_start_score;
    int level;
    int last_received_ai_response;
    SemaphoreHandle_t game_lock;
} Game_t;

typedef struct Invader{
    char type;
    char alive;
    int x;
    int y;
    int death_frame_counter;
    int max_appear;
    int appear_counter;
    sequence_handle_t sequence_handle;
} Invader_t;

typedef struct PlayerShip{
    int lives;
    int x;
    int y;
    int shot_y;
    int shot_x;
    int shot_active;
    int hitbox_height;
    int hitbox_width;
    int shot_hitbox_alignment;
} PlayerShip_t;

typedef struct Invaders{
    int alive_cnt;
    Invader_t invaders[INVADER_ROWS][INVADER_COLUMNS];
    float speed;
    coord_t shots[3];
    int shot_x;
    int shot_y;
    int shot_hitbox_alignment;
    int shot_active[3];
    int hitbox_height;
    int hitbox_width;
} Invaders_t;

typedef struct Barrier{
    int coords[4][2];
    int hit_cnt[4];
    int hitbox_height;
    int hitbox_width;
} Barrier_t;

//FreeRtos and TumDraw related
static TaskHandle_t UDPControlTask = NULL;
static SemaphoreHandle_t HandleUDP = NULL;;
static QueueHandle_t NextKeyQueue = NULL;
static SemaphoreHandle_t DownwardSignal = NULL;
static image_handle_t invaders_spritesheet_image = NULL;
static image_handle_t opponent_explosion_image = NULL;
static spritesheet_handle_t invaders_spritesheet = NULL;
static spritesheet_handle_t barrier_spritesheet = NULL;
static spritesheet_handle_t opponent_spritesheet = NULL;
static animation_handle_t invader_animation = NULL;
static TimerHandle_t InvaderShotTimerOne = NULL;
static TimerHandle_t InvaderShotTimerTwo = NULL;
static TimerHandle_t InvaderShotTimerThree = NULL;
static TimerHandle_t InvaderDownwardsTimer = NULL;
static TimerHandle_t OpponentShipSpTimer = NULL;

//aIo related
aIO_handle_t udp_soc_receive = NULL, udp_soc_transmit = NULL;
typedef enum { NONE = 0, INC = 1, DEC = -1 } opponent_cmd_t;

//game entities
static Game_t game_config;
//all below global variables are protected by game_config.game_lock
//most functions use at least two variables 
//therefore invidual mutexes would make it unncesseray complicated
static Invaders_t my_invaders;
static PlayerShip_t my_player_ship;
static Invader_t opponent_ship;
static Barrier_t barriers[4];



// ------------------------------------------------

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
            game_config.last_received_ai_response = xTaskGetTickCount();
        }
        else if (strncmp(buffer, "DEC",
                         (read_size < 3) ? read_size : 3) == 0) {
            next_key = DEC;
            send_command = 1;
            game_config.last_received_ai_response = xTaskGetTickCount();
        }
        else if (strncmp(buffer, "NONE",
                         (read_size < 4) ? read_size : 4) == 0) {
            next_key = NONE;
            send_command = 1;
            game_config.last_received_ai_response = xTaskGetTickCount();
        }

        if (NextKeyQueue && send_command) {
            xQueueSendFromISR(NextKeyQueue, (void *)&next_key,
                              &xHigherPriorityTaskWoken2);
        } 
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
    static char last_shot_state = 0;
    static unsigned int last_distance = 0;
    char *addr = NULL; // Loopback
    in_port_t port = UDP_RECEIVE_PORT;

    udp_soc_receive =
        aIOOpenUDPSocket(addr, port, UDP_BUFFER_SIZE, UDPHandler, NULL);

    printf("UDP socket opened on port %d\n", port);

    while (1) {
        vTaskDelay(20);
        if (xSemaphoreTake(game_config.game_lock, portMAX_DELAY) == pdTRUE){
            signed int diff = my_player_ship.x - opponent_ship.x;
            if (my_player_ship.x > opponent_ship.x) {
                sprintf(buf, "+%d", diff);
            }
            else {
                sprintf(buf, "-%d", -diff);
            }
            if(last_distance != diff){
                aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                        strlen(buf));
                last_distance = diff;
            }
            if(my_player_ship.shot_active){
                sprintf(buf, "ATTACKING");
            } else {
                sprintf(buf, "PASSIVE"); //tbd lock here
            }

            if(my_player_ship.shot_active != last_shot_state){
                aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                            strlen(buf));
                last_shot_state = my_player_ship.shot_active;
            }
        }
        xSemaphoreGive(game_config.game_lock);
    }
}

char checkAiRunning(){
    int return_value = 0;
    if (xSemaphoreTake(game_config.game_lock, portMAX_DELAY) == pdTRUE){
        if(game_config.mode==Multiplayer){
            for(int i=0; i<10; i++){
                if((xTaskGetTickCount()-game_config.last_received_ai_response) < AI_MAX_RESPONSE_TIME){
                    return_value = 1;
                }
                //send value to AI to make it respond if running
                aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "+0",
                            strlen("+0"));       
                vTaskDelay(100);
            }
        } else {
            return_value = 1;
        }
    }
    xSemaphoreGive(game_config.game_lock);
    return return_value;
}

void setMpDifficulty(){
    static char buf[50];
    static int mp_difficulty = 1;
    if (xSemaphoreTake(game_config.game_lock, portMAX_DELAY) == pdTRUE){
        if(game_config.level>=2){
            //max difficulty is 3
            mp_difficulty= 3;
        } else {
            mp_difficulty = game_config.level+1;
        }
        sprintf(buf, "D%d", mp_difficulty);
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, buf,
                            strlen(buf));
    }
    xSemaphoreGive(game_config.game_lock);
}

void pauseMpAI(){
    if (xSemaphoreTake(game_config.game_lock, portMAX_DELAY) == pdTRUE){
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "PAUSE",
                            strlen("PAUSE"));
    }
    xSemaphoreGive(game_config.game_lock);
}

void resumeMpAI(){
    if (xSemaphoreTake(game_config.game_lock, portMAX_DELAY) == pdTRUE){
        aIOSocketPut(UDP, NULL, UDP_TRANSMIT_PORT, "RESUME",
                            strlen("RESUME"));
    }
    xSemaphoreGive(game_config.game_lock);
}



void initMpMode(){
    HandleUDP = xSemaphoreCreateMutex();
    NextKeyQueue = xQueueCreate(1, sizeof(opponent_cmd_t));
    if (xTaskCreate(vUDPControlTask, "UdpControlTask", mainGENERIC_STACK_SIZE*2, NULL,
                    configMAX_PRIORITIES-1, &UDPControlTask) != pdPASS) {
        exit(EXIT_FAILURE); 
    }
    vTaskSuspend(UDPControlTask);
}
// ------------------------------------------------------------------------------
void initGameConfig(){
    game_config.increased_start_score = 0;
    game_config.infinite_lives = 0;
    game_config.last_received_ai_response = -6000; //last received AI Response Start Value
    game_config.mode = None;
    game_config.level = 0;
    game_config.game_lock = xSemaphoreCreateMutex();
}


void initiateBarriers(){
    for(int i=0; i<4; i++){
        tumGetImageSize("../resources/images/invaders_sheet.png", &barriers[i].hitbox_width, &barriers[i].hitbox_height);
        barriers[i].hitbox_height = roundf(barriers[i].hitbox_height/4);
        barriers[i].hitbox_width = roundf(barriers[i].hitbox_width/16);
        barriers[i].coords[0][0]=SCREEN_WIDTH*(i+1)/5;
        barriers[i].coords[0][1]=SCREEN_HEIGHT*8/10;
        barriers[i].coords[1][0]=SCREEN_WIDTH*(i+1)/5 + barriers[i].hitbox_width;
        barriers[i].coords[1][1]=SCREEN_HEIGHT*8/10;
        barriers[i].coords[2][0]=SCREEN_WIDTH*(i+1)/5;
        barriers[i].coords[2][1]=SCREEN_HEIGHT*8/10 + barriers[i].hitbox_height;
        barriers[i].coords[3][0]=SCREEN_WIDTH*(i+1)/5 + barriers[i].hitbox_width;
        barriers[i].coords[3][1]=SCREEN_HEIGHT*8/10 + barriers[i].hitbox_height;
        for(int j=0; j<4; j++){
            barriers[i].hit_cnt[j]=0;
        }
    }   
}

void initiateOpponent(mode_t mode, int level){
    opponent_explosion_image = tumDrawLoadScaledImage("../resources/images/mothership_explosion.png", 0.15);
    opponent_spritesheet =
            tumDrawLoadSpritesheet(invaders_spritesheet_image, 4, 2);
    if(mode==Multiplayer){
        opponent_ship.x = SCREEN_WIDTH/2;
        xTimerStop(OpponentShipSpTimer, 0);
    } else{
        opponent_ship.x = -100;
    }
    opponent_ship.alive = 1;
    opponent_ship.y = OPPONENT_SHIP_HEIGHT;
    opponent_ship.max_appear = level + 1;
    opponent_ship.appear_counter = 0;
    opponent_ship.death_frame_counter = 0;
    opponent_ship.type = 3;
}

void lifeIncrease(){
    static int counter=1;
    if(getCurrentScore()>LIFE_INCREASE_SCORE*counter){
        counter += 1;
        my_player_ship.lives += 1;
    }
}

void checkOpponentHit(){
    if(opponent_ship.alive && my_player_ship.shot_active){
        if(my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment < opponent_ship.x+my_invaders.hitbox_width*2 &&
        my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment > opponent_ship.x &&
        my_player_ship.shot_y + my_player_ship.shot_hitbox_alignment< opponent_ship.y+ my_invaders.hitbox_width &&
        my_player_ship.shot_y + my_player_ship.shot_hitbox_alignment > opponent_ship.y){
            opponent_ship.alive = 0;
            opponent_ship.appear_counter = opponent_ship.max_appear;
            my_player_ship.shot_active=0;
            my_player_ship.shot_y=0;
            tumSoundPlaySample(explosion);
            lifeIncrease();
            increaseScore(100, game_config.mode);
        }
    }
}

void DrawOpponentShip(){
    static opponent_cmd_t current_key = NONE;
    if (NextKeyQueue) {
        xQueueReceive(NextKeyQueue, &current_key, 0);
    }
    checkOpponentHit();
    if(opponent_ship.alive && opponent_ship.x > -20){
        tumDrawSprite(opponent_spritesheet, 3, 0, opponent_ship.x, opponent_ship.y);
        if(game_config.mode==SinglePlayer){
            opponent_ship.x = opponent_ship.x - 2;
        } else {
            if(current_key == DEC && opponent_ship.x>2){
                opponent_ship.x = opponent_ship.x - 2;
            } else if(current_key == INC && opponent_ship.x<(SCREEN_WIDTH-70)){
                opponent_ship.x = opponent_ship.x + 2;
            }
        }
    } 
    else if(opponent_ship.death_frame_counter<30 && opponent_ship.x > -20){
        //coordinate alignment because Image is not exact same size as sprite
        tumDrawLoadedImage(opponent_explosion_image , opponent_ship.x+5, opponent_ship.y+10);
        opponent_ship.death_frame_counter += 1;
    }
}

int initiateInvaders(int keep_game_data, game_mode_t mode){
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        if(!keep_game_data){
            game_config.mode = mode;
            if(mode==Multiplayer){
                vTaskResume(UDPControlTask);
            } else{
                vTaskSuspend(UDPControlTask);
            }
            if(!game_config.increased_start_score){
                resetCurrentScore();
            }
            if(!game_config.infinite_lives){
                my_player_ship.lives = DEFAULT_PLAYER_LIVES;
            }
            invaders_spritesheet_image = tumDrawLoadImage("../resources/images/invaders_sheet.png");
            tumGetImageSize("../resources/images/invaders_sheet.png", &my_player_ship.hitbox_width, &my_player_ship.hitbox_height);
            my_player_ship.hitbox_width = my_player_ship.hitbox_width/8;
            my_player_ship.hitbox_height = my_player_ship.hitbox_height/2;
            my_player_ship.shot_hitbox_alignment = my_player_ship.hitbox_width/2; //to align center of shot for hit checks
            my_invaders.hitbox_width = my_player_ship.hitbox_width; //same spritesheet results in same hitbox values
            my_invaders.hitbox_height = my_player_ship.hitbox_height;
            my_invaders.shot_hitbox_alignment = my_player_ship.shot_hitbox_alignment;
            invaders_spritesheet = tumDrawLoadSpritesheet(invaders_spritesheet_image, 8, 2);
            barrier_spritesheet = tumDrawLoadSpritesheet(invaders_spritesheet_image, 16, 4);
            invader_animation = tumDrawAnimationCreate(invaders_spritesheet);
            tumDrawAnimationAddSequence(invader_animation, "INVADER_ZERO", 0, 0,
                                        SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
            tumDrawAnimationAddSequence(invader_animation, "INVADER_ONE", 0, 2,
                                        SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
            tumDrawAnimationAddSequence(invader_animation, "INVADER_TWO", 0, 4,
                                        SPRITE_SEQUENCE_HORIZONTAL_POS, 2);
        }

        initiateOpponent(game_config.mode, game_config.level);
        if(game_config.infinite_lives){
            my_player_ship.lives = -1; //setting lives to -1 to indicate infinite lives
        }
        initiateBarriers();
        my_invaders.shots[0].x = 0;
        my_invaders.shots[0].y = 0;
        my_invaders.shots[1].x = 0;
        my_invaders.shots[1].y = 0;
        my_invaders.shots[2].x = 0;
        my_invaders.shots[2].y = 0;
        my_invaders.shot_active[0] = 0;
        my_invaders.shot_active[1] = 0;
        my_invaders.shot_active[2] = 0;
        my_invaders.alive_cnt=INVADER_COLUMNS*INVADER_ROWS;
        my_invaders.speed = 1.0*(game_config.level+1); 
        for(int i=0; i<INVADER_ROWS; i++){
            for(int j=0; j<INVADER_COLUMNS; j++){
                if (i==0){
                    my_invaders.invaders[i][j].type=0;
                    my_invaders.invaders[i][j].sequence_handle =
                        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_ZERO", DEFAULT_ANIMATION_PERIOD);
                } else if (i < 3){
                    my_invaders.invaders[i][j].type=1;
                    my_invaders.invaders[i][j].sequence_handle =
                        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_ONE", DEFAULT_ANIMATION_PERIOD);
                } else{
                    my_invaders.invaders[i][j].type=2;
                    my_invaders.invaders[i][j].sequence_handle =
                        tumDrawAnimationSequenceInstantiate(invader_animation, "INVADER_TWO", DEFAULT_ANIMATION_PERIOD);
                }
                my_invaders.invaders[i][j].alive=1;
                my_invaders.invaders[i][j].death_frame_counter=0;
                my_invaders.invaders[i][j].x=TOP_LEFT_INVADER_X + (SCREEN_WIDTH/10)*j;
                my_invaders.invaders[i][j].y=TOP_LEFT_INVADER_Y + (SCREEN_HEIGHT/11)*i;
            }
        }
        my_player_ship.x=SCREEN_WIDTH/2;
        my_player_ship.y=SCREEN_HEIGHT*7/8;
        my_player_ship.shot_x = 0;
        my_player_ship.shot_y = 0;
        my_player_ship.shot_active = 0;
    }
    xSemaphoreGive(game_config.game_lock);
    return 0;
}

int checkDeath(){
    for(int m=0; m<3; m++){
        if(my_invaders.shot_active[m]){
            if(my_invaders.shots[m].x + my_invaders.shot_hitbox_alignment < my_player_ship.x+my_player_ship.hitbox_width &&
            my_invaders.shots[m].x+my_invaders.shot_hitbox_alignment > my_player_ship.x &&
            my_invaders.shots[m].y+my_invaders.shot_hitbox_alignment < my_player_ship.y+my_player_ship.hitbox_height &&
            my_invaders.shots[m].y+my_invaders.shot_hitbox_alignment > my_player_ship.y){

                tumDrawSprite(invaders_spritesheet, 0, 1, my_player_ship.x, my_player_ship.y);
                my_player_ship.lives = my_player_ship.lives-1;
                tumSoundPlaySample(explosion);
                my_player_ship.x = SCREEN_WIDTH/2;
                my_invaders.shot_active[m]=0;
                my_invaders.shots[m].y=0;
                if(my_player_ship.lives==0){
                    resetCurrentScore();
                    game_config.infinite_lives = 0;
                    game_config.increased_start_score = 0;
                }
                return 1;
            }
        }
    }
    return 0;
}

int DrawPlayerShip(){
    if(!checkDeath()){
        if(getContinuousButtonState(KEYCODE(A))){
            my_player_ship.x = my_player_ship.x - 2;
        }
        if(getContinuousButtonState(KEYCODE(D))){
            my_player_ship.x = my_player_ship.x + 2;
        }
        if(getDebouncedButtonState(KEYCODE(SPACE))){
            if(!my_player_ship.shot_active){
                my_player_ship.shot_active = 1;
                my_player_ship.shot_y=my_player_ship.shot_y + my_player_ship.y;
                my_player_ship.shot_x = my_player_ship.x;
                tumSoundPlaySample(shoot); //shoot sound
            }
        }
        if(my_player_ship.shot_active==1 && my_player_ship.shot_y > SCREEN_HEIGHT/10){
            my_player_ship.shot_y=my_player_ship.shot_y - 8;
            tumDrawSprite(invaders_spritesheet, 2, 1, my_player_ship.shot_x, my_player_ship.shot_y);
        } else {
            my_player_ship.shot_active = 0;
            my_player_ship.shot_y = 0;
        }
        lifeIncrease();
        tumDrawSprite(invaders_spritesheet, 5, 1, my_player_ship.x, my_player_ship.y);
        return 0;
    } else {
        return 1;
    }
}

void opponentAppear(){
    int my_rand;
    my_rand = rand() % 20;
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        if(my_rand > 18 && opponent_ship.appear_counter < opponent_ship.max_appear 
        && opponent_ship.x < 0){
            opponent_ship.x = SCREEN_WIDTH;
            opponent_ship.alive = 1;
            opponent_ship.appear_counter +=1;
        }
    }
    xSemaphoreGive(game_config.game_lock);
}

int DrawBarricades(){
    for(int i=0; i<4; i++){
        if(barriers[i].hit_cnt[0]<4){
            tumDrawSprite(barrier_spritesheet, 12 + barriers[i].hit_cnt[0], 2, barriers[i].coords[0][0], barriers[i].coords[0][1]);
        }
        if(barriers[i].hit_cnt[1]<4){
            tumDrawSprite(barrier_spritesheet, 13 + barriers[i].hit_cnt[1], 2, barriers[i].coords[1][0], barriers[i].coords[1][1]);
        }
        if(barriers[i].hit_cnt[2]<4){
            tumDrawSprite(barrier_spritesheet, 12 + barriers[i].hit_cnt[2], 3, barriers[i].coords[2][0], barriers[i].coords[2][1]);
        }
        if(barriers[i].hit_cnt[3]<4){
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
            if(barriers[i].hit_cnt[j] < 4){
                for(int m=0; m<3; m++){
                    //for all invaders shots
                    if(barriers[i].coords[j][0] <= my_invaders.shots[m].x + my_invaders.shot_hitbox_alignment &&
                    barriers[i].coords[j][0] + barriers[i].hitbox_width >= my_invaders.shots[m].x + my_invaders.shot_hitbox_alignment &&
                    barriers[i].coords[j][1] + barriers[i].hitbox_height >= my_invaders.shots[m].y + my_invaders.shot_hitbox_alignment&&
                    barriers[i].coords[j][1] <= my_invaders.shots[m].y + my_invaders.shot_hitbox_alignment){
                                
                        barriers[i].hit_cnt[j] +=2;
                        my_invaders.shot_active[m]=0;
                        my_invaders.shots[m].y=0;
                    }
                }
                if(barriers[i].coords[j][0] + barriers[i].hitbox_width >= my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment &&
                    barriers[i].coords[j][0] <= my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment &&
                    barriers[i].coords[j][1] + barriers[i].hitbox_height >= my_player_ship.shot_y + my_player_ship.shot_hitbox_alignment &&
                    barriers[i].coords[j][1]  <= my_player_ship.shot_y+ my_player_ship.shot_hitbox_alignment){
                    
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
        if(my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment < invader->x+my_invaders.hitbox_width &&
        my_player_ship.shot_x + my_player_ship.shot_hitbox_alignment > invader->x &&
        my_player_ship.shot_y + my_player_ship.shot_hitbox_alignment < invader->y + my_invaders.hitbox_height &&
        my_player_ship.shot_y + my_player_ship.shot_hitbox_alignment > invader->y){
            invader->alive = 0;
            my_player_ship.shot_active=0;
            my_player_ship.shot_y=0;
            my_invaders.alive_cnt += -1;
            my_invaders.speed += 0.1;
            tumSoundPlaySample(invaderkilled);
            if(my_invaders.alive_cnt == 1){
                my_invaders.speed = 10.0; 
            }
            increaseScore(10 + abs(invader->type - 2)*10, game_config.mode);
        }
    }      
}

void createInvaderShot(int shot_number){
    int random_column;
    Invader_t* shooting_invader = NULL;
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        if(!my_invaders.shot_active[shot_number]){
            while(!shooting_invader){
                random_column = rand() % (INVADER_COLUMNS-1);
                for(int i=0; i<INVADER_ROWS; i++){
                    if(my_invaders.invaders[i][random_column].alive){
                        shooting_invader = &my_invaders.invaders[i][random_column];
                    }
                }
            }
            my_invaders.shot_active[shot_number] = 1;
            my_invaders.shots[shot_number].x = shooting_invader->x;
            my_invaders.shots[shot_number].y = shooting_invader->y;
        }
    }
    xSemaphoreGive(game_config.game_lock);
}

void shotOneCallBack(){
    createInvaderShot(0);
}

void shotTwoCallBack(){
    createInvaderShot(1);
}

void shotThreeCallBack(){
    createInvaderShot(2);
}

void checkGameOver(Invader_t *invader){
    if(invader->y > SCREEN_HEIGHT*16/20){
        my_player_ship.lives = 0;
        resetCurrentScore();
    }
}

int checkReachedBarrier(Invader_t *invader){
    if(invader->y > SCREEN_HEIGHT*15/20){
        for(int i=0; i<4; i++){
        //for every barrier
            for (int j=0; j<4; j++){
                //for every barrier part
                barriers[i].hit_cnt[j]=4;
            }
        }
        return 1;
    } else {
        return 0;
    }
}

void moveInvadersDown(){
    xSemaphoreGive(DownwardSignal);
}

int getPlayerLives(){
    int return_value=0;
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        return_value = my_player_ship.lives;
    }
    xSemaphoreGive(game_config.game_lock);
    return return_value;
}

int getAliveInvaders(){
    int return_value=0;
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        return_value = my_invaders.alive_cnt;
    }
    xSemaphoreGive(game_config.game_lock);
    return return_value;
}

void updateStartScore(){
    if(getDebouncedButtonState(KEYCODE(UP))){
        increaseScore(10, 0);
        if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
            game_config.increased_start_score = 1;
        }
        xSemaphoreGive(game_config.game_lock);
    } else if(getDebouncedButtonState(KEYCODE(DOWN))){
        increaseScore(-10, 0);
    }
}

int updateInfiniteLives(){
    static int return_value = 0;
    if(getDebouncedButtonState(KEYCODE(O))){
        if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
            if(!game_config.infinite_lives){
                my_player_ship.lives = -1;
                game_config.infinite_lives = 1;
            } else {
                game_config.infinite_lives = 0;
            }
            return_value = game_config.infinite_lives;
        }
        xSemaphoreGive(game_config.game_lock);
    }
    return return_value;
}

int initiateTimer(){
    DownwardSignal = xSemaphoreCreateMutex();
    InvaderShotTimerOne = xTimerCreate("InvadershotTimerOne", pdMS_TO_TICKS(1000), pdTRUE, (void*)0, shotOneCallBack);
    InvaderShotTimerTwo = xTimerCreate("InvadershotTimerTwo", pdMS_TO_TICKS(800), pdTRUE, (void*)0, shotTwoCallBack);
    InvaderShotTimerThree = xTimerCreate("InvadershotTimerThreee", pdMS_TO_TICKS(700), pdTRUE, (void*)0, shotThreeCallBack);
    InvaderDownwardsTimer = xTimerCreate("InvaderDownwardsTimer", pdMS_TO_TICKS(3000), pdTRUE, (void*)0, moveInvadersDown);
    OpponentShipSpTimer = xTimerCreate("OpponentShipSpTimer", pdMS_TO_TICKS(1000), pdTRUE, (void*)0, opponentAppear);
    return 0;
}

void startTimer(){
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        xTimerStart(InvaderShotTimerOne, 0);
        if(game_config.level>0){
            xTimerStart(InvaderShotTimerTwo, 0);
        } else if (game_config.level>1){
            xTimerStart(InvaderShotTimerThree, 0);
        }
        xTimerStart(InvaderDownwardsTimer, 0);
        xTimerStart(OpponentShipSpTimer, 0);
        }
    xSemaphoreGive(game_config.game_lock);
}

void stopTimer(){
    xTimerStop(InvaderShotTimerOne, 0);
    xTimerStop(InvaderShotTimerTwo, 0);
    xTimerStop(InvaderShotTimerThree, 0);
    xTimerStop(InvaderDownwardsTimer, 0);
    xTimerStop(OpponentShipSpTimer, 0);
}

void toggleDownwardSpeed(int up_down){
    static int ms = 2800; //default timer period
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        if(up_down){
            ms += -200; //faster
        } else if(game_config.level>0){
            ms += 200; //slower
        }
    }
    xSemaphoreGive(game_config.game_lock);
    xTimerChangePeriod(InvaderDownwardsTimer, pdMS_TO_TICKS(ms), 0);
}

void toggleCurrentLevel(char up_down){
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        if(up_down){
            game_config.level += 1;
        } else if(game_config.level>0){
            game_config.level += -1;
        }
    }
    xSemaphoreGive(game_config.game_lock);
}

int getCurrentLevel(){
    int return_value = 0;
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        return_value = game_config.level;
    }
    xSemaphoreGive(game_config.game_lock);
    return return_value;
}

int drawInvaders(){
    static int current_direction = 1;
    static int next_direction = 1;
    static float speed_control = 0.2;
    static int frame_counter = 0;
    static TickType_t xLastFrameTime = 0;
    int return_value = 0;

    tumDrawClear(Black);
    tumDrawLine(0, BOTTOM_LINE_HEIGHT, SCREEN_WIDTH, BOTTOM_LINE_HEIGHT, 1, Green);
    if(xSemaphoreTake(game_config.game_lock, portMAX_DELAY)==pdTRUE){
        checkBarrierHit();
        if(xSemaphoreTake(DownwardSignal, 0)==pdTRUE){
            for(int i=0; i<INVADER_ROWS; i++){
                for(int j=0; j<INVADER_COLUMNS; j++){
                        my_invaders.invaders[i][j].y = my_invaders.invaders[i][j].y + 5;
                }
            }   
        }
        for(int i=0; i<INVADER_ROWS; i++){
            for(int j=0; j<INVADER_COLUMNS; j++){
                checkHit(&my_invaders.invaders[i][j]);
                if(my_invaders.invaders[i][j].alive){
                    checkGameOver(&my_invaders.invaders[i][j]);
                    checkReachedBarrier(&my_invaders.invaders[i][j]);
                    my_invaders.invaders[i][j].x = my_invaders.invaders[i][j].x+current_direction*(int)floor(my_invaders.speed)*speed_control;
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
                else if(my_invaders.invaders[i][j].death_frame_counter<DEFAULT_INVADER_EXPLOSION_FRAMES){
                    tumDrawSprite(invaders_spritesheet, 4, 1, my_invaders.invaders[i][j].x, my_invaders.invaders[i][j].y);
                    my_invaders.invaders[i][j].death_frame_counter += 1;
                }
            }
        }
        for(int m=0; m<3; m++){
            if(my_invaders.shot_active[m]==1 && my_invaders.shots[m].y < BOTTOM_LINE_HEIGHT-20){
                my_invaders.shots[m].y=my_invaders.shots[m].y + 5;
                tumDrawSprite(invaders_spritesheet, 1, 1, my_invaders.shots[m].x, my_invaders.shots[m].y);
            } else if (my_invaders.shot_active[m]==1 && my_invaders.shots[m].y >= BOTTOM_LINE_HEIGHT-20){
                my_invaders.shot_active[m] = 0;
                my_invaders.shots[m].y = 0;
            }
        }
        if(frame_counter<2){
            speed_control = 0;
            frame_counter +=1;
        } else {
            speed_control = 1;
            frame_counter = 0;
        }
        current_direction=next_direction;
        DrawOpponentShip();
        DrawBarricades();
        xLastFrameTime = xTaskGetTickCount();
        return_value = DrawPlayerShip();
    }
    xSemaphoreGive(game_config.game_lock);
    return return_value;
}