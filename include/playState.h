#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

int drawInvaders();
int initiateInvaders(int keep_current_data, mode_t mode);
int getPlayerLives();
int getAliveInvaders();
void drawLives(); //maybe move to drawing
void initMpMode();
void updateStartScore();
int updateInfiniteLives();
int initiateTimer();
void startTimer();
void toggleDownwardSpeed(int up_down); //0 slower, 1 faster
void toggleCurrentLevel(char up_down);
int getCurrentLevel();
void stopTimer();
void drawLevel(); //maybe move to drawing
void resumeMpAI();
void pauseMpAI();
void setMpDifficulty();
char checkAiRunning();
void initGameConfig();