# oboe

## Introduction

Oboe is a C++ wrapper for the AAudio and OpenSL ES audio APIs on Android.

A program written using Oboe can run on multiple versions of Android.
On O or later versions, Oboe can use Audio or OpenSL ES.
On N or earlier versions, Oboe will only use OpenSL ES.

This is not an official Google product.

## Apps

Hello Oboe - simple tone generator

Native MIDI Synth - A MIDI synthesizer that renders in C++ and calls Oboe.
It uses the Android MIDI API in Java.

## How to Use

Include this as a sub-module in your Git project.

