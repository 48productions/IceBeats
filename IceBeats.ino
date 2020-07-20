/*****************************
 * ICEBEATS MUSIC VISUALIZER *
 ****************************/
 
#include "FastLED.h"

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <Bounce2.h>
#include "RunningAverage.h"



/*****************
 * CONFIGURATION *
 ****************/
#define STRIP_LENGTH 60 //Number of LEDs in the strip
#define DATA_PIN_STRIP 6 //Pin connected to the LED strip

#define DEBUG_FFT_BINS true //Set true to test FFT section responsiveness - bin sections are mapped to the brightness of specific pixels



/*************************************************
 * EFFECT CONFIG (AUTO GENERATED AT COMPILE TIME * 
 ************************************************/

const int STRIP_HALF = STRIP_LENGTH / 2; //Convinience variables for half/4th/etc of the strip length
const int STRIP_FOURTH = STRIP_LENGTH / 4;
const int STRIP_8TH = STRIP_LENGTH / 8;
const int STRIP_16TH = STRIP_LENGTH / 16;
const int STRIP_32ND = STRIP_LENGTH / 32;

const int PULSE_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Pulse (based on strip length)
const int PULSE_MAX_SIZE_MID = STRIP_LENGTH / 6;
const int PULSE_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int PUNCH_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Punch
const int PUNCH_MAX_SIZE_LOW = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_MID = STRIP_LENGTH / 8;
const int PUNCH_MAX_SIZE_HIGH = STRIP_LENGTH / 8;

const int KICK_MAX_SIZE_BASS = STRIP_LENGTH / 6; //Maximum section sizes for VE Kick
const int KICK_MAX_SIZE_SECTION = STRIP_LENGTH / 8;



 /**********************
  * HARDWARE VARIABLES *
  *********************/
  
// GUItool: begin automatically generated code
AudioInputAnalog         adc1;           //xy=274,198
AudioAnalyzePeak         peak1;          //xy=431,178
AudioAnalyzeFFT1024       fft;       //xy=436,222
AudioConnection          patchCord1(adc1, fft);
AudioConnection          patchCord2(adc1, peak1);
// GUItool: end automatically generated code

CRGB leds[STRIP_LENGTH];

Bounce debug0 = Bounce(5, 5); //DEBUG 0: Swap palette
Bounce debug1 = Bounce(4, 5); //DEBUG 1: Force bass kick/Swap visualization effect
Bounce debug2 = Bounce(3, 5); //DEBUG 2: Max FFT section reads, force getBassKicking()



/***************************
 * VISUALIZATION VARIABLES *
 **************************/

enum VisualizationEffect { //A list of all visualization effects we can use - described in their individual functions
  VEDebugFFT,
  VEKick,
  VEPunch,
  VEPulse
};

const int MAX_VE = 3; //Maximum effect number we can use

const CHSV palettes[][4] = { //List of HSV color palettes to use for visualization
  {CHSV(165, 240, 255), CHSV(128, 230, 255), CHSV(128, 180, 255), CHSV(128, 40, 255) }, //Palette 0: Blue, Aqua, Light Aqua, Light Aqua/white
  {CHSV(185, 255, 255), CHSV(160, 190, 235), CHSV(200, 165, 235), CHSV(192, 65, 255) }, //Palette 1: Purple, med blue, med purple, light purple
  {CHSV(96, 200, 200), CHSV(105, 190, 215), CHSV(115, 180, 235), CHSV(120, 21, 255) }, //Palette 2: Green, med green, light green, light/white green
  {CHSV(192, 240, 200), CHSV(160, 220, 215), CHSV(128, 190, 235), CHSV(128, 60, 255) }, //Palette 3: Purple, blue, aqua, light aqua
  {CHSV(0, 220, 200), CHSV(32, 220, 215), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 4: Red, orange, yellow, light yellow
  {CHSV(32, 220, 215), CHSV(50, 180, 200), CHSV(64, 210, 235), CHSV(64, 50, 255) }, //Palette 5: Orange, Dk Yellow, Yellow, Light yellow
  };

const int MAX_PALETTE = sizeof(palettes) / sizeof(palettes[0]) - 1; //Maximum palette number we can use

VisualizationEffect curEffect = VEPunch; //Current visualization in use

int curPaletteIndex = 0; //Current palette index in use
CHSV curPalette[4] = palettes[0]; //Current palette colors in use

unsigned long curMillis = 0; //Current time

RunningAverage bassAverage(100); //Running average of the bass section, used for bass kick detection
RunningAverage shortBassAverage(2); //Shorter running average of the bass, used to tell if the bass is increasing or decreasing
unsigned long lastBassKickMillis = 0; //Last time a bass kick happened


short bass_scroll_pos = 0; //Pulse/punch effects - The bass kick scroll's position and color
CHSV bass_scroll_color;

short sectionOffset = 1; //Kick effect - How many indexes should we offset the sections?
short avgBassSize = 0; //Kick - The average size the bass is
short scrollOffset = 0; //Kick - How many pixels should we offset our drawing by to scroll the sections?
unsigned long nextScrollUpdateMillis = 0; //Kick - When did we last update the scroll?


unsigned long nextIdleStartMillis = 0; //The millis time to start the next idle animation at, calculated when the audio falls quiet or an idle anim starts
bool idling = false; //Are we currently idling? If so, don't render new visualization data on the strip
bool prevIdling = false; //Were we idling last time we checked if we were idling?



/********************
 * FUNCTIONS N SHIZ *
 *******************/


void setup() {
  AudioMemory(16);
  Serial.begin(9600);
  LEDS.addLeds<WS2812, DATA_PIN_STRIP, GRB>(leds, STRIP_LENGTH);
  LEDS.setBrightness(84);

  pinMode(3, INPUT_PULLUP); //Pinmode the debug buttons
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);

  setNewPalette(0); //Initialize palette-related variables

  bassAverage.clear(); //Clear out running average for the bass
  shortBassAverage.clear();
}


void loop() {
  debug0.update();
  debug1.update();
  debug2.update();

  if (debug0.rose()) { //Debug 0 pressed, swap to a new color palette
    setNewPalette(curPaletteIndex + 1);
  }

  if (debug1.rose()) { //Debug 1 pressed, swap to a new visualization effect
    setNewVE(curEffect + 1);
  }

  curMillis = millis();
  
  if (peak1.available()) { //We have peak data to read! READ IT!
    float peak = peak1.read();
    //Serial.println(peak);
    idling = (peak <= 0.05);
    if (idling == true && prevIdling == false) { //We just got a f a l l i n g  i d l i n g  e d g e (wat)
      nextIdleStartMillis = curMillis + 10000; //Start an idle anim in 10 seconds
    }

    if (idling) {
      if (curMillis >= nextIdleStartMillis) { //Time to start an idle animation!
        nextIdleStartMillis = curMillis + 60000; //Next idle animation should start in 60 seconds
      }
    }
    prevIdling = idling;
    
    if (curEffect == VEDebugFFT) { //Debugging the FFT? Map the peak to the first pixel!
      leds[0] = CHSV(0, 0, peak * 255);
    }
  }

  if (fft.available()) { //FFT has new data, let's visualize it!
    bassAverage.addValue(getFFTSection(0));
    shortBassAverage.addValue(getFFTSection(0));
    switch (curEffect) {
      case VEDebugFFT:
        visualizeFFTDebug();
        break;
      case VEPulse:
        visualizePulse();
        break;
      case VEPunch:
        visualizePunch();
        break;
      case VEKick:
        visualizeKick();
        break;
    }
    FastLED.show();
  }
  delay(10);
}


/**
 * Simple helper function to mirror the strip from the first half to the second
 */
void mirrorStrip() {
  for (int i = 0; i < STRIP_HALF; i++) {
    leds[STRIP_LENGTH - i - 1] = leds[i];
  }
}


/**
 * The FFT bins are divided into sections, color organ style.
 * Each section corresponds to a set of frequencies, return them here.
 * Always returns value between 0 and 1
 */
float getFFTSection(int id) {
  if (!debug2.read()) { return 1; } //Holding debug 2? Make all FFT sections return their max value
  
  switch (id) { //Switch based on the id of the section to get. Each Teensy FFT bin is 43Hz wide, and we get groups of bins for each section.
    case 0: //Bass bin - 129-172Hz
      return constrain(fft.read(1) * 2, 0, 1);
    case 1: //Low - 500-1.5kHz
      return constrain(fft.read(11, 34) / 1.25, 0, 1);
    case 2: //Mid - 1.5-5k
      return constrain(fft.read(35, 117) / 1.5, 0, 1);
    case 3: //High - 6k-12.04k
      return constrain(fft.read(140, 280), 0, 1);
    default:
      return 0;
  }
}


/**
 * Returns whether a bass kick has happened since the last call to getBassKicked()
 * Used for beat-based effects
 */
bool getBassKicked() {
  float curBassValue = getFFTSection(0); //Get the current value and running average of the bass section
  float curBassAverage = bassAverage.getAverage();
  /*Serial.print(curBassAverage);
  Serial.print(",");
  Serial.println(curBassValue);*/
  //if (((curBassValue >= curBassAverage * 1.3 && curBassValue >= 0.15) || debug1.rose()) && curMillis >= lastBassKickMillis + 300) { //If the bass section just increased a ton (over a certain amount) or the debug button was pressed, return TRUE if we've also had a certain amount of time pass since the last kick
  if ((curBassValue >= curBassAverage * 1.3 && curBassValue >= 0.15) && curMillis >= lastBassKickMillis + 300) {
    lastBassKickMillis = millis();
    return true;
  }
  return false;
}


/**
 * Returns true if the bass FFT bin is higher than normal
 * Used for other beat-based effects
 */
bool getBassKicking() {
  float curBassValue = getFFTSection(0); //Get the current value and running average of the bass section
  float curBassAverage = bassAverage.getAverage();
  return (curBassValue >= curBassAverage * 1.45 && curBassValue >= 0.15) || !debug2.read();
}


/**
 * Sets a new color palette to use
 */
void setNewPalette(int paletteId) {
  curPaletteIndex = paletteId;
  if (curPaletteIndex < 0) { curPaletteIndex = MAX_PALETTE; } //Wrap around palette values in case we lazily just give this function "curPalette + 1"
  if (curPaletteIndex > MAX_PALETTE) { curPaletteIndex = 0; }
  memcpy(curPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the new palette to use to curPalette[]

  if (curEffect == VEPulse) { //Visualizing pulse or punch, also set the color to use for bass kicks
    bass_scroll_color = CHSV(curPalette[0].h + 15, constrain(curPalette[0].s - 40, 0, 255), 160);
  } else if (curEffect == VEPunch) {
    bass_scroll_color = CHSV(curPalette[0].h + 10, constrain(curPalette[0].s - 40, 0, 255), 120);
  }
}


/**
 * Gets a non-bass color from the palette given a color index, wrapping around values (i.e. index 4 returns index 1's color, etc)
 */
CHSV getNonBassColor(short colorIndex) {
  /*while (colorIndex > 3) { colorIndex -= 3; }
  while (colorIndex < 1) { colorIndex += 3; }
  return curPalette[colorIndex];*/
  return curPalette[wrapValue(colorIndex, 1, 3)];
}


/**
 * If val is above a max value, it will wrap around to the min value
 * If a val is below the min value, it will wrap around to the max value
 */
short wrapValue(short val, short minVal, short maxVal) {
  while (val > maxVal) { val -= maxVal; }
  while (val < minVal) { val += maxVal; }
  return val;
}


/**
 * Sets a new Visualization Effect to use
 */
void setNewVE(int effectId) {
  curEffect = effectId;
  if (curEffect < 1) { curEffect = MAX_VE; } //Wrap around effect values for extra sanity
  if (curEffect > MAX_VE) { curEffect = 1; }

  setNewPalette(curPaletteIndex); //Also update the related palette variables
}
 
/**
 * Visualization effect: FFT Debug
 * 
 * Used to test FFT section responsiveness, audio peak and FFT sections are mapped to specific pixels.
 */
void visualizeFFTDebug() {
  float bin = getFFTSection(0); //Map bass
  leds[6] = CHSV(160, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(1); //Map low
  leds[9] = CHSV(96, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(2); //Map mid
  leds[12] = CHSV(64, 255, bin * 255);
  Serial.print(bin);
  Serial.print(",");
  bin = getFFTSection(3); //Map high
  leds[15] = CHSV(0, 255, bin * 255);
  Serial.println(bin);
}


/**
* Visualization effect: Pulse
* 
* Bass grows from center of strip, mids grow from 1/4 and 3/4 points, highs grow from edge of strip.
* Bass kicks produce faint dots that scroll to the edge of the strip
*/
void visualizePulse() {
  short sectionSize[] = {0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * PULSE_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * PULSE_MAX_SIZE_MID;
  sectionSize[2] = getFFTSection(3) * PULSE_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos = sectionSize[0];
  }

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  //fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  //High offset: None - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[2]; i++) { //Set HIGH
    leds[i] = curPalette[3];
  }

  short sectionOffset = STRIP_FOURTH - sectionSize[1] / 2; //Mid offset: 1/4 point on the strip (the center for this section) minus half of this section's size (to draw half on each side of the 1/4 point)
  for (int i = 0; i < sectionSize[1]; i++) { //Set MID
    leds[i + sectionOffset] = curPalette[1];
  }

  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass offset: 1/2 point on the strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    leds[i + sectionOffset] = curPalette[0];
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}


/**
 * Visualization effect: Punch
 * 
 * Modified version of Pulse to have 4 sections (bass, low, mid, high) instead of 3
 * Bass kicks exist, too
 */
void visualizePunch() {
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * PUNCH_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * PUNCH_MAX_SIZE_LOW;
  sectionSize[2] = getFFTSection(2) * PUNCH_MAX_SIZE_MID;
  sectionSize[3] = getFFTSection(3) * PUNCH_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos = sectionSize[0];
  }

  //If we're scrolling a bass kick outwards, update it
  if (bass_scroll_pos >= 1) {
    bass_scroll_pos++;
    if (bass_scroll_pos > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
      bass_scroll_pos = 0;
    } else { //Still scrolling, set an LED
      leds[STRIP_HALF - bass_scroll_pos] = bass_scroll_color;
    }
  }

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  short sectionOffset = STRIP_8TH + STRIP_16TH - sectionSize[2] / 2; //Mid: Offset 1/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[2]; i++) { //Set MID
    leds[i + sectionOffset] = curPalette[2];
  }

  sectionOffset = STRIP_8TH * 3 - STRIP_16TH - sectionSize[1] / 2; //Low: Offset 3/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[1]; i++) { //Set LOW
    leds[i + sectionOffset] = curPalette[1];
  }

  //High: No offset - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[3]; i++) { //Set HIGH
    leds[i] = curPalette[3];
  }
  
  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass: Offset 1/2 strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    leds[i + sectionOffset] = curPalette[0];
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}


/**
 * Visualization effect: Kick
 * 
 * Three sections + bass, solid color for each. Bass is drawn proportionally to FFT size, other sections take up the rest of the strip, their sizes proportional to each other
 * Other sections always scrolling outward, speed varies depending on bass value
 */
void visualizeKick() {
  float sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section
  sectionSize[0] = getFFTSection(0) * KICK_MAX_SIZE_BASS; //Bass section's size is directly proportional to it's FFT section's value
  sectionSize[1] = getFFTSection(1); //The other sections are proportional in size to each other - We'll sum their values and find out how big of a percentage of the sum each section is to find their size in pixels
  sectionSize[2] = getFFTSection(2);
  sectionSize[3] = getFFTSection(3);

  short curSection = 1; //Tracks the section we're drawing
  short curOffset = 0; //Tracks the first pixel of each non-bass section we draw

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  avgBassSize = (bassAverage.getAverage() * KICK_MAX_SIZE_BASS) + STRIP_8TH;
  //Serial.println(avgBassSize);

  float sectionSum = sectionSize[1] + sectionSize[2] + sectionSize[3]; //Let's find the percentage each non-bass section takes up (and each section's size in pixels)! First find the sum...
  short nonBassSize = STRIP_HALF - avgBassSize; //...then find out how much non-bass area there is on the strip to draw these other sections to
  for (int i = 1; i <= 3; i++) {
    sectionSize[i] = (sectionSize[i] / sectionSum) * nonBassSize; //...then divide each section by the sum to find their percentage of the sum
    //Finally, multiply each section's size by the area on the strip to draw all the non-bass sections to to get how big each section is in pixels
  }


  //Is it time to update the scroll yet?
  if (curMillis >= nextScrollUpdateMillis) {
    nextScrollUpdateMillis = curMillis + (getBassKicking() ? 5 : 150); //Find the next time to update the scroll at - don't wait as long if the bass isn't kicking
    //avgBassSize = 0;
    scrollOffset++;
    if (scrollOffset > sectionSize[sectionOffset]) { //Scrolled an entire section's worth of pixels, now offset the section instead and reset the scroll
      sectionOffset--;
      scrollOffset = -sectionSize[wrapValue(sectionOffset, 1, 3)];
      if (sectionOffset <= 0) { sectionOffset = 3; } //Out of sections to offset, reset to the last section
    }
  }


  //Step 1: Draw the bass section of the strip
  int i = 0; //i is used throughout the rest of this routine, declare/initialize it here
  for (; i <= sectionSize[0]; i++) {
    leds[STRIP_HALF - i] = curPalette[0];
  }

  if (i <= avgBassSize - 2) { //Step 2: If we're before the point to start drawing the non-bass sections, skip ahead to that point
    i = avgBassSize;
  } else { //Otherwise, skip a few pixels before drawing the next sections
    i += 3;
  }

  curOffset = avgBassSize + scrollOffset; //Start tracking the beginning of each section we're drawing...
  curSection = sectionOffset; //...and start drawing a new section based on the current section offset
  
  //Step 3: Pick up where we left off and iterate through the strip half mark to find the color each strip pixel should be
  for (; i <= STRIP_HALF; i++) {
    /*Serial.print(i - curOffset);
    Serial.print("/");
    Serial.println(sectionSize[curSection]);*/
    if (i - curOffset < sectionSize[curSection]) { //In the middle of drawing a section
      leds[STRIP_HALF - i] = curPalette[curSection];
      
    } else if (i - curOffset >= sectionSize[curSection]) { //This section has been fully drawn - skip an led and move onto the next section
      i += 2;
      curOffset = i; //Start drawing the next section where we are now
      curSection++;
      if (curSection >= 4) { //Drew all sections, reset to section 1
        curSection = 1;
      }
    }
    
  }
  
  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();

  //DEBUG: Turn on/off an LED if the bass is kicking
  /*leds[30] = CRGB::Black;
  if (getBassKicking()) { leds[30] = CHSV(96, 255, 255);}
  leds[31] = CRGB::Black; leds[29] = CRGB::Black;*/
  
}
