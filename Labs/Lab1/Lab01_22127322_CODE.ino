// Họ và Tên: Lê Phước Phát
// MSSV: 22127322
// Lớp: 22CLC10

const unsigned int LED_pins[6] = {5, 6, 7, 8, 9, 10}; // LED pins
const unsigned int button = 11;  // button pin
const unsigned long mode_time = 4000;  // time threshold for changing modes (4s)
const unsigned long delay_time = 1000;  // delay between LED changes in Mode 2 (1s)

unsigned long press_start_time = 0;  // time when button was pressed
unsigned int last_state = LOW;  // previous button state
unsigned long led_last_time = 0;  // last time an LED was updated in Mode 2
unsigned int current_led_mode_2 = 3; // current active LED in Mode 2

bool in_mode_2 = false;  // start in Mode 1

// Mode 1: Turn on LED 1, 2, and 3, turn off the others
void mode1() {
  for (int i = 0; i < 6; ++i) {
    if (i < 3) {
      digitalWrite(LED_pins[i], HIGH); // Turn on leds 1 2 3
    } else {
      digitalWrite(LED_pins[i], LOW); // Turn off leds 4 5 6
    }
  }
}

// Mode 2: Cycle LED 4, 5, and 6 alternately every 1 second
void mode2() {
  unsigned long led_cur_time = millis();  // current time
  
  if (led_cur_time - led_last_time >= delay_time) {
    // Time to switch to the next LED
    led_last_time = led_cur_time;
    digitalWrite(LED_pins[current_led_mode_2], LOW);  // Turn off current LED
    current_led_mode_2 = (current_led_mode_2 + 1) % 3 + 3;  // Move to next LED (LED 4, 5, 6)
  }
  digitalWrite(LED_pins[current_led_mode_2], HIGH);  // Turn on new current LED
}

// Handle button presses and mode changes
void change_mode() {
  unsigned int button_state = digitalRead(button);  // read the current button state
  
  if (button_state == LOW && last_state == HIGH) {
    // Button was just released
    unsigned long press_dur = millis() - press_start_time;  // calculate press duration
    if (press_dur >= mode_time && in_mode_2 == false) {
      // Switch to Mode 2 if button was held >= 4 seconds
      led_last_time = millis();  // reset LED switch timer for Mode 2
      in_mode_2 = true;
    } else if (press_dur < mode_time && in_mode_2 == true) {
      // Switch back to Mode 1 if button was held < 4 seconds
      in_mode_2 = false;
      current_led_mode_2 = 3;  // reset to LED 4 in Mode 2
    }
    
  } else if (button_state == HIGH && last_state == LOW) {
    // Button was just pressed
    press_start_time = millis();  // start timing the press duration
  }
  
  last_state = button_state;  // update last button state
}

void setup() {
  pinMode(button, INPUT);  // set the button pin as INPUT
  Serial.begin(9600);  // initialize serial communication at 9600 bps
  
  // set the LED pins as OUTPUT
  for (int i = 0; i < 6; ++i) {
    pinMode(LED_pins[i], OUTPUT);
  }
}

void loop() {
  change_mode();  // check for mode changes
  
  if (in_mode_2 == false) {
    mode1();  // run Mode 1 if the system is in Mode 1
  }
  else if (in_mode_2 == true) {
    mode2();  // run Mode 2 if the system is in Mode 2
  }
}
