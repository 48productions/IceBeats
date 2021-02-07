/*************************
 * 05_VISUALIZATIONS.INO *
 ************************/

/**
 * Update functions for our music visualizations
 */



 
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
* Bass grows from center of strip, highs grow from edge of strip. SM-controlled marquee lights turn on colored portions of strip
* Bass kicks produce faint dots that scroll to the edge of the strip
*/
void visualizePulse() {
  visualizePunch();
  /*short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section in pixels
  //sectionSize[0] = getFFTSection(0) * PUNCH_MAX_SIZE_BASS;
  float sectionBass = min(getFFTSection(0) * 1.3, 1);
  sectionSize[1] = getFFTSection(1) * PUNCH_MAX_SIZE_LOW;
  sectionSize[2] = getFFTSection(2) * PUNCH_MAX_SIZE_MID / 2;
  sectionSize[3] = getFFTSection(3) * PUNCH_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 125); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos[isAlternateBassKick] = sectionSize[0];
    //bass_scroll_pos = 1;
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!


  Serial.print(sectionBass);
  Serial.print(" ");
  Serial.println((1 - sectionBass) * 255);

  //Fill the background with a gradient based on the bass bin's value
  CHSV bg1 = curPalette[0];
  bg1.v = sectionBass * 255;
  //CHSV bg2 = blend(curPaletteDim[0], CHSV(0, 0, 0), (1 - sectionBass) * 255);
  CHSV bg2 = curPaletteDim[0];
  bg2.v = sectionBass * 127;
  fill_gradient(leds, STRIP_HALF, bg1, STRIP_HALF - (STRIP_HALF * sectionBass), bg2);

  updateBassScroll();

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  short sectionOffset = STRIP_HALF - sectionSize[2]; //Mid: Center of strip
  for (int i = 0; i < sectionSize[2]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[2];
    leds[i + sectionOffset] = blend(curPaletteDim[2], curPalette[2], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  sectionOffset = STRIP_FOURTH - sectionSize[1] / 2; //Low: Offset 1/4 of strip minus half this section's size
  for (int i = 0; i < sectionSize[1]; i++) { //Set LOW
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  //High: No offset - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[3]; i++) { //Set HIGH
    //leds[i] = curPalette[3];
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress);
  }
  
  /*sectionOffset = STRIP_HALF - sectionSize[0]; //Bass: Offset 1/2 strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
  } */

  //Finally, mirror the first half of the strip to the second half
  //mirrorStrip();
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

  fadeToBlackBy(leds, STRIP_LENGTH, 130); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos[isAlternateBassKick] = sectionSize[0];
    //bass_scroll_pos = 1;
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!

  updateBassScroll();

  /*Serial.print(sectionSize[0]);
  Serial.print(" ");
  Serial.println(bass_scroll_pos);*/

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  short sectionOffset = STRIP_8TH + STRIP_16TH - sectionSize[2] / 2; //Mid: Offset 1/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[2]; i++) { //Set MID
    //leds[i + sectionOffset] = curPalette[2];
    leds[i + sectionOffset] = blend(curPaletteDim[2], curPalette[2], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  sectionOffset = STRIP_8TH * 3 - STRIP_16TH - sectionSize[1] / 2; //Low: Offset 3/8 strip minus half this section's size
  for (int i = 0; i < sectionSize[1]; i++) { //Set LOW
    //leds[i + sectionOffset] = curPalette[1];
    leds[i + sectionOffset] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  //High: No offset - Start from 0 and increment upwards
  for (int i = 0; i < sectionSize[3]; i++) { //Set HIGH
    //leds[i] = curPalette[3];
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress);
  }
  
  sectionOffset = STRIP_HALF - sectionSize[0]; //Bass: Offset 1/2 strip minus this section's size
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[i + sectionOffset] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
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
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section (pixels)
  float rawSectionSize[] = {0, 0, 0, 0}; //Size of each section (0-1)
  sectionSize[0] = getFFTSection(0) * KICK_MAX_SIZE_BASS; //Bass section's size is directly proportional to it's FFT section's value
  rawSectionSize[1] = getFFTSection(1); //The other sections are proportional in size to each other - We'll sum their values and find out how big of a percentage of the sum each section is to find their size in pixels
  rawSectionSize[2] = getFFTSection(2);
  rawSectionSize[3] = getFFTSection(3);

  getBassKicked(); //Detect bass kick times (this value is unused, but it sets variables getBassKickProgress() requires, later)
  float bassKickProgress = getBassKickProgress();
  
  short curSection = 1; //Tracks the section we're drawing
  short curOffset = 0; //Tracks the first pixel of each non-bass section we draw

  fadeToBlackBy(leds, STRIP_LENGTH, 100); //Fade out the last update from the strip a bit

  //Serial.println(avgBassSize);

  float rawSectionSum = rawSectionSize[1] + rawSectionSize[2] + rawSectionSize[3]; //Let's find the percentage each non-bass section takes up (and each section's size in pixels)! First find the sum...
  //short nonBassSize = STRIP_HALF - avgBassSize; //...then find out how much non-bass area there is on the strip to draw these other sections to
  for (int i = 1; i <= 3; i++) {
    sectionSize[i] = (rawSectionSize[i] / rawSectionSum) * STRIP_FOURTH; //...then divide each section by the sum to find their percentage of the sum
    //Finally, multiply each section's size by the area on the strip to draw all the non-bass sections to to get how big each section is in pixels
    //Serial.print(sectionSize[i]);
    //Serial.print(" ");
  }
  //Serial.println(scrollOffset);


  //Is it time to update the scroll yet?
  if (curMillis >= lastScrollUpdateMillis + (bassKickProgress > 0 ? 2 : 250)) { //(Don't wait as long for an update if the bass isn't kicking)
    lastScrollUpdateMillis = curMillis; //Record the time this update happened at as reference for timing the next update
    //avgBassSize = 0;
    scrollOffset++;
    //scrollOffset += bassKickProgress > 0 ? 2 : 1; //Update the scroll offset - Offset it by more if we're kicking the bass
    if (scrollOffset > sectionSize[sectionOffset]) { //Scrolled an entire section's worth of pixels, now offset the section instead and reset the scroll
      sectionOffset--;
      scrollOffset = -sectionSize[wrapValue(sectionOffset, 1, 3)];
      if (sectionOffset <= 0) { sectionOffset = 3; } //Out of sections to offset, reset to the last section
    }
  }

  /*Serial.print(scrollOffset);
  Serial.print(" ");
  Serial.println(sectionOffset);*/

  int i = 0; //i is used throughout the rest of this routine, declare/initialize it here
  curOffset = scrollOffset; //Start tracking the beginning of each section we're drawing...
  curSection = sectionOffset; //...and start drawing a new section based on the current section offset
  
  //Iterate through the strip half mark to find the color each strip pixel should be
  for (; i <= STRIP_HALF; i++) {
    /*Serial.print(i - curOffset);
    Serial.print("/");
    Serial.println(sectionSize[curSection]);*/
    if (i - curOffset < sectionSize[curSection]) { //In the middle of drawing a section
      leds[STRIP_HALF - i] = blend(curPaletteDim[curSection], curPalette[curSection], rawSectionSize[curSection] * 255); //Blend in a brighter version of this color depending on how loud this section is
      
    } else if (i - curOffset >= sectionSize[curSection]) { //This section has been fully drawn - skip an led and move onto the next section
      i += 2;
      curOffset = i; //Start drawing the next section where we are now
      curSection++;
      if (curSection >= 4) { //Drew all sections, reset to section 1
        curSection = 1;
      }
    }
    
  }


  //Now overlay the bass section on top of whatever we've drawn for the rest of the strip
  i = 0; //Start drawing at the center of the strip...
  for (; i <= sectionSize[0]; i++) { //And iterate through the LEDs to draw for the bass section
    //leds[STRIP_HALF - i] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
    leds[STRIP_HALF - i] = curPalette[0];
  }

  curOffset = i;
  for (; i <= curOffset + 2; i++) { //And also draw a few blank LEDs after the bass section
    leds[STRIP_HALF - i] = CRGB::Black;
  }
  
  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();

  //DEBUG: Turn on/off an LED if the bass is kicking
  /*leds[30] = CRGB::Black;
  if (bassKickProgress > 1) { leds[30] = CHSV(96, 255, 255);}
  leds[31] = CRGB::Black; leds[29] = CRGB::Black;*/
  
}




/**
 * Visualization effect: Volume
 * 
 * Modified version of Pulse/Punch to stack FFT sections on top of each other for a "VU meter"-style effect
 * Bass is still mapped to the center, and bass kicks still scroll outwards
 */
void visualizeVolume() {
  short sectionSize[] = {0, 0, 0, 0}; //Size of each visualization section in pixels
  sectionSize[0] = getFFTSection(0) * VOLUME_MAX_SIZE_BASS;
  sectionSize[1] = getFFTSection(1) * VOLUME_MAX_SIZE_LOW;
  sectionSize[2] = getFFTSection(2) * VOLUME_MAX_SIZE_MID;
  sectionSize[3] = getFFTSection(3) * VOLUME_MAX_SIZE_HIGH;

  fadeToBlackBy(leds, STRIP_LENGTH, 110); //Fade out the last update from the strip a bit

  //Did the bass just kick? Start a new bass kick scroll from the end of the current bass section
  if (getBassKicked()) {
    bass_scroll_pos[isAlternateBassKick] = sectionSize[0];
    //bass_scroll_pos = 1;
  }

  float bassKickProgress = getBassKickProgress() * 255; //Store the current bass kick progress too - We'll use it a bunch!

  updateBassScroll();

  /*Serial.print(sectionSize[0]);
  Serial.print(" ");
  Serial.println(bass_scroll_pos);*/

  //For each section to draw, iterate through the number of LEDs to draw. Then, find and add the offset to each LED to draw so it draws in the correct place on the strip
  int i = 0; //First, we'll track the LED we're writing to as i - it'll be used for the first 3 strip sections to write to
  
  //Then for each FFT section, write a number of LEDs for that section based on how loud it is
  for (int j = 0; j < sectionSize[3]; j++) { //Set LOW
    i++;
    leds[i] = blend(curPaletteDim[3], curPalette[3], bassKickProgress); //If the bass is kicking, blend in a bit of a brighter version of the current palette
  }

  i += 3; //Let's also increment i between sections, to give gaps between the FFT sections
  for (int j = 0; j < sectionSize[2]; j++) { //Set MID
    i++;
    leds[i] = blend(curPaletteDim[2], curPalette[2], bassKickProgress);
  }

  i += 3;
  for (int j = 0; j < sectionSize[1]; j++) { //Set HIGH
    i++;
    leds[i] = blend(curPaletteDim[1], curPalette[1], bassKickProgress);
  }

  //The bass section should pulse out from the center of the strip instead of stacking with the others (idk why, is it cool???)
  for (int i = 0; i < sectionSize[0]; i++) { //Set BASS
    //leds[i + sectionOffset] = curPalette[0];
    leds[STRIP_HALF - i] = blend(curPaletteDim[0], curPalette[0], bassKickProgress);
  } 

  //Finally, mirror the first half of the strip to the second half
  mirrorStrip();
}




/**
 * Visualization effect: Pitch
 * 
 * Raw FFT bins are mapped as pixels on the strip. The strip fades between the current palette colors in a big gradient, and pulses to the beat
 */
void visualizePitch() {
  getBassKicked(); //Also update the bass kick detection
  //short bassKickProgress = getBassKickProgress() * 255;
  getBassKickProgress(); //And the bass kick progress - Neither are used here but updates values needed elsewhere

  /* Gradient handling: Smoothly fade between the four palette colors along the half strip we're working with
   * We'll divide the half of the strip we're working with into three sections (one for each transition between the four palette colors)
   * The beginning/end of each section marks a palette color to fade between, so fade between the two nearest colors inside each section
  */
  short sectionPos = 0;
  short section = 0;
  CHSV ledColor;

  //Also run through the FFT bins and set some LED brightnesses based on that
  //pitchGetLEDBrightnesses(fftLEDBrightnesses);
  
  for (int i = 0; i <= STRIP_HALF; i++) { //Iterate through the LEDs on the strip to set
    sectionPos++;
    if (sectionPos >= PITCH_SECTION_SIZE) { sectionPos = 0; section++; }
    ledColor = blend(curPalette[section], curPalette[section + 1], ((float)(sectionPos) / PITCH_SECTION_SIZE) * 255); //Now find the base color for this LED - Blend between the two palette colors we're between using the position into this "section" we're at
    ledColor.v = fftLEDBrightnesses[i]; //Then change that color's value (brightness) based on how loud this pixel's set of FFT bins are
    leds[STRIP_HALF - i] = ledColor; //Finally, set this pixel and move onto the next
  }

  mirrorStrip();
}
