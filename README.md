# SONOS::Squeezebox

This software lets you integrate Sonos players in a Logitech Media Server (Squeezebox) environment.

It builds on two excellent software projects, being Noson and Squeezelite. Noson is a C++ language library to control Sonos
equipment, primarily used in the Noson-app. Squeezelite is a headless C language client for LMS that is often used as a skeleton
for larger projects.

## Usage

```sh
./sonos-squeezebox [options] --room=<Room/Zone name>
```

* Connecting to Sonos. You specify the room or zone name of the player you want to control using the `--room` option. The application will
scan the network for Sonos players. If this fails or if the players are located in a separate network you may provide the the IP-address of
the Sonos player using the `--ip` option. This can be the IP-address of any player in the network as they generally find each other and provide
the software with a complete list of available players. You may need to open a port in the firewall to allow access from the Sonos box to the `sonos-squeezebox` software. The first instance of the software will be listening on port 1400, additional instances with use 1401, 1402, etc.

* Connecting to the Logitech Media Server (LMS). The application searches for the squeezebox server by scanning the network. If this fails or if the server is located in a separate network you may provide the server address and port using the `--server` option.

### Example

```text
$ ./sonos-squeezebox --ip=192.168.15.247 --room=Bibliotheek --server=127.0.0.1:9000


| SONOS::Squeezebox -- deploy Sonos in a Logitech Media Server (LMS) streaming environment
|
| Copyright (c) 2023 Martin van der Werff <github (at) newinnovations.nl>


Connecting to Sonos (through player 192.168.15.247) ... SUCCESS

+----------------------------------------------------------------------- devices / players ---+
| player name                         | uuid                     | host               |  port |
+---------------------------------------------------------------------------------------------+
| Bibliotheek                         | RINCON_000E5883160901400 | 192.168.15.243     |  1400 |
| Eetkamer                            | RINCON_5CAAFDF6210101400 | 192.168.15.247     |  1400 |
+---------------------------------------------------------------------------------------------+

+--------------------------------------------------------------------------- zones / rooms ---+
| room name                           | coordinating player                                   |
+---------------------------------------------------------------------------------------------+
| Bibliotheek                         | Bibliotheek                                           |
| Eetkamer                            | Eetkamer                                              |
+---------------------------------------------------------------------------------------------+

Connecting to room Bibliotheek ... SUCCESS (MAC = 00:0E:58:83:16:09)
```

## Building

### Requirements

```sh
apt-get install -y --no-install-recommends \
        make cmake g++ libz-dev libssl-dev libflac++-dev libpulse-dev \
        libasound-dev libvorbis-dev libfaad-dev libmad0-dev libmpg123-dev libsoxr-dev
```

### Cloning

```sh
git clone --recursive https://github.com/newinnovations/sonos-squeezebox.git
```

If you cloned without the `--recursive` option, you can initialize the sub-modules using:

```sh
git submodule update --init --recursive
```

### Compiling

```sh
make
```

## Technical challenges

* Sonos buffers a lot and causes latency issues with other software. Similar stuff happened to the pulseaudio support in Noson and the Noson-app. A different solution was chosen here. We throttle the encoder to not encode more than 2 seconds of music in the future. This also keeps the squeezebox server happy as it does not really understand minutes of music being consumed in mere seconds.

* Sonos sometimes gets greedy and requests the same stream twice. Streaming to multiple sinks is not supported in the software, so I my first idea was to cancel the first request and continue serving on the second. That doesn’t work. What works it to reject the second request, and continue serving on the first.

* Goal was to use squeezelite as much “out-of-the-box” as possible, by providing only a Sonos output module. But how to know when to start a (new) stream to the Sonos? Turned out that the output module receives a silent flag. Looking at transitions here is the solution. From silent to non-silent requires to start a new stream, from non-silent to silent needs to terminate the current stream which will stop the Sonos. While silent we do not stream silence over the network. During the playback of an album or playlist, the silent flag will not toggle between tracks so the stream continues nicely.

* Sonos does not support high quality streams,  but this is handled quite nicely by LMS. By only advertising support for 44k1 to the squeezebox server, streams are automatically sampled down by the server. No need for resampling in our client software.

* Squeezebox differentiates between different players using the MAC-address. Squeezelite by default uses the host mac, and we would run into problems controlling multiple players from the same host. We therefor use the player MAC-address and try to retrieve it from the UUID string (assuming it will be always in the form `RINCON_<MAC><PORT>`).

## Related software

* Philippe44 created the [LMS to UPnP bridge](https://github.com/philippe44/LMS-uPnP) which you may be able to use as Sonos supports UPnP.
