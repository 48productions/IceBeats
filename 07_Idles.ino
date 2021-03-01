/****************
 * 05_IDLES.INO *
 ***************/

/**
 * Update functions for our idle animations
 */



 
/**
 * Idle effect: Rainbow Wave
 * 
 * Rainbowy waves of goodness wipe out and slowly recede in, like the ocean.
 * The bass strip just fades in and out without rainbows so that it doesn't look like unicorn barf
 */
void idleRainbowWave() {
  fadeToBlackBy(leds, STRIP_LENGTH, 90); //Fade out the last update from the strip a bit
  short wavePos = abs(STRIP_HALF * sin(0.02 * idlePos)); //Where is the wave at during this update? Here it is!
  idleCurHue += 10 * sin(-0.04 * idlePos); //Calculate how far to move the strip-wide hue shift
  short huePos = idleCurHue; //And offset all further hues by this value
  
  for (int i = 0; i <= wavePos; i++) {
    huePos += 15; //Draw each LED on the strip, shifting the hue a tad for each LED
    leds[STRIP_HALF - i] = CHSV(huePos, 255, 255);
  }
  mirrorStrip(); //Finally, mirror the strip

  if (idlePos >= 785) { //Roughly equal to 250pi (5 cycles), which is when we should stop this idle animation
    idlePos = 0;
  }

  updateBassStrip();
  //curPalette[0] = CHSV(idleCurHue, 255, 255); //Nuking the current palette for the bass strip looks kinda bleh
}




/**
 * Scrolls singular dots on the main and bass strips back and forth
 * (Todo: Does nothing on the main strip yet)
 */
void idleCylon() {
  fadeToBlackBy(ledsBass, STRIP_BASS_LENGTH, 42); //Fade out the last update from the strip a bit
  short pixelPos = wrapValue(idlePos * 2, 0, STRIP_BASS_LENGTH - 1); //Position of the scrolling dot this update
  idleCurHue += sin(-0.04 * idlePos) * 2; //Waver the hue used for this idle a bit
  
  ledsBass[pixelPos] = CHSV(idleCurHue, 255, 255);
  ledsBass[wrapValue(pixelPos - 1, 0, STRIP_BASS_LENGTH)] = CHSV(idleCurHue, 255, 255);
  ledsBass[STRIP_BASS_LENGTH - pixelPos] = CHSV(idleCurHue, 255, 255);
  ledsBass[wrapValue(STRIP_BASS_LENGTH - pixelPos - 1, 0, STRIP_BASS_LENGTH)] = CHSV(idleCurHue, 255, 255);

  if (idlePos >= STRIP_BASS_LENGTH * 6) { //Do this for 6 cycles, then stop this animation
    bassBrightness = 0;
    idlePos = 0;
  }
}
