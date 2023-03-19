/********************
 * 03_IO_UTILS.INO  *
 *******************/

/**
 * Utility functions related to IO
 * 
 * Output functions for cab/pad lights, bass LED outputs, etc
 */




/**
 * Alternates the isAlternateBassKick variable
 */
void alternateBassSel() {
  isAlternateBassKick = !isAlternateBassKick;
}




/**
 * Writes the two bass light pins.
 * One must be the signal, the other ground
 */
void writeBass(short val) {
  if (isAlternateBassKick) {
    analogWrite(PIN_BASS_LIGHT_1, val);
    analogWrite(PIN_BASS_LIGHT_2, 0);
  } else {
    analogWrite(PIN_BASS_LIGHT_2, val);
    analogWrite(PIN_BASS_LIGHT_1, 0);
  }
}




/**
 * Writes data to the cabinet lighting shift registers, sets cabinet lighting IO pins accordingly too
 */
void writeCabLighting() {
#ifdef EXPANSION_SHIFT_REGISTERS // If we're using shift registers on the expansion port, output the data now
  // Shift registers: Uncomment the shiftOut lines to customize what data to write to the shift register(s) on the expansion port
  digitalWrite(PIN_LIGHTS_LAT, LOW); //Pull latch pin low, shift out data, and throw latch pin high again
  //shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, etcLEDs);
  shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, cabLEDs);
  //shiftOut(PIN_LIGHTS_DAT, PIN_LIGHTS_CLK, LSBFIRST, padLEDs);
  digitalWrite(PIN_LIGHTS_LAT, HIGH);
#else // No shift registers = write a few cab lights to the expansion pins instead
  digitalWrite(PIN_P1_MENU, bitRead(cabLEDs, 7));
  digitalWrite(PIN_P2_MENU, bitRead(cabLEDs, 6));
#endif

  // Then write cab LEDs hooked up to the regular IO pins
  digitalWrite(PIN_P1_UP, bitRead(padLEDs, 0));
  digitalWrite(PIN_P1_DOWN, bitRead(padLEDs, 1));
  digitalWrite(PIN_P1_LEFT, bitRead(padLEDs, 2));
  digitalWrite(PIN_P1_RIGHT, bitRead(padLEDs, 3));

  digitalWrite(PIN_P2_UP, bitRead(padLEDs, 4));
  digitalWrite(PIN_P2_DOWN, bitRead(padLEDs, 5));
  digitalWrite(PIN_P2_LEFT, bitRead(padLEDs, 6));
  digitalWrite(PIN_P2_RIGHT, bitRead(padLEDs, 7));

  digitalWrite(PIN_P1_START, bitRead(cabLEDs, 5));
  digitalWrite(PIN_P2_START, bitRead(cabLEDs, 4));
  digitalWrite(PIN_MARQUEE_LR, bitRead(cabLEDs, 0));
  digitalWrite(PIN_MARQUEE_UR, bitRead(cabLEDs, 1));
  digitalWrite(PIN_MARQUEE_LL, bitRead(cabLEDs, 2));
  digitalWrite(PIN_MARQUEE_UL, bitRead(cabLEDs, 3));
}
