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
 * Sets a new color palette to use
 */
void setNewPalette(int paletteId) {
  /*curPaletteIndex = wrapValue(paletteId, 0, MAX_PALETTE); //Wrap around palette values in case we lazily just give this function "curPalette + 1"
  //if (curPaletteIndex < 0) { curPaletteIndex = MAX_PALETTE; }
  //if (curPaletteIndex > MAX_PALETTE) { curPaletteIndex = 0; }
  memcpy(curPalette, palettes[curPaletteIndex], sizeof(curPalette)); //Copy the contents of the new palette to use to curPalette[]*/

  curPalette[0] = CHSV((1-commonFreqAvg.getAverage()) * 180, 255, 255);
  curPalette[1] = CHSV(max(curPalette[0].h - 23, 0), 220, 255);
  curPalette[2] = CHSV(max(curPalette[0].h - 33, 0), 180, 255);
  curPalette[3] = CHSV(max(curPalette[0].h - 40, 0), 55, 255);
  
  for (short i = 0; i <= 3; i++) { //Also set the current dim palette's values to be a dim version of the current palette
    curPaletteDim[i] = CHSV(curPalette[i].h, curPalette[i].s * 0.87, curPalette[i].v * 0.6);
  }

  if (curEffect == VEPulse) { //Visualizing pulse or punch, also set the color to use for bass kicks
    bass_scroll_color = CHSV(curPalette[0].h + 15, constrain(curPalette[0].s - 40, 0, 255), 130);
  } else if (curEffect == VEPunch) {
    bass_scroll_color = CHSV(curPalette[0].h + 10, constrain(curPalette[0].s - 40, 0, 255), 100);
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
