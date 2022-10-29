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

  #ifdef EXPANSION_SHIFT_REGISTERS
  pinMode(PIN_LIGHTS_LAT, OUTPUT);
  pinMode(PIN_LIGHTS_DAT, OUTPUT);
  pinMode(PIN_LIGHTS_CLK, OUTPUT);
  #else
  pinMode(PIN_P1_MENU, OUTPUT);
  pinMode(PIN_P2_MENU, OUTPUT);
  #endif

  pinMode(PIN_P1_UP, OUTPUT);
  pinMode(PIN_P1_DOWN, OUTPUT);
  pinMode(PIN_P1_LEFT, OUTPUT);
  pinMode(PIN_P1_RIGHT, OUTPUT);

  pinMode(PIN_P2_UP, OUTPUT);
  pinMode(PIN_P2_DOWN, OUTPUT);
  pinMode(PIN_P2_LEFT, OUTPUT);
  pinMode(PIN_P2_RIGHT, OUTPUT);

  pinMode(PIN_P1_START, OUTPUT);
  pinMode(PIN_P1_DOWN, OUTPUT);
  pinMode(PIN_MARQUEE_LR, OUTPUT);
  pinMode(PIN_MARQUEE_UR, OUTPUT);
  pinMode(PIN_MARQUEE_LL, OUTPUT);
  pinMode(PIN_MARQUEE_UL, OUTPUT);

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
    lastPaletteSwap = curMillis;
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
    //Serial.println(curMillis - lastUpdateMillis); //Debug: Print update rate
    /*Serial.print(" ");
    Serial.print(AudioProcessorUsage());
    Serial.print(" ");
    Serial.println(AudioMemoryUsage());*/
    lastUpdateMillis = curMillis;


#ifdef USE_PRESET_PALETTES //If using preset color palettes, check if we recently swapped between palettes
    if (curMillis <= lastPaletteSwap + 150) { //If so, we should be fading between the old and the new colors
      short transProgress = ((float)(curMillis - lastPaletteSwap) / 150) * 255;
      //Serial.println(transProgress);
      for (int i = 0; i <= 3; i++) {
        curPalette[i] = blend(lastPalette[i], palettes[curPaletteIndex][i], transProgress); //Blend between the last and current palettes over the course of our fade duration (100ms)
      }
      
    } else { //If we're not fading between the old/new palettes, waver the current one's hue
      int targetAnimLength = bassKickLengthAverage.getAverage() * 2; //First find out how long this waver should last (2x length between bass kicks)
      float waverPos = ((float)(curMillis - lastHueWaverMillis) / targetAnimLength) * PI; //Next, our position into the waver sine wave...
      if (waverPos >= 2 * PI) { lastHueWaverMillis = curMillis; } //(Also set the last time we completed a waver if we've just completed a waver)
      short waverVal = sin(waverPos) * HUE_WAVER_STRENGTH; //...and finally use sine to calculate how much we should offset the hue
      /*Serial.print(waverPos);
      Serial.print(" ");
      Serial.println(waverVal);*/
      for (int i = 0; i <= 3; i++) {
        curPalette[i].h += waverVal; //Now modify the current palette's hue based on our calculated value
      }
    }
    updateAltPalettes();
#endif

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
      if ((idling && idlePos < 1) || !idling) { //If we're not idling or idling without playing an idle animation, update the bass strip
        updateBassStrip();
      }
      
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
