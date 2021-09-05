# IceBeats Music Visualizer
*"it's like a firebeat, but **cooler**"*

## Introduction
IceBeats gives you real-time music visualization on an addressable LED strip.

The hardware is dirt simple, and designed for to literally just be plug n play. Grab a Teensy 3.2, an addressable LED strip, and maybe a few basic components, and you're done! No bloated bluetooth/wifi control or complex setup (well, kinda).


*(Sidenote, there IS a TON of 100% optional bloat instead! If you're planning on throwing this on a StepMania cabinet (like I am), this code can optionally drive your cabinet lights as well. It'll still work just fine if you don't need these extra features.)*



## Lighting Hardware
Code support for these are built-in - all 99% configurable, use any combination you like!

 * Visualization addressable LED strip - An addressable RGB LED strip for visualizations. Designed for 60-90 leds, more can be configured but may result in performance drops. 
 * Bass addressable LED strip - Another addressable RGB LED strip, blinks to the beat (automagically switches to SM control, if available)
 * Bass light strand - Designed for a single-color LED strip, or low voltage LED string lights. Blinks to the beat.
     * Single color strands/strips work fine, but you can optionally use this with two-wire strands where alternate LEDs/colors are driven by reversing the strip polarity using a H-Bridge, like the L293D. The code, with an L293D, will alternate between the two polarities/strings every other beat.

## Electronics Hardware
 * Accepts audio input via RCA/mono 3.5mm jack (or any other input Teensy's audio library supports, like USB, through easy mods)
 * Designed around Teensy 3.2, other Audio-capable Teensy boards likely compatible (The 4.0 doesn't look to be 100% compatible at time of writing)
 
## Visualization
 * FFT-based music visualization using the Teensy Audio Library
 * Multiple visualization effects and optional dynamic, music-based color palettes
 * Idle animations for when no music is playing
 
## StepMania IO
 * Drives StepMania cabinet/pad lighting (reads the standard SextetStream format via serial, configure either SextetStreamToFile (Linux) or Win32Serial (Windows, SM5.3). Other, non-Stepmania programs are untested but probably work.
 * Can send input via keystrokes (currently disabled, you REALLY should go buy a J-PAC instead :P )


## Hardware Setup
**Pin numbers and LED strip lengths can ALL be configured in `IceBeats.ino` - the settings are at the top!**

### Audio Input
Audio input is taken via a 3.5mm/RCA jack. [Wiring instructions can be found on the Teensy Audio Design Tool](https://www.pjrc.com/teensy/gui/?info=AudioInputAnalog)'s info panel for AudioInputAnalog.


### Visualization LED strip:
The main attraction, music visualization on an addressable LED strip!

Any WS2812/NeoPixel strip will do here (default: pin 9, 54 pixels long, but you can easily drive 90+ without issue).
Excessive pixel count may cause performance drops.


### Bass LED strip:
This blinks to the beat with a nice strip-wide gradient, occasionally changing colors.

Again, use any WS2812/NeoPixel strip (default: pin 5, 128 pixels. This can be made longer too)


### Bass light strand:
This one just blinks to the beat! You've got a few options for this:

For a single-color LED strip/string, one PWM pin is used for setting the strip's brightness (default: pin 6), just hook it up to a mosfet/transistor and you're good to go.
  
Some cheap LED strings on Amazon will control alternate LEDs, or different LED colors by reversing the strip polarity.
If you have one of these strings, a L293D can be used to drive the strip at two different polarities to turn on alternate LEDs/colors.
Two pins (default: 7/8) can optionally connect to a L293D to do this.


### StepMania pad/cab light control:
This code accepts input over serial for any games/tools that use the SextetStream protocol for driving DDR cabinet/pad lights.

In the case of StepMania, this means either using the SextetStreamToFile (Linux) or Win32Serial (Windows) lights driver.

The light states are sent to 3 shift registers (default pins: clock 2, data 4, latch 3). The first controls pad lights, the second/third cabinet and menu button lights. 


## Todo:
 There's a lotta things that could be improved
 * A few bugfixes with idle animations on the main led strip
 * Better beat detection
 * Getting the visualizations to look like they reflect the music a bit closer
 * More color palettes.

## License
I'd greatly apprecate it if you credit/link back to this repo if you use this code somewhere - this thing took a TON of time to make.

That being said,

¯\\_(ツ)_/¯