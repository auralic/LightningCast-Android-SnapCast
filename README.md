# Snapcast
## This repository is a fork of [badaix/snapcast](https://github.com/badaix/snapcast).

![Snapcast](doc/Snapcast_800.png#gh-light-mode-only)
![Snapweb-Dark](doc/Snapcast_800_dark.png#gh-dark-mode-only)

**S**y**n**chronous **a**udio **p**layer

[![CI](https://github.com/badaix/snapcast/actions/workflows/ci.yml/badge.svg)](https://github.com/badaix/snapcast/actions/workflows/ci.yml)
[![Github Releases](https://img.shields.io/github/release/badaix/snapcast.svg)](https://github.com/badaix/snapcast/releases)
[![GitHub Downloads](https://img.shields.io/github/downloads/badaix/snapcast/total)](https://github.com/badaix/snapcast/releases)
[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.me/badaix)

Snapcast is a multiroom client-server audio player, where all clients are time synchronized with the server to play perfectly synced audio. It's not a standalone player, but an extension that turns your existing audio player into a Sonos-like multiroom solution.  
Audio is captured by the server and routed to the connected clients. Several players can feed audio to the server in parallel and clients can be grouped to play the same audio stream.  
One of the most generic ways to use Snapcast is in conjunction with the music player daemon ([MPD](http://www.musicpd.org/)) or [Mopidy](https://www.mopidy.com/).
