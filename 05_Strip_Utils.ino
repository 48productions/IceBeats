/**********************
 * 04_STRIP_UTILS.INO *
 *********************/

/**
 * Utility functions related to the LED strips
 * 
 * Palette generation/setting, 
 */




/**
 * Simple helper function to mirror the strip from the first half to the second
 */
void mirrorStrip() {
  for (int i = 0; i < STRIP_HALF; i++) {
    leds[STRIP_LENGTH - i - 1] = leds[i];
  }
}




/**
 * Sets LEDs on the strip for bass kick scrolling, used by a few effects
 */
void updateBassScroll() {
  //If we're scrolling a bass kick outwards, update it
  for (int i = 0; i < 2; i++) {
    if (bass_scroll_pos[i] >= 1) {
      bass_scroll_pos[i]++;
      if (bass_scroll_pos[i] > STRIP_HALF) { //We've scrolled past the end of the strip, reset to 0 to stop scrolling
        bass_scroll_pos[i] = 0;
      } else { //Still scrolling, set an LED
        leds[STRIP_HALF - bass_scroll_pos[i]] = bass_scroll_color[i];
      }
    }
  }
}




/**
 * Overloaded setNewPalette() - give no arguments for a random palette
 */
void setNewPalette() {
  int newPalette = random(0, MAX_PALETTE + 1);
  if (newPalette == curPalette) { newPalette = curPalette + 1; } //Our new palette is the same as the old one. That's boring as heck, just add one to the current palette to get a new one kek
  setNewPalette(newPalette);
}



 
/**
 * Sets a new color palette to use
 */
void setNewPalette(int paletteId) {
  if (USE_PRESET_PALETTES) { //Preset palettes - Grab a new palette from the table-o-palettes and apply it
    memcpy(lastPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the current palette to lastPalette[]
    curPaletteIndex = wrapValue(paletteId, 0, MAX_PALETTE); //Wrap around palette values in case we lazily just give this function "curPalette + 1"
    //if (curPaletteIndex < 0) { curPaletteIndex = MAX_PALETTE; }
    //if (curPaletteIndex > MAX_PALETTE) { curPaletteIndex = 0; }
    //memcpy(curPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the new palette to use to curPalette[] (no longer used, we're gonna slowly fade between palettes now)
    
  } else { //Music-based - Grab the "common frequency", make some colors based off it
    curPalette[0] = CHSV((1-commonFreqAvg.getAverage()) * 180, 255, 255);
    curPalette[1] = CHSV(max(curPalette[0].h - 23, 0), 220, 255);
    curPalette[2] = CHSV(max(curPalette[0].h - 33, 0), 180, 255);
    curPalette[3] = CHSV(max(curPalette[0].h - 40, 0), 55, 255);
  }

  updateAltPalettes();
}




/**
 * Update any non-main palettes based on the current palette's colors (i.e. the dim palette, or bass scroll colors)
 */
void updateAltPalettes() {
  for (short i = 0; i <= 3; i++) { //Also set the current dim palette's values to be a dim version of the current palette
    curPaletteDim[i] = CHSV(curPalette[i].h, curPalette[i].s * 0.87, curPalette[i].v * 0.6);
  }

  if (curEffect == VEPulse) { //Visualizing pulse or punch, also set the color to use for bass kicks
    bass_scroll_color[0] = CHSV(curPalette[0].h + 15, constrain(curPalette[0].s - 40, 0, 255), 130);
    bass_scroll_color[1] = CHSV(curPalette[2].h + 15, constrain(curPalette[0].s - 40, 0, 255), 130);
  } else if (curEffect == VEPunch) {
    bass_scroll_color[0] = CHSV(curPalette[0].h + 10, constrain(curPalette[0].s - 40, 0, 255), 100);
    bass_scroll_color[1] = CHSV(curPalette[2].h + 10, constrain(curPalette[0].s - 40, 0, 255), 100);
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
 * Gets a CRGB color for the LED strip light test
 */
CRGB getLightTestStripColor() {
  switch (lightTestStripColor) {
    case 0:
      return CRGB::Red;
      break;
    case 1:
      return CRGB::Green;
      break;
    case 2:
      return CRGB::Blue;
      break;
    case 3:
      return CRGB::Cyan;
      break;
    case 4:
      return CRGB::Magenta;
      break;
    case 5:
      return CRGB::Yellow;
      break;
    case 6:
      return CRGB::White;
      break;
    default:
      return CRGB::Black;
  }
}




/**
 * Returns the size of the bass 
 */
float getBassSize() {
  if (reactiveBass) { //Audio-reactive bass: Use the brightness of the bass LED to control the bass strip's brightness
    //return getFFTSection(0) * STRIP_BASS_HALF;
    return (float)(bassBrightness) / 255;
    
  } else { //SM-controlled bass: Fade on/off the bass strip based off Stepmania's bass light output
    
    if (bitRead(etcLEDs, 7)) { bassStripBrightness = min(bassStripBrightness * 4, 1); //Bass on? Embrightenify (but not too much)!
    } else { bassStripBrightness = max(bassStripBrightness * 0.75, 0.01); //Bass off? Dimmerify
    }
    
    return bassStripBrightness;
  }
  
}
