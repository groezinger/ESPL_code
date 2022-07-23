#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

int drawInvaders();
int initiateInvaders(int keep_current_data, int level, mode_t mode);
int getPlayerLives();
int getAliveInvaders();
void drawLives();
void initMpMode();
void updateStartScore();
int updateInfiniteLives();
int initiateTimer();
void startTimer(int current_leve);
void toggleDownwardSpeed(int up_down); //0 slower, 1 faster
void stopTimer();
void drawLevel(int level);
void resumeMpAI();
void pauseMpAI();
void setMpDifficulty(int level);
char checkAiRunning();