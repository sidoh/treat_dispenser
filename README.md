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
