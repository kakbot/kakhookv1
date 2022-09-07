# Cathook Training Software
[![C++](https://img.shields.io/badge/language-C%2B%2B-%23f34b7d.svg?style=flat-square)](https://en.wikipedia.org/wiki/C%2B%2B)
[![TF2](https://img.shields.io/badge/game-TF2-orange.svg?style=flat-square)](https://store.steampowered.com/app/440/Team_Fortress_2/)
[![GNU/Linux](https://img.shields.io/badge/platform-GNU%2FLinux-ff69b4?style=flat-square)](https://www.gnu.org/gnu/linux-and-gnu.en.html)
[![x86](https://img.shields.io/badge/arch-x86-red.svg?style=flat-square)](https://en.wikipedia.org/wiki/X86)
[![License](https://img.shields.io/github/license/explowz/cathook.svg?style=flat-square)](LICENSE)
[![Issues](https://img.shields.io/github/issues/explowz/cathook.svg?style=flat-square)](https://github.com/explowz/cathook/issues)

Free open-source GNU/Linux training software for the game **Team Fortress 2**. Designed as an internal cheat - [Shared Library](https://en.wikipedia.org/wiki/Library_(computing)#Shared_libraries) (SO) loadable into game process. Compatible with the Steam version of the game.

## Risk of VAC detection

The software could be detected by VAC in the future. Only use it on accounts you won't regret getting VAC banned.

## Overview

Cathook is a training software designed for Team Fortress 2 for Linux. Cathook includes some joke features like

* Ignore Hoovy
* Encrypted chat
* Nullnexus Support (Completely broked.)
* Sandvich aimbot
* Chance to get manually VAC banned by Valve

and a lot of useful features, including

* Hitscan nospread
* Navparser Bots (Walkbots that can walk on any map without manual configuration)
* Working crit hack (Work in Progress.)
* Backtrack
* Some Improvment and more.

[FULL LIST OF FEATURES HERE](https://github.com/nullworks/cathook/wiki)

# Install

1. Copy repo
2. make sure cathook folder are in your user folder.
3. Open Terminal type `./install-all`

# Update

Open Terminal of cathook folder and type `./update`

# Attach/Inject

Open Terminal while inside the source folder and type `sudo ./attach`

This will inject `libcathook.so`(like a .dll on Windows files but in linux) into the `hl2_linux` process.

and Press `INSERT` to Open/close menu

## how i have install my config intro cathook?

You can open in `/opt/cathook/data/configs` and put your .conf intro it.
