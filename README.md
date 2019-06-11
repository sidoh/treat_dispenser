# Treat Dispenser [![Build Status](https://travis-ci.org/sidoh/treat_dispenser.svg?branch=master)](https://travis-ci.org/sidoh/treat_dispenser)

Software for an ESP32-driven, Internet-connected pet treat dispenser.

## Overview

This software is very barebones.  It exposes a minimal REST API to configure and interact with it, and not much else.  The core components are:

* *Camera*.  Get a snapshot or MJPG stream of the camera.
* *Audio*.  Using ESP8266Audio and the onboard ESP32 DAC, play an MP3 audio file.
* *Motor*.  Control a stepper motor to dispense treats.
* *Settings*.  Configure the device.

## REST Endpoints

### Settings

Inspect and update settings.

* Get a JSON blob of settings: `GET /settings`
* Patch settings blob: `PUT /settings`

### Camera

Get images from the camera.

* Get a snapshot from the camera: `GET /camera/snapshot.jpg`
* Get an MJPG stream from the camera: `GET /camera/stream.mjpg`

### Audio

Manage audio files that are stored on flash.

* Get a list of audio files: `GET /sounds`
* Upload a new audio file: `POST /sounds`
* Get the contents of a particular file: `GET /sounds/:filename`
* Delete a particular file: `DELETE /sounds/:filename`

### Audio Commands

Play audio files

* Play an audio file: `POST /audio/commands`\
  Example body: `{"file":"/s/my_audio_file.mp3"}`

### Motor Commands

Control the motor

* Send a command to the motor controller: `POST /motor/commands`\
  Example body: `{"type":"dispense"}`

### About

Retrieve system information

* Get system informat: `GET /about`

## Default Pin Mappings

There's a really sloppy Fritzing diagram checked into the project.  Otherwise, here are some pin mappings:

### Camera

The ArduCAM module is hooked up to the default I2S and VSPI bus on the ESP32.

| **ArduCAM**  | **ESP32**  |
|---|---|
|  CS | GPIO5 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| SCK | GPIO18 |
|GND | Gnd |
| VCC | 3.3v |
| SDA | GPIO21 |
| SCL | GPIO22 |

### Audio

We use the builtin DAC (channel 1) and a Class D amplifier:

| **Class D Amp** | **ESP32** |
|---|---|---|
| Audio In- | Gnd |
| Audio In+ | GPIO25 |
| Enable | GPIO16 |
| Gnd | Gnd |
| Vin | 5v |

| **Class D Amp** | **Speaker** |
|---|---|
| Audio Out- | Gnd |
| Audio Out+ | Vin |

### Motor

| **A4988** | **ESP32** | Notes |
|--|--|--|--|
| VMOT | N/A | Connect to 12v w/ inline 100 uF capacitor
| Gnd | Gnd | |
| VDD | 5v | |
| ~EN | GPIO16 | |
| MS1 | 17 | |
| MS2 | 26 | |
| MS3 | 27 | |
| STEP | 14 | |

Bipolar motors have two positive/negative pairs.  You can find them by connecting two arbitrary wires and checking if you can turn the motor shaft with your hand.  If you can't, or if there's a lot of resistance, you've got a pair.  If not, pair it with one of the other wires.

Connect one pair to 1A and 1B, and the other to 2A and 2B on the A4988.