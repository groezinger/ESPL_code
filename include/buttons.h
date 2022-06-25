//Codes to evaluate corresponding array position in my_used_buttons[]
typedef enum{
    MOUSE_LEFT = 0,
    MOUSE_MIDDLE = 1,
    MOUSE_RIGHT = 2,
    KEYBOARD_A = 3,
    KEYBOARD_B = 4,
    KEYBOARD_C = 5,
    KEYBOARD_D = 6,
    KEYBOARD_E = 7,
    KEYBOARD_K = 8,
    KEYBOARD_L = 9,
    KEYBOARD_J = 10,
    KEYBOARD_Q = 11
} MY_CODES;

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
    unsigned char buttons[SDL_NUM_SCANCODES];
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

/* function to take desired action for the single buttons when they have been pressed/not pressed
paramter: my_button_t* my_used_buttons: array of my_button_t with all the used buttons. Single button can be accessed for example by [KEYBOARD_A] = [3]
returns: nothing */
void evaluateButtons();

int getButtonCounter(MY_CODES code);

int getButtonState(MY_CODES code);

void resetButtonCounter(MY_CODES code);