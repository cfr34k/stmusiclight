# STMusiclight

Sound2Light for the STM32F4Discovery board.

See [this blog post](http://blog.tkolb.de/archives/8-Musiclight.html) for a
description and demo video (it’s on a different platform, but the same
algorithm).

The audio is sampled from an external analog microphone (with amplifier) using
the ADC or from the on-board MEMS microphone (onboard_mic branch). The analog
microphone version potentially produces much cleaner results, depending on your
setup.

You may use this code under the terms of the GPL version 3.

© 2017 Thomas Kolb
