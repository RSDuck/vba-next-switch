# VBA Next/VBA-M for Switch

<img style="float: right;" src="icon.jpg">

A VBA-M port for Nintendo Switch. It's based of the [libretro port](https://github.com/libretro/vba-next)(the actual emulator) and
[3DSGBA](https://github.com/shinyquagsire23/3DSGBA)(the GUI, although heavily refactored).

After porting 3DSGBA(which often crashed probably because of a huge amount of memory leaks), I tried porting mGBA which ran not so well. That's why I decided to experiment with a lighter less accurate emulator, which lead to this port.

Needs [libnx](https://github.com/switchbrew/libnx) and [devkitPro](http://devkitpro.org/) to build. Just run `make`.

## Features

- Quite high compability(haven't tried many games)
- Save games and save states
- Frameskip

## Problems

- In rare occasions the audio has problems
- Video and Input not frame accurate(see Speed hack)

## Speed hack

NOTE: some things don't apply anymore or were inaccurate observations

Before implementing this "speed hack" the emulator had regular slowdowns. Although I found out about these things by myself, these things might be common knowledge to emulator devs. These were the things which apparently slowed the emulator down:
- The thread/core it's running on
- The VSync

The first problem was solved by running the whole emulator in a second thread with a very high priority pinned to a core not used by the system. 

Omitting the wait for vertical synchronisation lead naturally to artefacts. So I decided on only running the emulator inside the second thread, locked using sleep thread to 60 Hz. At the same time the main thread is locked by the vsync and only receives the framebuffer while sending the input. I left the audio in the emulator thread.

This leads of course to the problem that both threads, although locked to 60 Hz, may not run in sync, so the input or the graphics of a frame may be skipped or out of sync sometimes.

## Credits

- [jakibaki](https://github.com/jakibaki) and [dene-](https://github.com/dene-) who helped massively with their Pull Requests
- [BernardoGiordano](https://github.com/BernardoGiordano) for the great keyboard in [Checkpoint](https://github.com/BernardoGiordano/Checkpoint).
- VBA-M and [libretro devs](https://github.com/libretro/vba-next/graphs/contributors)
- shinyquagsire23 and Steveice10 for [3DSGBA](https://github.com/shinyquagsire23/3DSGBA)
- [libnx devs](https://github.com/switchbrew/libnx/graphs/contributors)