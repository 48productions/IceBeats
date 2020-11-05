/********************
 *   01_MAIN.INO    *
 *******************/

 /**
  * Functions for setup and loop
  */


void setup() {
  Keyboard.begin();
  
  AudioMemory(16);
  fft.windowFunction(AudioWindowHamming1024);
  
  Serial.begin(9600);
  LEDS.addLeds<WS2812, DATA_PIN_STRIP, GRB>(leds, STRIP_LENGTH);
  LEDS.setBrightness(84);

  pinMode(PIN_DEBUG_0, INPUT_PULLUP); //Pinmode ALL the pins
  pinMode(PIN_DEBUG_1, INPUT_PULLUP);
  pinMode(PIN_DEBUG_2, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(DATA_PIN_STRIP, OUTPUT);
  pinMode(PIN_BASS_LIGHT, OUTPUT);
  pinMode(PIN_BASS_SEL_1, OUTPUT);
  pinMode(PIN_BASS_SEL_2, OUTPUT);
  
  pinMode(PIN_LIGHTS_LAT, OUTPUT);
  pinMode(PIN_LIGHTS_DAT, OUTPUT);
  pinMode(PIN_LIGHTS_CLK, OUTPUT);

  for (int i = 0; i <= 3; i++) { //Pinmode all the marquee lights
    pinMode(pinLightsMarquee[i], OUTPUT);
  }
  
  for (int i = 0; i <= maxKeycode; i++) { //Pinmode all the cabinet buttons we're gonna read
    pinMode(keyIOPins[i], INPUT_PULLUP);
  }

  setNewPalette(0); //Initialize palette-related variables

  bassChangeAverage.clear(); //Clear out running averages for the bass
  shortBassAverage.clear();
  shortNotBassAverage.clear();
  bassChangeAverage.addValue(0); //And add some initial data to the peak average
  averagePeak.clear();
  bassKickLengthAverage.clear();
}




void loop() {
  curMillis = millis();
  
  debug0.update();
  debug1.update();
  debug2.update();

  if (debug2.fell()) { //Debug 2 pressed, swap to a new color palette
    setNewPalette(curPaletteIndex + 1);
  }

  if (debug1.fell()) { //Debug 1 pressed, swap to a new visualization effect
    setNewVE(curEffect + 1);
  }

  if (debug0.fell()) { //Debug 0 pressed, toggle lighting test mode
    lightTestEnabled = !lightTestEnabled;
    if (lightTestEnabled) { //Going into the light test, set some variables to prepare for it
      lightTestStartMillis = curMillis;
      analogWrite(PIN_BASS_LIGHT, 255);
      lightTestStripPos = 0;
      lightTestStripColor = 0;
      FastLED.clear(); //Clear the LED strip for the test:tm:
    }
  }

  if (Serial.available() > 0) { //Do we have lights data to receive?
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

  if (lightTestEnabled) { //In the lighting test, update some lights boi
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

    if (curMillis - lightTestLastStripUpdateMillis >= 25) { //Time to update the LED strip!
      lightTestLastStripUpdateMillis = curMillis;
      lightTestStripPos++;
      if (lightTestStripPos >= STRIP_LENGTH) { //Wiped a color onto the entire strip, reset to the beginning of the strip using a new color
        lightTestStripPos = 0;
        lightTestStripColor++;
        if (lightTestStripColor >= 8) { lightTestStripColor = 0; }
      }

      leds[lightTestStripPos] = getLightTestStripColor();
      FastLED.show();
    }
    
  } else { //Not in the lighting test, update visualizations/idles/etc
    if (peak.available()) { //We have peak data to read! READ IT!
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
      
      if (averagePeak.getAverage() > 0.85) { //LOUD MUSIC
        curGain *= 0.98; //Reduce gain, update the amp!
        amp.gain(curGain);
        
      } else if (averagePeak.getAverage() < 0.85 && curGain <= 4) { //we quiet bois and we haven't gotten HYPER LOUD yet
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
  
    if (!idling && fft.available()) { //FFT has new data and we aren't idling, let's visualize it!
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
      
      if (notBassDelta >= 1.4 && curMillis - lastVESwap >= 5000) { //If we've gotten a drastic change in music (and haven't done a VE swap in a while), swap a new palette (todo: Will probably swap visualization effects in the future)
        setNewPalette(curPaletteIndex + 1);
        lastVESwap = curMillis;
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

      leds[0] = CHSV((1-commonFreq.getAverage()) * 255, 255, 255); //Debug: Show our common freq -> color conversion on the first pixel
      
      FastLED.show();
      
    } else if (idling && idlePos >= 1 && curMillis >= nextIdleUpdateMillis) { //We're idling, playing the idle animation, and it's time for an update!
      nextIdleUpdateMillis = curMillis + idleUpdateMs;
      idlePos++;
  
      bassBrightness = abs(255 * sin(0.05 * idlePos)); //Let's also pulse on and off the bass light - calculate it's brightness
      if (bassBrightness > lastBassBrightness && idleBassBrightnessDecreasing) { alternateBassSel(); idleBassBrightnessDecreasing = false; } //Have we just started increasing the brightness? Flip which bass light is in use
      if (bassBrightness < lastBassBrightness && !idleBassBrightnessDecreasing) { idleBassBrightnessDecreasing = true; } //Also detect when we've just started decreasing, too
      analogWrite(PIN_BASS_LIGHT, bassBrightness); //Finally write the bass brightness and record the last used bass brightness
      lastBassBrightness = bassBrightness;
  
      idleRainbowWave();
      FastLED.show();
      
    } else if (idling && idlePos < 1) { //Not running a visualization or idle animation (either finished an idle or just stopped getting sound), just fade out the strip and bass lightsthis cycle
      fadeToBlackBy(leds, STRIP_LENGTH, 30);
      FastLED.show();
  
      if (bassBrightness > 0) { bassBrightness *= 0.8; analogWrite(PIN_BASS_LIGHT, bassBrightness); } //Did we not completely fade out the bass light during the last idle cycle? Let's keep fading it out here
    }
  
  }
  
  for (int i = 0; i <= maxKeycode; i++) { //Handle keypresses!
    bool buttonState = !digitalRead(keyIOPins[i]); //Read the state of this button, and invert it (they're active low)
    if (buttonState && !keyIOPressed[i]) { //Falling edge: We just pressed this button!
      if (keyIOCodes[i] < 0) { //Negative keycodes - special handling (a.k.a. do some volume control stuffs
        Keyboard.press(KEY_F3); //Volume is handled in SM by holding F3 and pressing R/T. Press F3 and pulse R/T here, and release F3 on volume button release
        switch (keyIOCodes[i]) {
          case -1:
            Keyboard.press('r');
            Keyboard.release('r');
            break;
          case -2:
            Keyboard.press('t');
            Keyboard.release('t');
            break;
        }
      } else {
        Keyboard.press(keyIOCodes[i]);
      }
      keyIOPressed[i] = true;

      
    } else if (!buttonState && keyIOPressed[i]) { //Rising edge: We just released this button!
      if (keyIOCodes[i] < 0) { //Negative keycodes - special handling (a.k.a. release F3 for volume control)
        Keyboard.release(KEY_F3);
      } else {
        Keyboard.release(keyIOCodes[i]);
      }
      keyIOPressed[i] = false;
    }
  }
  
  if (curMillis - lastHeartbeatFlipMs >= 500) { //500ms since the heartbeat LED toggled states, toggle it now!
    heartbeatState = !heartbeatState;
    lastHeartbeatFlipMs = curMillis;
    digitalWrite(LED_BUILTIN, heartbeatState);
  }

  /*for (int i = 0; i <= 3; i++) { //Handle smooth fading for the marquee lights   //Edit: Cab said no, begone!
    if (marqueeOn[i] && marqueeBrightness[i] < 255) { //If we still need to fade on this light...
      marqueeBrightness[i] = min((int)marqueeBrightness[i] + 40, 255); //...increment its brightness (but cap it at 255)
      Serial.println(marqueeBrightness[i]);
      analogWrite(pinLightsMarquee[i], marqueeBrightness[i]);
      
    } else if (!marqueeOn[i] && marqueeBrightness[i] > 0) { //What if we need to fade OFF this light?
      marqueeBrightness[i] = max((int)marqueeBrightness[i] - 40, 0); //...decrement its brightness (but prevent it from going below 0!)
      analogWrite(pinLightsMarquee[i], marqueeBrightness[i]);
    }
  }*/
  
  delay(10);
}
