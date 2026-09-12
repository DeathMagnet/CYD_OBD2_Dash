//     USER DEFINED SETTINGS
// Set driver type, fonts to load, pins used, SPI control method etc.
//

#define USER_SETUP_ID 501  //Provide a unique ID

// ################################################################
// Section 1. Call up the right driver file and any options for it
// ################################################################

#define ST7796_DRIVER
#define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red
#define TFT_WIDTH  320 // Portrait mode
#define TFT_HEIGHT 480 // Portrait mode
#define TFT_INVERSION_OFF

// ##############################################################
// Section 2. Define the pins that are used for the display here
// #############################################################

#define TFT_BL   27            // LED back-light control pin
#define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light

// ###### EDIT THE PIN NUMBERS IN THE LINES FOLLOWING TO SUIT YOUR ESP32 SETUP

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST  -1  // Set to -1 if RESET connected to ESP32 RST
#define TOUCH_CS 33  // Chip select pin (T_CS) of touch screen

// This data is usually found by running a calibration sketch once.
// To get the calibration data run the tft generic Touch_calibrate example sketch and open the serial monitor
#define TOUCH_CALIBRATION { 280, 3625, 253, 3562, 7 } // <-- EXAMPLE! USE YOURS!
// uint16_t calData[5] = { 252, 3676, 180, 3648, 7 };

// ####################################################
// Section 3. Define the fonts that are to be used here
// ####################################################

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2. Small 16 pixel high font
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font
#define LOAD_FONT6  // Font 6. Large 48 pixel font
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font
#define LOAD_FONT8  // Font 8. Large 75 pixel font
#define LOAD_GFXFF  // FreeFonts

#define SMOOTH_FONT

// ############################
// Section 4. Other options
// ############################

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000

#define SPI_TOUCH_FREQUENCY  2500000
