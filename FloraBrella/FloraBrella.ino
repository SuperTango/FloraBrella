#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_NeoPixel.h>

#define PIN 6
#define TOTAL_PIXEL_COUNT 13 * 8 //The total amount of pixel's/led's in your connected strip/stick (Default is 60)

#define COLOR_RED   0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE  0x0000FF
#define COLOR_CYAN  0x009999
#define COLOR_WHITE 0x999999

enum UserMode {
    MODE_RAINBOW,
    MODE_SOLID_RED,
    MODE_SOLID_GREEN,
    MODE_SOLID_BLUE,
    MODE_SOLID_CYAN,
    MODE_SOLID_WHITE,
    MODE_RANDOM_DOTS,
    MODE_READANDSET,
};

inline UserMode operator++(UserMode &currentMode, int)
{
    const UserMode prevMode = currentMode;
    const int i = static_cast<int>(currentMode);
    // This is a bit of a hack using MODE_READANDSET as the mod (%) operator value here.  We don't actually
    // want to increment to MODE_READANDSET (since it's triggered by a long click). MODE_READANDSET should always
    // be the last element in the UserMode enum, so we're using it's ordinal value to mod the number so that
    // the ++ operator will wrap around from the second to last element (the one before MODE_READANDSET) to the
    // first element.
    currentMode = static_cast<UserMode>((i + 1) % MODE_READANDSET);
    return prevMode;
}
UserMode userMode = MODE_RAINBOW;

enum ColorFunction {
    FUNCTION_RAINBOW,
    FUNCTION_COLORWIPE,
    FUNCTION_RANDOM_DOTS,
};
ColorFunction colorFunction = FUNCTION_RAINBOW;

float r, g, b;
uint32_t colorWipeColor;

enum ButtonResponse {
    BUTTONRESPONSE_NONE,
    BUTTONRESPONSE_CLICK,
    BUTTONRESPONSE_LONG_CLICK,
};
ButtonResponse readButton();

const int buttonPin = 10; // switch is connected to pin 10
int val; // variable for reading the pin status

Adafruit_NeoPixel strip = Adafruit_NeoPixel(TOTAL_PIXEL_COUNT, PIN, NEO_GRB + NEO_KHZ800);
// our RGB -> eye-recognized gamma color
byte gammatable[256];
static const int BUTTON_WAIT_MILLIS = 30;
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

bool updatingMode = false;

unsigned long lastButtonRead = 0;
bool currentButtonValue = true;
bool newButtonValue;
enum ButtonState {
    BUTTONSTATE_NORMAL,
    BUTTONSTATE_CHANGING
};

ButtonState buttonState = BUTTONSTATE_NORMAL;

unsigned long longPressStart = 0;

uint32_t takeColorMeasurement() {
    uint16_t clear, red, green, blue;

    tcs.setInterrupt(false); // turn on LED

    delay(60); // takes 50ms to read
    tcs.getRawData(&red, &green, &blue, &clear);
    tcs.setInterrupt(true); // turn off LED
    Serial.print("C:\t");
    Serial.print(clear);
    Serial.print("\tR:\t");
    Serial.print(red);
    Serial.print("\tG:\t");
    Serial.print(green);
    Serial.print("\tB:\t");
    Serial.print(blue);
// Figure out some basic hex code for visualization
    uint32_t sum = red;
    sum += green;
    sum += blue;
    sum = clear;
    r = red;
    r /= sum;
    g = green;
    g /= sum;
    b = blue;
    b /= sum;
    r *= 256;
    g *= 256;
    b *= 256;
    Serial.print("\t");
    Serial.print((int) r, HEX);
    Serial.print((int) g, HEX);
    Serial.print((int) b, HEX);
    Serial.println();

    Serial.print((int) r);
    Serial.print(" ");
    Serial.print((int) g);
    Serial.print(" ");
    Serial.println((int) b);
    return strip.Color(gammatable[(int) r], gammatable[(int) g], gammatable[(int) b]);
}

ButtonResponse readButton() {
    bool tmpButtonValue = (bool) digitalRead(buttonPin);

    if (!tmpButtonValue && (longPressStart != 0) && (millis() - longPressStart > 1000)) {
        Serial.println("longClick");
        return BUTTONRESPONSE_LONG_CLICK;
    }

    if (buttonState == BUTTONSTATE_CHANGING) {
        if (millis() - lastButtonRead > BUTTON_WAIT_MILLIS) {
            bool verifyButtonValue = tmpButtonValue;
            Serial.print("verifyButtonValue: ");
            Serial.println(verifyButtonValue);
            if (verifyButtonValue == newButtonValue) {
                buttonState = BUTTONSTATE_NORMAL;
                currentButtonValue = newButtonValue;
                if (newButtonValue) {
                    longPressStart = 0;
                }
                return BUTTONRESPONSE_CLICK;
            } else {
                buttonState = BUTTONSTATE_NORMAL;
                newButtonValue = currentButtonValue;
            }
        }
    } else {
        newButtonValue = tmpButtonValue;
        if (newButtonValue != currentButtonValue) {
            Serial.print("Got a button press, old: " );
            Serial.print(currentButtonValue);
            Serial.print(", new: " );
            Serial.print(newButtonValue);
            Serial.println();
            buttonState = BUTTONSTATE_CHANGING;
            lastButtonRead = millis();
            if (!newButtonValue) {
                longPressStart = lastButtonRead;
            }
        }
    }
    return BUTTONRESPONSE_NONE;
}

bool processedLongClick = false;
void updateUserMode() {
    ButtonResponse buttonResponse = readButton();
    if (buttonResponse == BUTTONRESPONSE_CLICK && !currentButtonValue) {
        Serial.println("X");
        if (!updatingMode) {
            Serial.print ("Changing userMode.  Old function: " );
            Serial.print(userMode);
            if (userMode == MODE_READANDSET) {
                userMode = MODE_RAINBOW;
            } else {
                userMode++;
            }
            Serial.print(", new userMode: " );
            Serial.print(userMode);
            Serial.println();
            updatingMode = true;
            switch (userMode) {
                case MODE_RAINBOW:
                    colorFunction = FUNCTION_RAINBOW;
                    break;
                case MODE_SOLID_BLUE:
                    colorFunction = FUNCTION_COLORWIPE;
                    colorWipeColor = COLOR_BLUE;
                    break;
                case MODE_SOLID_RED:
                    colorFunction = FUNCTION_COLORWIPE;
                    colorWipeColor = COLOR_RED;
                    break;
                case MODE_SOLID_CYAN:
                    colorFunction = FUNCTION_COLORWIPE;
                    colorWipeColor = COLOR_CYAN;
                    break;
                case MODE_SOLID_GREEN:
                    colorFunction = FUNCTION_COLORWIPE;
                    colorWipeColor = COLOR_GREEN;
                    break;
                case MODE_SOLID_WHITE:
                    colorFunction = FUNCTION_COLORWIPE;
                    colorWipeColor = COLOR_WHITE;
                    break;
                case MODE_RANDOM_DOTS:
                    colorFunction = FUNCTION_RANDOM_DOTS;
                    break;
                default:
                    colorFunction = FUNCTION_RAINBOW;
                    break;
            }

//            printColorFunction();
        }
        processedLongClick = false;
    } else if (buttonResponse == BUTTONRESPONSE_LONG_CLICK) {
        if (!processedLongClick) {
            processedLongClick = true;
            colorFunction = FUNCTION_COLORWIPE;
            colorWipeColor = takeColorMeasurement();

        }
    } else {
        updatingMode = false;
        processedLongClick = false;
    }
}



// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
    Serial.print("Doing ColorWipe ");
    Serial.print(c, HEX);
    Serial.print(", ColorWipeColor: " );
    Serial.println(colorWipeColor, HEX);
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
        updateUserMode();
        if (colorFunction != FUNCTION_COLORWIPE || c != colorWipeColor) {
            return;
        }
        strip.setPixelColor(i, c);
        strip.show();
        delay(wait);
    }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
    if (WheelPos < 85) {
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}


//Rainbow Program
void rainbow(uint8_t wait) {
    Serial.println("Doing Rainbow");
    uint16_t i, j;

    for (j = 0; j < 256; j++) {
        updateUserMode();
        if (colorFunction != FUNCTION_RAINBOW) {
            return;
        }

        for (i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel((i + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

// Rainbow Cycle Program - Equally distributed
void rainbowCycle(uint8_t wait) {
    uint16_t i, j;

    for (j = 0; j < 256 * 5; j++) { // 5 cycles of all colors on wheel
        for (i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        strip.show();
        delay(wait);
    }
}

void readAndSetColor() {
    takeColorMeasurement();
    colorWipeColor = strip.Color(gammatable[(int) r], gammatable[(int) g], gammatable[(int) b]);
}
unsigned long lastRandomDots = 0;
void randomDots() {
    updateUserMode();
    if (colorFunction != FUNCTION_RANDOM_DOTS) {
        return;
    }
    if (millis() - lastRandomDots > 500) {
        lastRandomDots = millis();
        for (uint16_t i = 0; i < TOTAL_PIXEL_COUNT; i++) {
            long color = random(0xFFFFFF);
            strip.setPixelColor(i, (uint32_t) color);
        }
        strip.show();
    }
}

void rain() {
    // Create an array of 20 raindrops
    const int raindropCount = 20;
    int pos[raindropCount];
    // Set each rain drop at the starting gate.
    // Signify by a position of -1
    for (int i = 0; i < raindropCount; i++) {
        pos[i] = -1;
    }
    // Main loop. Keep looping until we've done
    // enough "frames."
    boolean done = false;
    int counter = 0;
    while (!done) {
        // Start by turning all LEDs off:
        for (int i = 0; i < strip.numPixels(); i++)
            strip.setPixelColor(i, 0);

        // Loop for each rain drop
        for (int i = 0; i < raindropCount; i++) {
            // If the drop is out of the starting gate,
            // turn on the LED for it.
            if (pos[i] >= 0) {
                strip.setPixelColor(pos[i], strip.Color(0, 0, 127));
                // Move the drop down one row
                pos[i] -= 7;
                // If we've fallen off the strip, but us back at the starting gate.
                if (pos[i] < 0)
                    pos[i] = -1;
            }
            // If this drop is at the starting gate, randomly
            // see if we should start it falling.
            if (pos[i] == -1 && random(40) == 0 && counter < 380) {
                // Pick one of the 6 starting spots to begin falling
                pos[i] = 143 - random(6);
            }
            strip.show();
            delay(2);
        }

    }
}

void setup() {
    Serial.begin(115200); // Set up serial communication at 9600bps
    Serial.println("Starting FloraBrella (Tango Version)");
    pinMode(buttonPin, INPUT_PULLUP); // Set the switch pin as input
    pinMode(PIN, OUTPUT);
    strip.setBrightness(80); //adjust brightness here

    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
    if (tcs.begin()) {
        Serial.println("Found sensor");
    } else {
        Serial.println("No TCS34725 found ... check your connections");
        while (1); // halt!
    }
    tcs.setInterrupt(true); // turn off LED

// thanks PhilB for this gamma table!
// it helps convert RGB colors to what humans see
    for (int i = 0; i < 256; i++) {
        float x = i;
        x /= 255;
        x = pow(x, 2.5);
        x *= 255;
        gammatable[i] = x;
//Serial.println(gammatable[i]);
    }

//    for (int i = 0;
//         i < 3; i++) { //this sequence flashes the first pixel three times as a countdown to the color reading.
//        strip.setPixelColor(0, strip.Color(188, 188,
//                                           188)); //white, but dimmer-- 255 for all three values makes it blinding!
//        strip.show();
//        delay(1000);
//        strip.setPixelColor(0, strip.Color(0, 0, 0));
//        strip.show();
//        delay(500);
//    }
//
//    rain();
//    rainbowCycle(10);
//    takeColorMeasurement();
//    colorWipe(strip.Color(gammatable[(int) r], gammatable[(int) g], gammatable[(int) b]), 0);
}

void loop() {
    switch (colorFunction) {
        case FUNCTION_RAINBOW:
            rainbow(3);
            break;
        case FUNCTION_COLORWIPE:
            colorWipe(colorWipeColor, 10);
            break;
        case FUNCTION_RANDOM_DOTS:
            randomDots();
            break;
        default:
            FUNCTION_RAINBOW;
            break;
    }
}