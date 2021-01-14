/***********************
   03_UPDATE_FUNCS.INO
 **********************/

/**
   Update functions for various parts of the main loop

   (Receive serial data, check for idling, etc)
*/




/**
 * Reads lighting data from serial, and updates the cabinet/pad lights with this data
 * Called when Serial.available() > 0
 */
void updateSerialLights() {
  while (Serial.available() > 0) { //While we have lights data to receive, receive and process it!
    receivedData = Serial.read(); //Read the next byte of serial data
    //Serial.println(receivedData);
    if (receivedData == '\n') { //If we got a newline (\n), we're done receiving new light states for this update
      lightBytePos = 0; //The next byte of lighting data will be the first byte
    } else {

      if (!lightTestEnabled) { //Don't process serial data if we're in the lighting test (just receive it)
        switch (lightBytePos) { //Which byte of lighting data are we now receiving?
          case 0: //First byte of lighting data (cabinet lights)
            //Read x bit from the received byte using bitRead(), then set the corresponding light to that state
            bitWrite(cabLEDs, 7, bitRead(receivedData, 0)); //Marquee up left
            bitWrite(cabLEDs, 6, bitRead(receivedData, 1)); //Marquee up right
            bitWrite(cabLEDs, 5, bitRead(receivedData, 2)); //Marquee down left
            bitWrite(cabLEDs, 4, bitRead(receivedData, 3)); //Marquee down right
            /*for (int light = 0; light <= 3; light++) { //Iterate through the 4 marquee lights
              marqueeOn[light] = bitRead(receivedData, light);
            }*/
            bitWrite(etcLEDs, 7, bitRead(receivedData, 4)); //Bass L
            bitWrite(etcLEDs, 6, bitRead(receivedData, 5)); //Bass R
            //bitWrite(cabLEDs, 7, bitRead(receivedData, 4)); //Bass L
            //bitWrite(cabLEDs, 6, bitRead(receivedData, 5)); //Bass R
            break;
          case 1: //Second byte of lighting data (P1 menu button lights)
            bitWrite(cabLEDs, 3, bitRead(receivedData, 0)); //P1 menu left
            bitWrite(cabLEDs, 2, bitRead(receivedData, 4)); //P1 start

            //bitWrite(cabLEDs, 5, bitRead(receivedData, 5)); //P1 select
            break;
          case 3: //Byte 4: P1 pad lights
            bitWrite(padLEDs, 4, bitRead(receivedData, 0)); //P1 Pad
            bitWrite(padLEDs, 7, bitRead(receivedData, 1));
            bitWrite(padLEDs, 6, bitRead(receivedData, 2));
            bitWrite(padLEDs, 5, bitRead(receivedData, 3));
            break;
          case 7: //Byte 8: P2 menu
            bitWrite(cabLEDs, 1, bitRead(receivedData, 0)); //P2 menu left
            bitWrite(cabLEDs, 0, bitRead(receivedData, 4)); //P2 start

            //bitWrite(cabLEDs, 4, bitRead(receivedData, 5)); //P2 select
            break;
          case 9: //Byte 10: P2 pad
            bitWrite(padLEDs, 3, bitRead(receivedData, 0)); //P2 Pad
            bitWrite(padLEDs, 0, bitRead(receivedData, 1));
            bitWrite(padLEDs, 2, bitRead(receivedData, 2));
            bitWrite(padLEDs, 1, bitRead(receivedData, 3));
            break;
        }
        lightBytePos++; //Finally, update how many bytes of lighting data we've received.
      }
    }
  }

  writeCabLighting(); //When we're done processing the serial data we have right now, write it to the shift registers!
}




/**
 * Update function for the lighting test
 * Called when the lighting test is active
 */
void updateLightTest() {
  if (curMillis - lightTestStartMillis >= 300000 && !DEBUG_BURNIN)  { //A lotta time has passed since we started the light test (and we're not doing a burn-in), let's just exit it now
    lightTestEnabled = false;
  }
    
  if (curMillis - lightTestLastToggleMillis >= 500) { //500ms has passed, toggle the digital cabinet lights
    analogWrite(PIN_BASS_LIGHT, 255);
    alternateBassSel();
    lightTestLastToggleMillis = curMillis;
      
    if (isAlternateBassKick) { //Also alternate the cabinet lights as well, based on the state of the alternating bass kick thingy above
      cabLEDs = 255; padLEDs = 255; etcLEDs = 255;
    } else {
      cabLEDs = 0; padLEDs = 0; etcLEDs = 0;
    }

    for (int light = 0; light <= 3; light++) { //Iterate through the 4 marquee lights
      marqueeOn[light] = isAlternateBassKick;
    }
    writeCabLighting();
  }

  //if (curMillis - lightTestLastStripUpdateMillis >= 25) { //Time to update the LED strip!
    lightTestLastStripUpdateMillis = curMillis;
    lightTestStripPos += 0.01;
    if (lightTestStripPos > 1) { //Wiped a color onto the entire strip, reset to the beginning of the strip using a new color
      lightTestStripPos = 0;
      lightTestStripColor++;
      if (lightTestStripColor >= 9) { lightTestStripColor = 0; }
    }

    if (lightTestStripColor == 7) {
      fill_rainbow(leds, lightTestStripPos * STRIP_LENGTH, 0, (float)280 / STRIP_LENGTH);
      fill_rainbow(ledsBass, lightTestStripPos * STRIP_BASS_LENGTH, 0, (float)280 / STRIP_BASS_LENGTH);
    } else {
      fill_solid(leds, lightTestStripPos * STRIP_LENGTH, getLightTestStripColor());
      fill_solid(ledsBass, lightTestStripPos * STRIP_BASS_LENGTH, getLightTestStripColor());
    }
    FastLED.show();
  //}
}




/**
 * Update function for anything that needs peak-related data (AGC, idle music, etc)
 * Called when peak.available()
 */
void updatePeak() {
  float curPeak = peak.read();
      
  //First, determine if we should be starting an idle animation (audio has been quiet for a while)
  idling = (averagePeak.getAverage() <= 0.15);
  if (idling == true && prevIdling == false) { //We just got a  f a l l i n g  i d l i n g  e d g e  (wat)
    nextIdleStartMillis = curMillis + 10000; //Start an idle anim in 10 seconds
    
  } else if (idling == false && prevIdling == true) { //We've stopped idling!
    idlePos = 0; //Stop running the idle animation, there's SOUDN (prevents running the idle anim the instant we get no sound again instead of waiting 10 seconds)
  }
  
  if (idling) {
    if (curMillis >= nextIdleStartMillis) { //Time to start an idle animation!
      nextIdleStartMillis = curMillis + 60000; //Next idle animation should start in 60 seconds
      idlePos = 1; //Kickstart the animation!
      idleCurHue = 0;
    }
  }
  prevIdling = idling;
  
  //Now let's handle Automagic Gain Control (todo: Is this even the right term in this context?)
  averagePeak.addValue(curPeak);
      
  if (averagePeak.getAverage() > 0.80) { //LOUD MUSIC
    curGain *= 0.98; //Reduce gain, update the amp!
    amp.gain(curGain);
       
  } else if (averagePeak.getAverage() < 0.80 && curGain <= 3.5) { //we quiet bois and we haven't gotten HYPER LOUD yet
    curGain *= 1.0015; //Slightly increase gain, update amp
    amp.gain(curGain);
  }

  /**
   * AGC SERIAL DEBUG
   * 
   * Use the Arduino serial monitor to fine-tune AGC values. Uncomment to print:
   *  - The average'd peak value
   *  - The raw, current peak value
   *  - The current gain
   *  - A straight line for the maximum peak
   */
  /*Serial.print(averagePeak.getAverage() * 2);
  Serial.print(" ");
  Serial.print(curPeak * 2);
  Serial.print(" ");
  Serial.print(curGain * 2);
  Serial.print(" ");
  Serial.println(2);*/
  
     
  if (curEffect == VEDebugFFT) { //Debugging the FFT? Map the peak to the first pixel!
    leds[0] = CHSV(0, 0, curPeak * 255);
  }
}




/**
 * Update function when we're running the music visualizer
 * Called when fft.available() and we're not idling
 */
void updateVisualization() {
  //Serial.println(curMillis - lastUpdateMillis); //Debug: Benchmark how fast updates are happening (disable the code to force 60fps first!)
  //lastUpdateMillis = curMillis;
  
  //bassChangeAverage.addValue(getFFTSection(0));
  shortBassAverage.addValue(getFFTSection(0)); //Add some FFT bins to our averages for beat detection
  float notBassDelta = updateNotBassAverages();
     
  //DEBUG: Print FFT bins to serial
  /*Serial.print(String(getFFTSection(0) * 5) + " ");
  Serial.print(String(getFFTSection(1) * 5) + " ");
  Serial.print(String(getFFTSection(2) * 5) + " ");
  Serial.print(String(getFFTSection(3) * 5) + " ");
  Serial.print(String(longNotBassAverage.getAverage() * 3) + " ");
  Serial.println(notBassDelta);*/
      
  if (curMillis - lastVESwap >= 5000) { //Only detect VE/palette swaps if we've swapped to a new VE for a bit
    if (notBassDelta >= 1.4) { //If we've gotten a drastic change in music (and haven't done a VE swap in a while), swap a new VE
      setNewPalette(random(0, MAX_PALETTE + 1));
      setNewVE(random(1, MAX_VE + 1));
      lastVESwap = curMillis;
    } else {
     if (USE_PRESET_PALETTES) { //Preset palettes - Set a new palette whenever we get a big (but smaller than drastic) notBass change
        if (notBassDelta >= 1.27 && curMillis - lastPaletteSwap >= 2000) {
          setNewPalette(random(0, MAX_PALETTE + 1));
          lastPaletteSwap = curMillis;
        }
        
      } else {
        setNewPalette(0); //setNewPalette will update the palette colors to use the commonFreq-derived colors, call it every update to get smooth palette fades n such
      }
    }
  }

  pitchGetLEDBrightnesses(curEffect == VEPitch); //Run VE Pitch's LED Brightness function (it also calculates our common frequency)

  
      
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
    case VEVolume:
      visualizeVolume();
      break;
    case VEPitch:
      visualizePitch();
      break;
  }

  //leds[1] = CHSV((1-commonFreq) * 170, 255, 255);  //Debug: Show our common freq -> color conversion on the first few pixels (average'd on 0, non-average'd on 1)
  //leds[0] = CHSV((1-commonFreqAvg.getAverage()) * 170, 255, 255);
  
  FastLED.show();
}




/**
 * Update function when we're running an idle animation
 * Called when we're idling, running an idle animation, and it's time for an idle animation update
 */
void updateIdleAnim() {
  lastIdleUpdateMillis = curMillis;
  idlePos++;
  
  bassBrightness = abs(255 * sin(0.05 * idlePos)); //Let's also pulse on and off the bass light - calculate it's brightness
  if (bassBrightness > lastBassBrightness && idleBassBrightnessDecreasing) { alternateBassSel(); idleBassBrightnessDecreasing = false; } //Have we just started increasing the brightness? Flip which bass light is in use
  if (bassBrightness < lastBassBrightness && !idleBassBrightnessDecreasing) { idleBassBrightnessDecreasing = true; } //Also detect when we've just started decreasing, too
  analogWrite(PIN_BASS_LIGHT, bassBrightness); //Finally write the bass brightness and record the last used bass brightness
  lastBassBrightness = bassBrightness;
  
  idleRainbowWave();
  FastLED.show();
}




/**
 * Update function for when we're TRULY idling
 * Called when we're idling but not running an idle animation
 */
void updateIdle() {
  fadeToBlackBy(leds, STRIP_LENGTH, 30);
  FastLED.show();
  
  if (bassBrightness > 0) { bassBrightness *= 0.8; analogWrite(PIN_BASS_LIGHT, bassBrightness); } //Did we not completely fade out the bass light during the last idle cycle? Let's keep fading it out here
}




/**
 * Update function for the bass LED strip
 * Called every update when not in the lighting test
 */
 void updateBassStrip() {
  smBassState = bitRead(etcLEDs, 7); //Detect when SM changes the bass light's state to switch between reactive and SM-controlled bass LED strip modes
  if (smBassState != lastSMBassState) {
    lastSMBassChange = curMillis;
    reactiveBass = false;
    if (smBassState) { bassKicked = true; } //Light JUST turned on? Grats this is also a bass kick now
    
  } else if (!reactiveBass && curMillis - lastSMBassChange >= 10000) { //Switch to reactive bass after 10 seconds of no bass LED strip changes
    reactiveBass = true;
  }
  lastSMBassState = smBassState;
  
  short bassLEDSize = getBassSize();
  //Serial.println(bassLEDSize);
  fadeToBlackBy(ledsBass, STRIP_BASS_LENGTH, 150); //Fade out the last update from the strip a bit
  
  short hueOffset = 0;
  for (int i = 0; i < bassLEDSize; i++) {
    ledsBass[STRIP_BASS_HALF - i - 1] = CHSV(curPalette[0].h - hueOffset, curPalette[0].s, curPalette[0].v);
    ledsBass[STRIP_BASS_HALF + i] = CHSV(curPalette[0].h - hueOffset, curPalette[0].s, curPalette[0].v);
    hueOffset += 2; //Offset the hue for a slight gradient across the bass LED strip
  }
 }
