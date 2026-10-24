# Glance

[繁體中文](README.md) | **English**

**A calendar that lives in a picture frame.** Glance is an e-paper display that shows today's date, the weather and your next ten events -- and nothing else. It updates itself once or twice a day, runs for a long time on one charge, and looks like a print on the shelf rather than a gadget.

<p align="center"><img src="docs/images/glance.jpg" alt="Glance in a wooden frame, showing the picture it shows before its first setup" width="480"></p>

## What it shows

| Calendar | Privacy mode |
|---|---|
| ![The calendar screen: date, weather and the next ten events](docs/images/screen-calendar.png) | ![Privacy mode: the date, a photo, and a guest WiFi QR code in the corner](docs/images/screen-photo.png) |
| Today's date, the day's weather, and the next ten events from up to five calendars (Google Calendar, iCloud, or anything with an iCal link). Just the date and the title: the details are on your phone. | Guests coming over? One press of the button swaps the events for a photo. Upload your own from your phone, or keep the two Hokusai prints it comes with. The corner shows the weather, or a QR code your friends scan to join your WiFi. |

A few more things it can do:

-   **A photo frame**: set up no calendar at all, and it shows your photos with the date and weather.
-   **Guest WiFi**: in privacy mode it can show a QR code your friends scan to join your WiFi (ideally your router's guest network).
-   **Dark or light**: white on black or black on white, and photos get whichever background sets them off.
-   **Your refresh times**: 07:00 by default, or any time you like, plus an optional second one in the evening -- which then shows tomorrow's weather.
-   **Keeps itself up to date**: new firmware arrives on its own, over WiFi.
-   **Tells you only when something's wrong**: a small icon appears if it can't get online, a calendar stops working, or the battery runs low.

## Using it

**Setting up** takes a phone and a couple of minutes, and the panel shows what to do without words:

1.  Press the button on the back. A QR code appears: scan it with your phone's camera to join Glance's own WiFi, then pick your home WiFi on the page that opens.
2.  A second QR code appears. Switch your phone back to your home WiFi (so it has internet), scan it, and paste your calendar links, choose your county for the weather, and upload photos if you like.
3.  That's it. Glance draws your calendar and goes to sleep until the next refresh.

**The button on the back:**

| Press | What happens |
|---|---|
| A short press | Switches between the calendar and privacy mode |
| Hold for about 5 seconds | Opens the setup page again (to change calendars, WiFi, photos...) |
| Hold for about 20 seconds | Erases everything, for giving it to someone else -- or as a way out if the setup page can't be reached |

The timing needn't be exact: letting go anywhere from 3 to 15 seconds opens the setup page, from 15 to 30 seconds erases, and past 30 seconds the button is taken to be stuck and nothing happens. Counting to 5 or 20 lands safely in the middle, even when the frame hides the LED.

**Charging** is through USB-C. A battery icon appears about two months before it runs out; if it does run out, the panel asks to be charged and Glance picks up where it left off once it has been.

## Battery life

Glance is asleep almost all the time and only wakes up to draw the panel, which keeps what it holds even with no power.

| | |
|---|---|
| Asleep | about 27 µA |
| One refresh (about 12 seconds) | about 0.16 mAh |
| A day with one refresh | about 0.8 mAh |

With a 1000 mAh battery that works out to more than three years on paper. Real batteries slowly lose charge on their own and photos take a little more, so expect **well over a year** per charge; a long-term test running since October 2026 will pin the number down.

## Privacy

Glance talks only to your calendar links, the weather service, a time server and GitHub (for updates); there's no account and no cloud service of its own. Your WiFi password and calendar links stay on the device, though they aren't encrypted there, and the setup page on your home WiFi is plain HTTP guarded by a one-time code. The details, and what Glance doesn't protect against, are in the [firmware notes](firmware/README.md#security-notes).

## Build your own

You'll need the Glance board, a Waveshare 7.5" e-paper panel (V2, 800×480, black and white), a 1000 mAh LiPo battery and a picture frame -- and optionally a free API key from Taiwan's Central Weather Administration for the weather.

-   [`hardware/`](hardware/README.md) -- the board: schematics, parts, power design and pins.
-   [`firmware/`](firmware/README.md) -- the software: building and flashing, how each feature works, and the security notes.

Glance is a personal project, built and used at home; it isn't sold anywhere.

---

## License

Copyright (c) 2025 Bo Lin

[![License: CC BY-NC-SA 4.0](https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.

You are free to:

- Share — copy and redistribute the material in any medium or format
- Adapt — remix, transform, and build upon the material

Under the following terms:

- Attribution — You must give appropriate credit to the original author (Bo Lin).
- NonCommercial — You may not use the material for commercial purposes.
- ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

No additional permissions are required for non-commercial reuse that respects these terms.

A copy of the full license is available at:
https://creativecommons.org/licenses/by-nc-sa/4.0/
