/********************
 * 02_FFT_UTILS.INO *
 *******************/

/**
 * Utility functions related to the FFT
 * 
 * Bass kick detection, common frequency detection, etc
 */



  
/**
 * Alternate version of getFFTSection() with some exponential magic applied to hopefully make visual effects a bit more appealing
 * On louder songs, this makes subtle volume changes more distinct but tends to *really* mute quiet songs
 * TODO: Not quite ready for prime time, revisit implementing this in the future
 */
/*float getFFTSection(int id) {
  if (debug2.read()) {
    return pow(5, getRawFFTSection(id) - 1) * 1.25 - 0.25; //Extra multiplication/subtraction to center our exponential curve within the 0-1 range we need
    return pow(10, getRawFFTSection(id) - 1) * 1.1 - 0.1; //Extra multiplication/subtraction to center our exponential curve within the 0-1 range we need
  } else {
     return getRawFFTSection(id);
  }
}*/




/**
 * The FFT bins are divided into sections, color organ style.
 * Each section corresponds to a set of frequencies, return them here.
 * Always returns value between 0 and 1
 */
float getFFTSection(int id) {
  //if (!debug2.read()) { return 1; } //Holding debug 2? Make all FFT sections return their max value
  
  switch (id) { //Switch based on the id of the section to get. Each Teensy FFT bin is 43Hz wide, and we get groups of bins for each section.
    case 0: //Bass bin - 43Hz-86Hz
      return constrain(fft.read(1) * 2, 0, 1); //Todo: Constrain is jank is frig and WILL cut off the tops of audio waveforms on busier/louder songs
    case 1: //Low - 473-1.462kHz
      return constrain(fft.read(11, 34) / 1.25, 0, 1);
    case 2: //Mid - 1.505-5.031k
      return constrain(fft.read(35, 117) / 1.5, 0, 1);
    case 3: //High - 6.02k-12.04k
      return constrain(fft.read(140, 280), 0, 1);
    default:
      return 0;
  }
}




/**
 * Updates the long not bass (FFT section 2/3) averages
 * Returns the difference % between the current longNotBassAverage and the average from several updates ago - >1 means the average is getting bigger
 */
float updateNotBassAverages() {
  for (short section = 2; section <= 3; section++) { //Here, add bins 2 and 3 to the NotBass averages
    float fftSection = getFFTSection(section);
    shortNotBassAverage.addValue(fftSection);
    longNotBassAverage.addValue(fftSection);
  }

  for (short i = 0; i <= 8; i++) { //Now for the long average history: Roll all the averages back a position...
    longNotBassAverageHistory[i] = longNotBassAverageHistory[i + 1];
  }
  longNotBassAverageHistory[9] = longNotBassAverage.getAverage(); //Then add our new average to the top of the list
  return longNotBassAverageHistory[9] / longNotBassAverageHistory[0]; //Finally, return the new value / the old value for a percent change
}




/**
 * Returns whether a bass kick has happened since the last call to getBassKicked()
 * Used for beat-based effects
 */
bool getBassKicked() {
  float curBassValue = shortBassAverage.getAverage(); //Get the current value (short average) and (longer) running average of the bass section
  float curNotBassValue = shortNotBassAverage.getAverage();
  //float curBassValue = getFFTSection(0);
  float curBassChange = max(curBassValue - lastShortBassAverage, 0); //Also find out how much the bass value changed this read
  //float curBassChange = abs(curBassValue - lastShortBassAverage); //Also find out how much the bass value changed this read
  //if (curBassChange > 0) { 
    bassChangeAverage.addValue(curBassChange);
  //}
  

  if (reactiveBass) { //Reactive bass: Do some magic to our FFT results, and see if the bass kicked that way. Not super accurate, but works in a pinch for bass-heavy songs. Kinda spammy otherwise at this time.
    float curBassChangeAverage = bassChangeAverage.getAverage();
    float curLongNotBassValue = longNotBassAverage.getAverage();
    bassKicked = false;
  
    bool notBassBelowThreshold = curNotBassValue <= curLongNotBassValue * 0.89; //Check if our NotBass is below a certain threshold
    bool bassAboveThreshold = curBassChange >= curBassChangeAverage * (notBassBelowThreshold ? 3.4 : 3.54); //Check if our bass change is above a certain threshold (lower it slightly if notbass is low, or raise it if notbass is high) (Non-notbass ORIG: 3.45)
  
    if (bassAboveThreshold && curBassChangeAverage >= 0.009) { //Did our bass change enough above the average (and above an arbitrary "volume" threshold)?
      //bassChangeAverage.addValue(lastBassChange);
      bassKicked = true; 
    }
    
  } //SM-controlled bass: Bass kicks happen whenever SM turns on the bass neon light (bassKicked is set true when needed during updateBassStrip() for this, since it also handles switching between reactive and SM modes)
  
  lastBassChange = curBassChange; //Now store the last bass change and average for the next read
  lastShortBassAverage = curBassValue;


  /**
   * BEAT DETECTION SERIAL DEBUG
   * 
   * Use the Arduino serial monitor to fine-tune AGC values. Uncomment these and the two lines under if (bassKicked &&...) to print:
   *  - A lotta random garbage
  */
  /*Serial.print(curBassValue * 12); //DEBUG: Print the current bass bin's value
  Serial.print(" ");*/
  //Serial.print(curNotBassValue * 6);
  //Serial.print(" ");
  //Serial.print(curLongNotBassValue * 6);
  //Serial.print(" ");
  //Serial.print(notBassBelowThreshold ? 6 : 0);
  //Serial.print(" ");
  /*Serial.print(curBassChangeAverage * 48); //The average bass peak value
  Serial.print(" ");*/
  /*Serial.print(curBassChangeAverage * 48 * 3.35); //The threshold to trigger a bass kick
  Serial.print(" ");
  Serial.print(curBassChangeAverage * 48 * 3.49);
  Serial.print(" ");
  Serial.print(curBassChange * 48); //And the current amount the bass has changed this update
  Serial.print(" ");*/
  /*Serial.print(getFFTSection(0) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(1) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(2) * 6);
  Serial.print(" ");
  Serial.print(getFFTSection(3) * 6);
  Serial.print(" ");*/
  /*Serial.print(" ");
  Serial.println(abs((curBassValue - curBassAverage)));*/
  
  if (bassKicked && curMillis >= lastBassKickMillis + 140) { //We detected a new peak earlier and it's been a bit since a bass kick
    bassKickLengthAverage.addValue(curMillis - lastBassKickMillis); //Add the time between the last kick and now to the average bass kick length
    lastBassKickMillis = curMillis; //Start a new bass kick!
    //Serial.println("6");
    alternateBassSel(); //Invert the alternating bass kick variable, drive the bass light selection pins
    bassKicked = false; //Clear the bass kick flag before we exit!
    return true;
  }
  //Serial.println("0");
  bassKicked = false;
  return false;
}




/**
 * Returns true if the bass is currently kicking
 * Used for other beat-based effects
 */
float getBassKickProgress() {
  /*float curBassValue = getFFTSection(0); //Old implementation: Have we met similar criteria for a regular bass kick to happen?
  float curBassAverage = bassChangeAverage.getAverage();
  bool kicking = (curBassValue >= curBassAverage * 1.35 && curBassValue >= 0.15) || !debug2.read();*/
  //bool kicking = curMillis <= lastBassKickMillis + 150;

  //The current implementation is really just an extension of getBassKicked for effects that "pulse to the beat"
  //Right now, bass kicks "happen" for 1/3 of the average time between the last few bass kicks
  //This function returns how close we are to the start of the x ms-wide window after the bass kick happens (1 - just started kick, 0 - x ms has passed since kick)
  //Since this function pulls the time the last time getBassKicked() returned true, getBassKicked() MUST be called beforehand for this function to work!
  float kicking = 0;
  int kickLength = min(bassKickLengthAverage.getAverage() / 3, 400); //Find the average kick length here, cap it at 400ms so we don't get uber long bass kicks
  //Serial.println(kickLength);
  //if (curMillis <= lastBassKickMillis + 125) { kicking = 1 - ((float)(curMillis - lastBassKickMillis) / 125); } //Only calculate this value if we're still in the middle of the bass kick (and guaranteed to not return less than 0)
  if (curMillis <= lastBassKickMillis + kickLength) { kicking = 1 - ((float)(curMillis - lastBassKickMillis) / kickLength); } //Only calculate this value if we're still in the middle of the bass kick (and guaranteed to not return less than 0)
  
  //digitalWrite(LED_BUILTIN, kicking > 0); //Also light our debug LED while the bass kick is happening
  bassBrightness = kicking * 255; //Record the brightness of the bass light (for use with other functions)
  analogWrite(PIN_BASS_LIGHT, bassBrightness); //Also (also) write our bass output while we're at it
  
  //Serial.println(kicking);
  return kicking;
}




/**
 * Returns a list of brightness (0-255) for each LED on the LED strip (for VE Pitch)
 * Accounts only for brightness changes from the LED's corresponding FFT bins
 */
void pitchGetLEDBrightnesses(bool fullStrip) {
  //short ret[STRIP_HALF];

  /*Writing down my thought process to keep my sanity: We need an exponential curve of some sort to help map FFT bins to pixels. What kind of curve? I honestly have no idea
  But just to get something down imma use: max bin = 10^(a * pixel)
  To grab that a value, some substitution is used to calculate/store it in PITCH_BIN_EXP
  y = 10^(ax)

  We know a point on this curve - the max bin (y) should be at 511 (highest bin the audio library gives us) at the strip half point (x, using 45 here)
  511 = 10^(a * 45)

  Take the log of both sides and we get log(511) = a * 45 - Then just divide by STRIP_HALF and we'll have our value!
  
  PITCH_BIN_EXP = log(511) / STRIP_HALF


  (Ninja edit: This curve spends a lotta time on the low end, gonna offset x by 1 to help it get to the higher frequencies faster)
  
  If anyone actually knows what they're doing please submit a PR - 48
  */
  
  short lastMaxBin = 0;
  short newMaxBin = 0;

  short maxCommon = 0;
  short maxIndex = 0;

  short ledStart = (fullStrip ? 0 : COMMON_FREQ_MIN); //Next, the start and end points for our iteration - If we don't need all the LED brightnesses (since we aren't drawing VE Pitch), then only do these calculations for the pixels in our common frequency range
  short ledEnd = (fullStrip ? STRIP_HALF : COMMON_FREQ_MAX);
  
  for (int i = 0; i < STRIP_HALF; i++) { //Find a brightness for each LED
    newMaxBin = pow(10, (i + 1) * PITCH_BIN_EXP); //First, calculate the highest bin we should read for this pixel
    //newMaxBin = pow(i, 1.54); //Alternative method - Less exp, more linear, not quite as good imo. Also hardcoded for a 90 px strip (45 half)
    fftLEDBrightnesses[i] = fft.read(lastMaxBin, newMaxBin) * 255 * 2.25; //Then, grab all FFT bins between the last bin read for the previous pixel, and our new max bin (* by 255 for an LED brightness)
    /*Serial.print(ret[i]);
    Serial.print(" ");
    Serial.print(lastMaxBin);
    Serial.print("-");
    Serial.println(newMaxBin);*/
    lastMaxBin = newMaxBin + 1; //Then store our current max bin for the next pixel to use (add 1 so we don't use the same bin twice)

    if (i >= COMMON_FREQ_MIN && i <= COMMON_FREQ_MAX && fftLEDBrightnesses[i] >= maxCommon) { //If we're in our scanning range for max common frequencies, then check for a new max frequency
      maxIndex = i;
      maxCommon = fftLEDBrightnesses[i];
    }
  }

  commonFreq = ((float)maxIndex - COMMON_FREQ_MIN) / (COMMON_FREQ_MAX - COMMON_FREQ_MIN);
  commonFreqAvg.addValue(commonFreq); //Finally, calculate our common frequency position based on how far into our "common range" the loudest frequency was
  /*Serial.print(COMMON_FREQ_MIN);
  Serial.print("-");
  Serial.print(COMMON_FREQ_MAX);
  Serial.print(" ");*/
  //Serial.println(commonFreq.getAverage());
  //Serial.println();
}
