# nuru-view

Display [nuru images](https://github.com/domsson/nuru) on your Terminal.

## Status / Overview 

Early work in progress. Prototype alpha stage kinda stuff. Don't use yet.

## Dependencies / Requirements

- Terminal that supports 256 colors ([8 bit color mode](https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit))
- Requires `TIOCGWINSZ` to be supported (to query the terminal size)

## Building / Running

You can compile it with the provided `build` script.

    chmod +x ./build
    ./build

## Usage

    nuru-view [OPTIONS...] image-file

Options:

  - `-b`: custom background color (not yet implemented)
  - `-f`: custom foreground color (not yet implemented)
  - `-h`: print help text and exit
  - `-p FILE`: palette file to use
  - `-V`: print version information and exit

## Support

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/L3L22BUD8)

