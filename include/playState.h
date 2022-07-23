#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

typedef enum game_mode{
    None = 0,
    SinglePlayer = 1,
    Multiplayer = 2 
}game_mode_t;

void initGameConfig();
int initiateInvaders(int keep_current_data, game_mode_t mode);
int initiateTimer();
void initMpMode();

void updateStartScore();
int updateInfiniteLives();
void toggleDownwardSpeed(int up_down); //0 slower, 1 faster
void toggleCurrentLevel(char up_down);


void startTimer();
void stopTimer();

int drawInvaders();

int getPlayerLives();
int getAliveInvaders();
int getCurrentLevel();

void resumeMpAI();
void pauseMpAI();
void setMpDifficulty();
char checkAiRunning();