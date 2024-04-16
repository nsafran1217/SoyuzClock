# SoyuzClock
Recreation of the 744H clock that flew on Soyuz space capsules

This is meant to be an accurate replica of the Soyuz 744H clock as feature on CuriousMarc's youtube channel.

- `SoyuzControlBoard` contains the KiCAD files for the esp32 board. There are many components that are optional.

- `SoyuzLedBoard` contains the KiCAD files for the board that the LED display mount on.

- `Soyuz-esp32` contains the PlatformIO project that runs the clock

## Part numbers
- АЛС324А is the 7 segment LED display. They are available on eBay or at tubes-store.com. They have the same quality issues as found in the real clock.

- МТ-1 is the toggle switch. They are available on eBay.

- ПКн2-1 is the correct push button, however, they are hard to find in the west. I found some stores selling them in Ukraine and Russia, but each switch is at least $15 US, plus you need to convince the seller its worth their time to ship it to you.

- KM1-1 is the next best option. They are easily found on eBay. The push button is smaller than the real switch. This is what I am using for now.


# 
Thanks to Curious Marc for documenting this clock: https://www.curiousmarc.com/space/soyuz-clock-744h-digital

I made a recreation of the case in FreeCAD based HEAVILY on this FreeCAD design: https://grabcad.com/library/soyuz-spacecraft-clock-1

This design will be added to this repo when finalized.
