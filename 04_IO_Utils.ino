/********************
 * 03_IO_UTILS.INO  *
 *******************/

/**
 * Utility functions related to IO
 * 
 * Output functions for cab/pad lights, bass LED outputs, etc
 */




/**
 * Alternates the isAlternateBassKick variable and drives the bass selection pins
 */
void alternateBassSel() {
  isAlternateBassKick = !isAlternateBassKick;
  writeBassSel(isAlternateBassKick);
}




/**
 * Writes the two bass light selection pins.
 * One must be high and the other low, otherwise we get no output when the enable pin is high.
 */
void writeBassSel(bool sel) {
  digitalWrite(PIN_BASS_SEL_1, sel);
  digitalWrite(PIN_BASS_SEL_2, !sel);
}




/**
 * Writes data to the cabinet lighting shift registers
 */
void writeCabLighting() {
  digitalWrite(PIN_LIGHTS_LAT, LOW); //Pull latch pin low, shift out data, and throw latch pin high again
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, etcLEDs);
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, cabLEDs);
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, padLEDs);
  digitalWrite(PIN_LIGHTS_LAT, HIGH);
}
