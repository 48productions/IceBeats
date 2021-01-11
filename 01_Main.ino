/********************
 *   01_MAIN.INO    *
 *******************/

 /**
  * Functions for setup and loop
  */


void setup() {
  //Keyboard.begin();
  
  AudioMemory(10);
  fft.windowFunction(AudioWindowHamming1024);
  
  Serial.begin(9600);
  LEDS.addLeds<WS2812, STRIP_DATA, GRB>(leds, STRIP_LENGTH);
  LEDS.addLeds<WS2812, STRIP_BASS_DATA, GRB>(ledsBass, STRIP_BASS_LENGTH);
  LEDS.setBrightness(94);

  pinMode(PIN_DEBUG_0, INPUT_PULLUP); //Pinmode ALL the pins
  pinMode(PIN_DEBUG_1, INPUT_PULLUP);
  pinMode(PIN_DEBUG_2, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(STRIP_DATA, OUTPUT);
  pinMode(PIN_BASS_LIGHT, OUTPUT);
  pinMode(PIN_BASS_SEL_1, OUTPUT);
  pinMode(PIN_BASS_SEL_2, OUTPUT);
  
  pinMode(PIN_LIGHTS_LAT, OUTPUT);
  pinMode(PIN_LIGHTS_DAT, OUTPUT);
  pinMode(PIN_LIGHTS_CLK, OUTPUT);

  /*for (int i = 0; i <= 3; i++) { //Pinmode all the marquee lights
    pinMode(pinLightsMarquee[i], OUTPUT);
  }*/
  
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
    updateSerialLights();
  }

  if (curMillis >= lastUpdateMillis + 16) { //Time for an update, gotta keep a 60fps update rate
    /*Serial.print(curMillis - lastUpdateMillis); //Debug: Print update rate
    Serial.print(" ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" ");
    Serial.println(AudioMemoryUsage());*/
    lastUpdateMillis = curMillis;

    /*for (int i = 0; i <= 3; i++) { //Handle smooth fading for the marquee lights
      if (marqueeOn[i] && marqueeBrightness[i] < 1) { //If we still need to fade on this light...
        marqueeBrightness[i] = min(marqueeBrightness[i] * 2.75, 1); //...increment its brightness (but cap it at 1)
        //Serial.println(marqueeBrightness[i]);
        //analogWrite(pinLightsMarquee[i], marqueeBrightness[i] * 255); //My cab said no to smooth fading marquee lights, so writing to pins is disabled but the rest of the code is left enabled since VE Pulse uses these values
        
      } else if (!marqueeOn[i] && marqueeBrightness[i] > 0) { //What if we need to fade OFF this light?
        marqueeBrightness[i] = max(marqueeBrightness[i] * 0.6, 0.01); //...decrement its brightness (but prevent it from going below 0)
        //analogWrite(pinLightsMarquee[i], marqueeBrightness[i] * 255);
      }
    }*/
    
    if (lightTestEnabled) { //In the lighting test, update some lights boi
      updateLightTest();
      
    } else { //Not in the lighting test, update visualizations/idles/etc
      updateBassStrip();
      
      if (peak.available()) { //We have peak data to read! READ IT!
        updatePeak();
      }
    
      if (!idling && fft.available()) { //FFT has new data and we aren't idling, let's visualize it!
        updateVisualization();
        
      } else if (idling && idlePos >= 1 && curMillis - lastIdleUpdateMillis >= idleUpdateMs) { //We're idling, playing the idle animation, and it's time for an update!
        updateIdleAnim();
        
      } else if (idling && idlePos < 1) { //Not running a visualization or idle animation (either finished an idle or just stopped getting sound), just fade out the strip and bass lightsthis cycle
        updateIdle();
      }
    }
  }
  
  /*for (int i = 0; i <= maxKeycode; i++) { //Handle keypresses!
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
  }*/

  /*Serial.print(curMillis);
  Serial.print(" ");
  Serial.print(lastHeartbeatFlipMs);
  Serial.print(" ");
  Serial.println(lastUpdateMillis);*/
  
  if (curMillis - lastHeartbeatFlipMs >= 500) { //500ms since the heartbeat LED toggled states, toggle it now!
    heartbeatState = !heartbeatState;
    lastHeartbeatFlipMs = curMillis;
    digitalWrite(LED_BUILTIN, heartbeatState);
  }

  
  //delay(10);
}
