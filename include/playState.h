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
    sequence_handle_t sequence_handle;
} Invader_t;

typedef struct PlayerShip{
    int lives;
    int initial_x;
    int initial_y;
    int shot_y;
    int shot_x;
    int shot_active;
} PlayerShip_t;

typedef struct Invaders{
    int alive_cnt;
    Invader_t invaders[INVADER_ROWS][INVADER_COLUMNS];
    float speed;
    int shot_x;
    int shot_y;
    int shot_active;
    int downward_progress;
    SemaphoreHandle_t invaders_lock;
} Invaders_t;

typedef struct Score{
    int high_score;
    int current_score;
    SemaphoreHandle_t score_lock;
} Score_t;

typedef struct Barrier{
    int coords[4][2];
    int hit_cnt[4];
} Barrier_t;

int DrawInvaders(TickType_t xLastFrameTime);
int InitiateInvaders(int keep_current_data, int level, char mp_game);
int DrawPlayerShip();
int DrawBarricades();
void checkHit(Invader_t *invader);
void createInvaderShot();
int checkDeath();
void moveInvadersDown();
void drawScore();
void checkGameOver(Invader_t *invader);
int getPlayerLives();
int getAliveInvaders();
void DrawLives();
void checkBarrierHit();
void initiateBarriers();
void resetCurrentScore();
void initMpMode();
int updateStartScore();
int updateInfiniteLives();
int initiateTimer();
void startTimer();
void increaseDownwardSpeed();