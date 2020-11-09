/****************
 * 05_IDLES.INO *
 ***************/

/**
 * Update functions for our idle animations
 */



 
/**
 * Idle effect: Rainbow Wave
 * 
 * Rainbowy waves of goodness wipe out and slowly recede in, like the ocean. Subsequent waves wipe farther out
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
}
