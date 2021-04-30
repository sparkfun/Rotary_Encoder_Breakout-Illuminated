// "RG_Rotary_Encoder"
// Example sketch based on the Lilypad MP3 Player
// but modified for the the Arduino Uno/RedBoard
// Modified By: Ho Yun Bobby Chan
// By: Mike Grusin, SparkFun Electronics
// http://www.sparkfun.com

// This sketch demonstrates (only) the use of the rotary encoder
// hardware on the Lilypad MP3 Player board from Sparkfun. If you
// want to use the rotary encoder in your own sketches, this code
// will give you a starting point.

// HARDWARE CONNECTIONS

// Rotary encoder pin A to digital pin 3*
// Rotary encoder pin B to analog pin 3
// Rotary encoder pin C to ground

// This sketch implements software debounce, but you can further
// improve performance by placing 0.1uF capacitors between
// A and ground, and B and ground.

// If you wish to use the RG LED and button functions of
// SparkFun part number COM-10596 use the following connections:

// Rotary encoder pin 1 (red anode) to digital pin 10
// Rotary encoder pin 2 (green anode) to analog pin 1
// Rotary encoder pin 3 (button) to digital pin 4

// This pin is commented out since it is a RG rotary switch
// Rotary encoder pin 4 (blue cathode) to digital pin 5

// Note that because this is a common cathode device,
// the pushbutton requires an external 1K-10K pullUP resistor
// to operate.

// SERIAL MONITOR

// Run this sketch with the serial monitor window set to 115200 baud.

// HOW IT WORKS

// The I/O pins used by the rotary encoder hardware are set up to
// automatically call interrupt functions (rotaryIRQ and buttonIRQ)
// each time the rotary encoder changes states.

// The rotaryIRQ function transparently maintains a counter that
// increments or decrements by one for each detent ("click") of
// the rotary encoder knob. This function also sets a flag
// (rotary_change) to true whenever the counter changes. You can
// check this flag in your main loop() code and perform an action
// when the knob is turned.

// The buttonIRQ function does the same thing for the pushbutton
// built into the rotary encoder knob. It will set flags for
// button_pressed and button_released that you can monitor in your
// main loop() code. There is also a variable for button_downtime
// which records how long the button was held down.

// There is also code in the main loop() that keeps track
// of whether the button is currently being held down and for
// how long. This is useful for "hold button down for five seconds
// to power off"-type situations, which cannot be handled by
// interrupts alone because no interrupts will be called until
// the button is actually released.

// Uses the EnableInterrupt library from
// https://github.com/GreyGnome/EnableInterrupt

// License:
// We use the "beerware" license for our firmware. You can do
// ANYTHING you want with this code. If you like it, and we meet
// someday, you can, but are under no obligation to, buy me a
// (root) beer in return.

// Have fun!
// -your friends at SparkFun

// Revision history:
// 1.0 initial release MDG 2013/01/24
// 1.1 Use EnableInterrupt library
//     Use INPUT_PULLUP


#include <EnableInterrupt.h>

// Not all of these are used in this
// sketch; the unused pins are commented out:

#define ROT_LEDG A1     // green LED
#define ROT_B A3        // rotary B
#define ROT_A 3          // rotary A
#define ROT_SW 4         // rotary puhbutton
//#define ROT_LEDB 5       // blue LED
#define ROT_LEDR 10      // red LED

// RGB LED colors (for common cathode LED, 1 is on, 0 is off)
//You will only have RG available since it is an RG Encoder
#define OFF B000
#define RED B001
#define GREEN B010
#define YELLOW B011
/*
#define BLUE B100
#define PURPLE B101
#define CYAN B110
#define WHITE B111
*/

// Global variables that can be changed in interrupt routines
volatile int rotary_counter = 0; // current "position" of rotary encoder (increments CW)
volatile boolean rotary_change = false; // will turn true if rotary_counter has changed
volatile boolean button_pressed = false; // will turn true if the button has been pushed
volatile boolean button_released = false; // will turn true if the button has been released (sets button_downtime)
volatile unsigned long button_downtime = 0L; // ms the button was pushed before release

// "Static" variables are initalized once the first time
// that loop runs, but they keep their values through
// successive loops.

static unsigned char x = 0; //LED color
static boolean button_down = false;
static unsigned long int button_down_start, button_down_time;

void setup()
{
  // Set up all the I/O pins.
  pinMode(ROT_B, INPUT_PULLUP);
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_SW, INPUT_PULLUP);

  pinMode(ROT_LEDG, OUTPUT);
  pinMode(ROT_LEDR, OUTPUT);

  setLED(YELLOW);

  Serial.begin(115200); // Use serial for debugging
  Serial.println("Begin RG Rotary Encoder Testing");

  enableInterrupt(ROT_A, &rotaryIRQ, CHANGE);
  enableInterrupt(ROT_SW, &buttonIRQ, CHANGE);
  setLED(0);
}

void buttonIRQ()
{
  // Process rotary encoder button presses and releases, including
  // debouncing (extra "presses" from noisy switch contacts).
  // If button is pressed, the button_pressed flag is set to true.
  // (Manually set this to false after handling the change.)
  // If button is released, the button_released flag is set to true,
  // and button_downtime will contain the duration of the button
  // press in ms. (Set this to false after handling the change.)

  static boolean button_state = false;
  static unsigned long start, end;
  int btn;

  btn = digitalRead(ROT_SW);
  if ((btn == LOW) && (button_state == false))
    // Button was up, but is currently being pressed down
  {
    // Discard button presses too close together (debounce)
    start = millis();
    if (start > (end + 10)) // 10ms debounce timer
    {
      button_state = true;
      button_pressed = true;
    }
  }
  else if ((btn == HIGH) && (button_state == true))
    // Button was down, but has just been released
  {
    // Discard button releases too close together (debounce)
    end = millis();
    if (end > (start + 10)) // 10ms debounce timer
    {
      button_state = false;
      button_released = true;
      button_downtime = end - start;
    }
  }
}


void rotaryIRQ()
{
  // Process input from the rotary encoder.
  // The rotary "position" is held in rotary_counter, increasing for CW rotation (changes by one per detent).
  // If the position changes, rotary_change will be set true. (You may manually set this to false after handling the change).

  // This function will automatically run when rotary encoder input A transitions in either direction (low to high or high to low)
  // By saving the state of the A and B pins through two interrupts, we'll determine the direction of rotation
  // int rotary_counter will be updated with the new value, and boolean rotary_change will be true if there was a value change
  // Based on concepts from Oleg at circuits@home (http://www.circuitsathome.com/mcu/rotary-encoder-interrupt-service-routine-for-avr-micros)
  // Unlike Oleg's original code, this code uses only one interrupt and has only two transition states;
  // it has less resolution but needs only one interrupt, is very smooth, and handles switchbounce well.

  static unsigned char rotary_state = 0; // current and previous encoder states

  rotary_state <<= 2;  // remember previous state
  rotary_state |= (digitalRead(ROT_A) | (digitalRead(ROT_B) << 1));  // mask in current state
  rotary_state &= 0x0F; // zero upper nybble

  //Serial.println(rotary_state,HEX);

  if (rotary_state == 0x09) // from 10 to 01, increment counter. Also try 0x06 if unreliable
  {
    rotary_counter++;
    rotary_change = true;
  }
  else if (rotary_state == 0x03) // from 00 to 11, decrement counter. Also try 0x0C if unreliable
  {
    rotary_counter--;
    rotary_change = true;
  }
}


void loop()
{
    static boolean long_press = false;

  // The rotary IRQ sets the flag rotary_change to true
  // if the knob position has changed. We can use this flag
  // to do something in the main loop() each time there's
  // a change. We'll clear this flag when we're done, so
  // that we'll only do this if() once for each change.

  if (rotary_change)
  {
    Serial.print("rotary: ");
    Serial.println(rotary_counter, DEC);
    rotary_change = false; // Clear flag

    // blink for visual feedback
    // Don't make delay too long, otherwise the Arduino will miss ticks
    // Flash on delay doesn't need to be as long to be noticeable
    // as the flash off delay does.
    if ((x & YELLOW) == 0) {
        setLED(RED);
        delay(5);
    }
    else {
        setLED(OFF);
        delay(10);
    }
    setLED(x);
  }

  // The button IRQ also sets flags to true, one for
  // button_pressed, one for button_released. Like the rotary
  // flag, we'll clear these when we're done handling them.

  if (button_pressed)
  {
    Serial.println("button press");
    //x++; setLED(x); // Change the color of the knob LED
    button_pressed = false; // Clear flag

    // We'll set another flag saying the button is now down
    // this is so we can keep track of how long the button
    // is being held down. (We can't do this in interrupts,
    // because the button state is not changing).

    button_down = true;
    button_down_start = millis();
  }

  if (button_released)
  {
    Serial.print("button release, downtime: ");
    Serial.println(button_downtime, DEC);
    long_press = false;
    x++; setLED(x); // Change the color of the knob LED
    button_released = false; // Clear flag

    // Clear our button-being-held-down flag
    button_down = false;

    //blink when you release the button press
    for (int i = 0; i < 4; i++) {
      digitalWrite(ROT_LEDR, HIGH);
      digitalWrite(ROT_LEDG, LOW);
      //digitalWrite(ROT_LEDB, LOW);
      delay(50);
      digitalWrite(ROT_LEDR, LOW);
      digitalWrite(ROT_LEDG, LOW);
      //digitalWrite(ROT_LEDB, LOW);
      delay(50);
    }

    setLED(x); // Change the color back to original color
  }

  // Now we can keep track of how long the button is being
  // held down, and perform actions based on that time.
  // This is useful for "hold down for five seconds to power off"
  // -type functions.

  if (button_down)
  {
    button_down_time = millis() - button_down_start;
    // Serial.println(button_down_time);
    if (button_down_time > 1000) {
        if (!long_press)
            Serial.println("button held down for one second");
        long_press = true;
    }
    //if LED is pressed down, display red
    digitalWrite(ROT_LEDR, HIGH);
    digitalWrite(ROT_LEDG, LOW);
    //digitalWrite(ROT_LEDB, LOW);

  }
}


void setLED(unsigned char color)
// Set RGB LED to one of eight colors (see #defines above)
{
  digitalWrite(ROT_LEDR, color & B001);
  digitalWrite(ROT_LEDG, color & B010);
  //digitalWrite(ROT_LEDB, color & B100);
}


