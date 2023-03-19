/*****************************
   ICEBEATS MUSIC VISUALIZER
 ****************************/

//#define NO_CORRECTION 1
#define FASTLED_ALLOW_INTERRUPTS 0 //Required for longer strips (required for 90 LEDs, but not for 60)
#include "FastLED.h"

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <Bounce2.h>
#include "RunningAverage.h"
//#include <Keyboard.h>




/*****************
   CONFIGURATION
 ****************/

// ADDRESSABLE LED CONFIGURATION
#define STRIP_LENGTH 54 //Number of LEDs in the strip (warning: Adding many LEDs slows updates)
#define STRIP_DATA 14 //Pin connected to the LED strip

#define STRIP_BASS_LENGTH 128 //Number of LEDs in our bass neon strip
#define STRIP_BASS_DATA 15 //Pin connected to the bass neon strip

//Set this next option to true to make bass strip effects start in the center and wipe outwards (instead of starting at the edges and wiping inwards)
//IceBeats was designed for the bass strip to be wrapped around a subwoofer with effects starting at the bottom of the sub and climbing towards the top
//This value compensates for whether the ends of this strip are mounted at the top (true) or bottom (false) of the sub
#define INVERT_BASS_STRIP_POS false


// EXPANSION PORT CONFIGURATION
// The expansion port currently has two modes:
// Option A: Comment out the below line to drive an extra 3 cab lights off of the expansion port (currently configured for P1/P2 Menu lights)
// Option B: Uncomment the below line to output cab/pad lighting data to shift registers attached to the expansion port
//#define EXPANSION_SHIFT_REGISTERS true


// VISUALIZATION CONFIGURATION
#define USE_PRESET_PALETTES true //Set whether to use pre-set or base palettes off of the music. Pre-set is more visually pleasing, music-based may reflect changes in music better
#define HUE_WAVER_STRENGTH 1.5 //When using preset palettes, the hue of the current palette will "waver" slightly to the beat of the music. This controls how strong this waver is (0 - none, 2 - strong)


// CAB LIGHT CONFIGURATION
#define PIN_BASS_LIGHT_1 22 //PWM-able pins for the bass string
#define PIN_BASS_LIGHT_2 23 //My setup uses an LED light string where alternating LEDs are driven by reverse polarities (drive at +29V for even lights, -29V for odd lights).
//#define PIN_BASS_SEL_2 24 //These are driven using an H-bridge, which use 2 pins control which set of lights are turned on.
// (previously used one pin for enable and two for direction (L298), now uses one for each direction (DRV8871). You can also drive two single color LED strips or similar)

#define PIN_DEBUG_0 17 //Lighting test button
#define PIN_DEBUG_1 A14 //Swap visualization button
#define PIN_DEBUG_2 25 //Swap palette button (there if you need it)

// Configure pins for the expansion port shift registers (top) or expansion port menu lights (bottom)
#ifdef EXPANSION_SHIFT_REGISTERS
#define PIN_LIGHTS_LAT 1 //Pins for the latch, clock, and data lines for the lighting shift registers, if shift register output is enabled above
#define PIN_LIGHTS_CLK 0 //(Default: Mapped to the Expansion pins on the IceBeats I/O PCB)
#define PIN_LIGHTS_DAT 12

#else
#define PIN_P1_MENU 0 // P1 Menu Left/Right (expansion port pin 5)
#define PIN_P2_MENU 1 // P2 Menu Left/Right (expansion port pin 6)
#endif

#define PIN_P1_UP 8
#define PIN_P1_DOWN 9
#define PIN_P1_LEFT 10
#define PIN_P1_RIGHT 11

#define PIN_P2_UP 18
#define PIN_P2_DOWN 19
#define PIN_P2_LEFT 20
#define PIN_P2_RIGHT 21

#define PIN_P1_START 2
#define PIN_P2_START 7
#define PIN_MARQUEE_LR 3
#define PIN_MARQUEE_UR 4
#define PIN_MARQUEE_LL 5
#define PIN_MARQUEE_UL 6

// Debugging
#define DEBUG_FFT_BINS true //Set true to test FFT section responsiveness - bin sections are mapped to the brightness of specific pixels
#define DEBUG_BURNIN false //Set to true to enable the light test at boot, and to disable the 


/*****************
   EFFECT CONFIG
 ****************/

const int STRIP_HALF = STRIP_LENGTH / 2; //Convinience variables for half/4th/etc of the strip length
const int STRIP_FOURTH = STRIP_LENGTH / 4;
const int STRIP_6TH = STRIP_LENGTH / 6;
const int STRIP_8TH = STRIP_LENGTH / 8;
const int STRIP_16TH = STRIP_LENGTH / 16;
const int STRIP_32ND = STRIP_LENGTH / 32;

const int STRIP_BASS_HALF = STRIP_BASS_LENGTH / 2; //Half length of the bass strip
const float BASS_DELTA = 65.0 / STRIP_BASS_HALF; //Controls strip-wide hue shifts for bass visualizations (higher number = more rainbow for bass kicks)

const int PULSE_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for Visualization Effect Pulse (based on strip length)
const int PULSE_MAX_SIZE_LOW = STRIP_LENGTH / 8;
const int PULSE_MAX_SIZE_MID = STRIP_LENGTH / 8;
const int PULSE_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int PUNCH_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for Visualization Effect Punch
const int PUNCH_MAX_SIZE_LOW = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_MID = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int KICK_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for Visualization Effect Kick
const int KICK_MAX_SIZE_SECTION = STRIP_LENGTH / 8;

const int VOLUME_MAX_SIZE_BASS = STRIP_LENGTH / 4; //Maximum section sizes for Visualization Effect Volume
const int VOLUME_MAX_SIZE_LOW = STRIP_LENGTH / 10;
const int VOLUME_MAX_SIZE_MID = STRIP_LENGTH / 10;
const int VOLUME_MAX_SIZE_HIGH = STRIP_LENGTH / 10;

const int PITCH_SECTION_SIZE = STRIP_HALF / 3; //Section size for VE Pitch's palette gradients
const double PITCH_BIN_EXP = log10(511) / STRIP_HALF; //Magic number used to help calculate a curve to determine the number of FFT bins per pixel in VE Pitch (More documentation below)

const short COMMON_FREQ_MIN = (log10(4) / PITCH_BIN_EXP) - 1; //The first and last LED brightness indexes from VE Pitch's brightness function to use for common frequency calculation
const short COMMON_FREQ_MAX = (log10(42) / PITCH_BIN_EXP) - 1; //(For these, we're finding the pixels where FFT bins 4 and 36 are)




/**********************
   HARDWARE VARIABLES
 *********************/

//Audio setup: Uses ADC for input, then uses the magic of software for the following steps:
// ADC goes into amp, amp output goes into objects to detect audio peaks and FFT analysis

// GUItool: begin automatically generated code
AudioInputAnalog         adc;           //xy=274,198
AudioAmplifier           amp;           //xy=333,187
AudioAnalyzePeak         peak;          //xy=431,178
AudioAnalyzeFFT1024       fft;       //xy=436,222
AudioConnection          patchCord1(adc, amp);
AudioConnection          patchCord2(amp, fft);
AudioConnection          patchCord3(amp, peak);
// GUItool: end automatically generated code

CRGB leds[STRIP_LENGTH];
CRGB ledsBass[STRIP_BASS_LENGTH];

Bounce debug0 = Bounce(PIN_DEBUG_0, 5); //DEBUG 0: Lighting test button
Bounce debug1 = Bounce(PIN_DEBUG_1, 5); //DEBUG 1: Swap visualization effect
Bounce debug2 = Bounce(PIN_DEBUG_2, 5); //DEBUG 2: Swap palette

RunningAverage averagePeak(10); //The average measured peak, used for AGC
float curGain = 1; //Current gain to use, used for AGC

bool heartbeatState = LOW; //Is the heartbeat LED on or off?
unsigned long lastHeartbeatFlipMs = 0; //Last time the heartbeat LED changed states




/***************************
   VISUALIZATION VARIABLES
 **************************/

enum VisualizationEffect { //A list of all visualization effects we can use - described in their individual functions
  VEDebugFFT,
  VEKick,
  VEPunch,
  VEPulse,
  VEVolume,
  VEPitch
};
const int MAX_VE = 5; //Maximum effect number we can use

VisualizationEffect curEffect = VEPunch; //Current visualization in use


enum IdleEffect { //A list of possible idle animations
  IDRainbowWave,
  IDCylon,
};
const int MAX_ID = 1; //Maximum usable idle number

IdleEffect curIdle = IDCylon; //Current idle effect in use

const CHSV palettes[][4] = { //List of HSV color palettes to use for visualization
  {CHSV(165, 240, 255), CHSV(128, 230, 255), CHSV(128, 180, 255), CHSV(128, 40, 255) }, //Palette 0: Blue, Aqua, Light Aqua, Light Aqua/white
  {CHSV(185, 255, 255), CHSV(160, 190, 235), CHSV(200, 165, 235), CHSV(192, 65, 255) }, //Palette 1: Purple, med blue, med purple, light purple
  {CHSV(96, 200, 200), CHSV(105, 190, 215), CHSV(115, 180, 235), CHSV(120, 21, 255) }, //Palette 2: Green, med green, light green, light/white green
  {CHSV(192, 240, 200), CHSV(160, 220, 215), CHSV(128, 190, 235), CHSV(128, 60, 255) }, //Palette 3: Purple, blue, aqua, light aqua
  {CHSV(0, 220, 200), CHSV(32, 220, 215), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 4: Red, orange, yellow, light yellow
  {CHSV(32, 220, 215), CHSV(50, 180, 200), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 5: Orange, Dk Yellow, Yellow, Light yellow
};
const int MAX_PALETTE = sizeof(palettes) / sizeof(palettes[0]) - 1; //Maximum palette number we can use

int curPaletteIndex = 0; //Current palette index in use
CHSV lastPalette[4] = palettes[0]; //The previous palette colors used
CHSV curPalette[4] = palettes[0]; //Current palette colors in use
CHSV curPaletteDim[4] = palettes[0]; //A slightly dimmed version of the current palette, used for "pulse to the beat" effects
unsigned long lastHueWaverMillis = 0; //The hue will slightly waver to the beat in preset palette mode to keep things spicy - this is when our last waver started

unsigned long curMillis = 0; //Current time
unsigned long lastUpdateMillis = 0; //Last time an update happened

RunningAverage bassChangeAverage(33); //Running average of detected bass changes, used for bass kick detection
RunningAverage shortBassAverage(3); //Shorter running average of the bass, smoothes out the wrinkles so we get  s m o o t h  l i n e s
RunningAverage shortNotBassAverage(9); //Short running average of the higher frequency bins (a.k.a. notBass), also used for bass kick detection
RunningAverage longNotBassAverage(60); //Longer running average of the NotBass (old = 70)
float longNotBassAverageHistory[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //History of our longNotBassAverage, used to detect changes in songs/parts of a song for pallete changes
float lastShortBassAverage; //The short bass average last time it was checked, also used for bass kick detection
float lastBassChange; //The last bass change last time it was checked. You know the drill.
bool bassChangeIncreasing = true; //Is the bass change increasing or decreasing? Helps detect when it peaks.
unsigned long lastBassKickMillis = 0; //Last time a bass kick happened
RunningAverage bassKickLengthAverage(2); //Yet another running average the time between bass kicks
bool isAlternateBassKick = false; //Alternates every time the bass is kicked, used for the alternating bass lights
short bassBrightness; //Track the current and last brightnesses for the bass light, used for fading it out when music/idle animations stop
short lastBassBrightness = 0;
bool idleBassBrightnessDecreasing = false; //Is the bass brightness value increasing or decreasing?
unsigned long lastVESwap = 0; //Last time the VE/palette was swapped (they're swapped at major changes in songs)
unsigned long lastPaletteSwap = 0;


short bass_scroll_pos[2] = {0}; //Pulse/punch effects - The bass kick scroll's position and color
CHSV bass_scroll_color[2];

short sectionOffset = 1; //Kick effect - How many indexes should we offset the sections?
short avgBassSize = 0; //Kick - The average size the bass is
short scrollOffset = 0; //Kick - How many pixels should we offset our drawing by to scroll the sections?
unsigned long lastScrollUpdateMillis = 0; //Kick - When did we last update the scroll?
short fftLEDBrightnesses[STRIP_HALF]; //Pitch - Store some LED brightnesses based on some FFT bins (also used for common frequency detection)
float commonFreq = 0; //Where the most common frequency is in a pre-defined scale (0-1 on that scale, generated by Pitch's LED brightness function)
RunningAverage commonFreqAvg(30);

unsigned long nextIdleStartMillis = 0; //The millis time to start the next idle animation at, calculated when the audio falls quiet or an idle anim starts
bool idling = false; //Are we currently idling? If so, don't render new visualization data on the strip
bool prevIdling = false; //Were we idling last time we checked if we were idling?
short idlePos = 0; //Position we are in the current idle animation - Animation is played when this is >= 1 and idling == true
unsigned long lastIdleUpdateMillis = 0; //The next time we should update the idle animation at
const short idleUpdateMs = 25; //How often should we update the idle animation?

short idleCurHue = 0; //Current hue, used for idle effects
bool lightTestEnabled = DEBUG_BURNIN; //If we're currently in the lighting test or not, and when we entered it (default to whether we're running a burn-in or not)
unsigned long lightTestStartMillis = 0; //Also track some timestamps of when some updates last occured
unsigned long lightTestLastToggleMillis = 0;
unsigned long lightTestLastStripUpdateMillis = 0;
float lightTestStripPos = 0; //...and some other variables to track the state of the LED strip test
short lightTestStripColor = 0;

bool bassKicked = false; //Set true when the bass has kicked, either via reactive bass or SM-controlled bass
bool reactiveBass = true; //Whether the bass neon LED strips should react to SM (false) or the audio signal (true)
float bassStripBrightness = 0.01; //"Brightness" (read: percentage of lit pixels) of the bass neon LED strip
bool smBassState = false; //Track how long it's been since SM has changed the bass light's state
bool lastSMBassState = false;
unsigned long lastSMBassChange = 0;




/**************************
   STEPMANIA IO VARIABLES
 *************************/

//This code also works as an IO board for Stepmania-converted dance cabinets. These variables handle all that fun stuff:
byte receivedData = 0; //The byte of serial data we just got
int lightBytePos = 0; //How many bytes of the 13 bytes of light data have we received?

byte cabLEDs = 0; //Cab lights byte (4x marquee, 4x menu buttons)
byte padLEDs = 0; //Pad lights byte (4x pad led per player)
byte etcLEDs = 0; //Etc lights byte (2x bass light, room for expansion/modification)




/*********************
   UTILITY FUNCTIONS
 ********************/




/**
   If val is above a max value, it will wrap around to the min value
   If a val is below the min value, it will wrap around to the max value
*/
short wrapValue(short val, short minVal, short maxVal) {
  while (val > maxVal) { val -= maxVal; }
  while (val < minVal) { val += maxVal; }
  return val;
}
