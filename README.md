# nRF5340 Audio DK - Hello Codec

This example is the bare minimum code needed to initialize, configure, and enable the CS47L63 hardware codec. It plays a short sine wave sample using the I2S TX buffer, and then briefly mixes in the codec's tone and noise generators as well.

This code is liberally commented and extremely verbose, so it should be easy to understand, and it uses legible constants instead of indecipherable hex whenever possible to help self-document the written register values, simplifying experimentation with other available codec features not included in this trivial demo.

## Overview

The `/src/drivers/` directory is copied verbatim from the nRF Connect SDK's [nrf5340_audio](https://github.com/nrfconnect/sdk-nrf/blob/main/applications/nrf5340_audio) application, and contains only those files needed to initialize and communicate with the CS47L63.

All other functionality is implemented in a single `main.c` file, demonstrating the basic steps necessary to configure and enable the codec. It mostly uses direct register manipulation, along with a few other functions from Cirrus Logic's `cs47l63.c` driver to configure the complicated frequency-locked loop peripheral.

`main.c` consists of three distinct sections: **NRFX&nbsp;CLOCKS**, **NRF&nbsp;I2S**, and **HW&nbsp;CODEC**. These are suggested separation points for dividing the application into dedicated modules, but are consolidated here to facilitate quick comprehension and reduce the number of files to read.

## References

- [nRF5340 Audio DK User Guide](https://docs.nordicsemi.com/bundle/ug_nrf5340_audio/page/UG/nrf5340_audio/intro.html)

- [CS47L63 Datasheet (PDF)](https://statics.cirrus.com/pubs/proDatasheet/CS47L63_DS1249F2.pdf)
