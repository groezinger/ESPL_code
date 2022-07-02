#define INVADER_ROWS 5
#define INVADER_COLUMNS 8
#define TOP_LEFT_INVADER_X SCREEN_WIDTH/9
#define TOP_LEFT_INVADER_Y SCREEN_HEIGHT/8
#define xstr(s) str(s)
#define str(s) #s
#define GET(s, d) "INVADER" str(s) str(d) 

typedef struct Invader{
    char type;
    char alive;
    int initial_x;
    int initial_y;
    sequence_handle_t sequence_handle;
} Invader_t;

typedef struct PlayerShip{
    int lives;
    int initial_x;
    int initial_y;
} PlayerShip_t;

typedef struct Invaders{
    int alive_cnt;
    Invader_t invaders[INVADER_ROWS][INVADER_COLUMNS];
    int speed;
} Invaders_t;

int DrawInvaders(TickType_t xLastFrameTime);
int InitiateInvaders();
int DrawPlayerShip();
int DrawBarricades();