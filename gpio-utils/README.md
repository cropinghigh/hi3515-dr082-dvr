# GPIO utilities

## Library

Since there is no official release for this library,
a generic version number ``0.1`` was used.

## Tools



## Building

First clone the repository:

    git clone git@github.com:RedPitaya/gpio-utils.git

To build the project do:

    meson builddir --buildtype release
    cd builddir
    ninja

Meson can be called with the option `--prefix /usr` intead of the dafault prefix `/usr/local`.

The library and tools can be installed into the default path using:

    ninja install
