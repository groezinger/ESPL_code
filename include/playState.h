#define INVADER_ROWS 5
#define INVADER_COLUMNS 8
#define TOP_LEFT_INVADER_X SCREEN_WIDTH/9
#define TOP_LEFT_INVADER_Y SCREEN_HEIGHT/4
#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)
#define xstr(s) str(s)
#define str(s) #s
#define GET(s, d) "INVADER" str(s) str(d) 

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
} PlayerShip_t;

typedef struct Invaders{
    int alive_cnt;
    Invader_t invaders[INVADER_ROWS][INVADER_COLUMNS];
    float speed;
    coord_t shots[3];
    int shot_x;
    int shot_y;
    int shot_active[3];
    int downward_progress;
    SemaphoreHandle_t invaders_lock;
    SemaphoreHandle_t shot_lock;
} Invaders_t;

typedef struct Barrier{
    int coords[4][2];
    int hit_cnt[4];
} Barrier_t;

int DrawInvaders(TickType_t xLastFrameTime);
int InitiateInvaders(int keep_current_data, int level, char mp_game);
int DrawPlayerShip();
void checkHit(Invader_t *invader);
void createInvaderShot(int shot_number);
int checkDeath();
void moveInvadersDown();
void checkGameOver(Invader_t *invader);
int getPlayerLives();
int getAliveInvaders();
void DrawLives();
void checkBarrierHit();
void initiateBarriers();
void initMpMode();
void updateStartScore();
int updateInfiniteLives();
int initiateTimer();
void startTimer(int current_leve);
void toggleDownwardSpeed(int up_down); //0 slower, 1 faster
void stopTimer();
void checkOpponentHit(Invader_t *opponent);
void DrawLevel(int level);
int checkReachedBarrier(Invader_t *invader);
void shotOneCallBack();
void shotTwoCallBack();
void shotThreeCallBack();
void resumeMpAI();
void pauseMpAI();
void setMpDifficulty(int level);
void lifeIncrease();
char checkAiRunning();