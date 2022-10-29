# IceBeats Music Visualizer
*"it's like a firebeat, but **cooler**"*

## Introduction
IceBeats is a Teensy-based lights driver board that does two things:
 - Works with StepMania to control cabinet and pad lights in a 573-era DDR cabinet.
 - Real-time, hardware independent music visualization across two LED strips.

The hardware is designed for minimal components/configuration for an embedded environment.
Grab a Teensy 3.2, a few addressable LED strips, and a few basic components, and you're done! No bloated bluetooth/wifi control or complex hardware setup required.



## Features
Code support for these are built-in - all 99% configurable, use any combination you like!

 * Visualization addressable LED strip - An addressable RGB LED strip for visualizations.
 * Bass addressable LED strip - Another addressable RGB LED strip, blinks to the beat.
 * Bass single-color LED strip/strand - Designed for a single-color LED strip, or low voltage LED string lights. Blinks to the beat.
     * Supports two-wire LED strands, where alternate LEDs/colors are driven by reversing the strip polarity using a H-Bridge, like the L293D. These are cheap and readibly available off places like Amazon.

 * Accepts audio input via RCA/mono 3.5mm jack (or with modification, any other input the Teensy audio library supports)
 * Designed around Teensy 3.2, other Audio-capable Teensy boards likely compatible (the 4.0 is untested but may work)
 
 * FFT-based music visualization using the Teensy Audio Library
 * Multiple visualization effects and color palettes. Optional dynamic, music-based color palettes available
 * Idle animations for when no music is playing
 
 * Drives 573-era DanceDanceRevolution cabinet/pad lighting (reads SextetStream-formatted data output by StepMania or other programs).
 * PCB design


## Hardware Setup
**Pin numbers and LED strip lengths can ALL be configured in `IceBeats.ino` - the settings are at the top!**

### Audio Input
Audio input is taken via a 3.5mm/RCA jack. [Wiring instructions can be found on the Teensy Audio Design Tool](https://www.pjrc.com/teensy/gui/?info=AudioInputAnalog)'s info panel for AudioInputAnalog.


### Visualization LED strip:
The main attraction, music visualization on an addressable LED strip!

Any WS2812/NeoPixel strip will do here (default: 54 pixels long, but can be configured for shorter/longer strips).
Excessive pixel count may cause performance drops.


### Bass LED strip:
This blinks to the beat with a strip-wide gradient, occasionally changing colors.

Again, use any WS2812/NeoPixel strip (default: 128 pixels. This can be made shorter/longer too)


### Bass light strand:
This one just blinks to the beat! You've got a few options for this:

For a single-color LED strip/string, one PWM pin is used for setting the strip's brightness, just hook it up to a mosfet/transistor and you're good to go.
  
Some cheap LED strings on Amazon will control alternate LEDs, or different LED colors by reversing the strip polarity.
If you have one of these strings, a L293D can be used to drive the strip at two different polarities to turn on alternate LEDs/colors.
Another pin can optionally connect to a L293D to do this. You will need to invert that pin's signal via a transistor, inverter, etc to drive the second input on the L293D.


### StepMania pad/cab light control:
This code accepts input over serial for any games/tools that use the SextetStream protocol for driving DDR cabinet/pad lights.

In the case of StepMania, this means either using the SextetStreamToFile (Linux) or Win32Serial (Windows) lights driver.

The light states are sent to the rest of the Teensy's IO pins. Ideally, these should connect to the DDR cabinet through opto isolators. No transistor/mosfet is needed here, the stock DDR cabinet amp handles the high-current loads.

Lights data can optionally be output to three shift registers, as well, if you need more light outputs than the Teensy has pins.


## Todo:
 There's a lotta things that could be improved
 * Better documentation/wiring instructions
 * A few bugfixes with idle animations on the main led strip
 * Better beat detection
 * Getting the visualizations to look like they reflect the music a bit closer
 * More color palettes.

## License
I'd greatly apprecate it if you credit/link back to this repo if you use this code somewhere - this thing took a TON of time to make.

That being said,

¯\\_(ツ)_/¯