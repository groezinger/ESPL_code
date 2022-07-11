#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

typedef enum{
    MY_SCANCODE_MOUSE_LEFT = 1,
    MY_SCANCODE_MOUSE_RIGHT = 2,
    MY_SCANCODE_MOUSE_MIDDLE = 3
} MY_SCANCODE_MOUSE;

//struct to save all attributes of a single button
typedef struct my_button{
    int last_debounce_time;
    int counter;
    bool button_state;
    bool last_button_state;
    bool new_press;
} my_button_t;

//buttons buffer to evaluate of input
typedef struct buttons_buffer {
    unsigned char input[SDL_NUM_SCANCODES];
    my_button_t buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

/*Function to initiate button lock
returns: 0 for success or 1 for failure */
int buttonLockInit();

/*Function to delete ButtonLock
returns: nothing */
void buttonLockExit();

/*Function to get ButtonInput
returns: nothing */
void xGetButtonInput();

/*function to debounce button
parameter:
    my_button_t my_button: button which should be debounced
    int reading: current reading of buttons value (1 pressed, 0 not pressed)
returns: 1 for button is really pressed(long enough) and 0 for button has not really been pressed or not long enough */
bool debounceButton(my_button_t* my_button, int reading);

int getButtonCounter(SDL_Scancode code);

int getDebouncedButtonState(SDL_Scancode code);

int getContinuousButtonState(SDL_Scancode code);

int getDebouncedMouseState(MY_SCANCODE_MOUSE code);

void resetButtonCounter(SDL_Scancode code);