# IceBeats Music Visualizer
*"it's like a firebeat, but **cooler**"*

## Introduction
IceBeats is yet *another* Arduino sketch for real-time music visualization on an addressable LED strip, aiming for a simple setup that provides dynamic and intricate music visualizations. This project is aimed as a mod for Dance Dance Revolution cabinets (and can drive DDR cabinet lighting via an output from StepMania), but can be repurposed for any other kind of display.

It is designed around the Teensy Audio Library to run on the Teensy 3.2. Input is taken from an RCA audio jack, and visualized to a WS2812B LED strip. Other devices that can run the Teensy Audio library (like newer/fancier Teensy boards) should also be compatible.

**Keep in mind this project is still a work in progress!**


## Hardware
 * Accepts audio input via RCA/mono 3.5mm jack
 * Designed around Teensy 3.2, other Audio-capable Teensy boards likely compatible
 * Output to WS2812B LED Strip via FastLED (other strip types likely compatible)
 
## Visualization
 * FFT-based music visualization using the Teensy Audio Library
 * Multiple visualization effects and color palettes, cycled at random when music is quiet
 * Multiple idle animations to play when there is no music
 * Reads StepMania's SextetStream lighting format via serial to drive traditional DDR/ITG-style cabinet lights