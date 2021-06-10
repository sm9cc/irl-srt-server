# SRT Live Server

## Introduction

srt-live-server (SLS) is an open source live streaming server for low latency based on Secure Reliable Tranport (SRT).
Normally, the latency of transport by SLS is less than 1 second in internet.

## Requirements

Please install the SRT library first, refer to SRT(https://github.com/Haivision/srt) for system enviroment.
SLS can only run on OS based on linux, such as mac, centos or ubuntu etc.

## Compilation

```bash
mkdir build && cd build
cmake ../
make -j
```

Binaries are created in `build/` directory.

## Directivies

Regarding the config file, please see the wiki:
https://github.com/Edward-Wu/srt-live-server/wiki/Directives

## Usage

`cd build`

### Help information

```bash
./srt_server -h
```

### Run with default configuration file

```bash
./srt_server -c ../sls.conf
```

## Testing

srt-live-server only supports the MPEG-TS format streaming. 

### Test with FFmpeg

You can push camera live stream using FFmpeg. FFmpeg must be compiled with `--enable-libsrt` flag - to obtain appropriate binaries, download FFmpeg sourcecode from https://github.com/FFmpeg/FFmpeg, then compile FFmpeg with `--enable-libsrt`.

`srt` library is installed in folder `/usr/local/lib64`.

If `ERROR: srt >= 1.3.0 not found using pkg-config` occurs during the compilation of FFmpeg, please check the `ffbuild/config.log` file and follow its instruction to resolve this issue. In most cases it can be resolved by executing the following command:

```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
```

If `error while loading shared libraries: libsrt.so.1` occurs, please add `srt` library path to the runtime linker configuration file, `/etc/ld.so.conf`, then refresh the cache by running the comand `/sbin/ldconfig` as root.

#### Push stream from webcam to SRT

```bash
./ffmpeg -f avfoundation -framerate 30 -i "0:0" -vcodec libx264  -preset ultrafast -tune zerolatency -flags2 local_header  -acodec libmp3lame -g  30 -pkt_size 1316 -flush_packets 0 -f mpegts "srt://[your.sls.ip]:8080?streamid=uplive.sls.com/live/test"
```

#### Play a SRT stream using FFplay

```bash
./ffplay -fflags nobuffer -i "srt://[your.sls.ip]:8080?streamid=live.sls.com/live/test"
```


### Test with OBS

OBS supports SRT protocol to publish streams from version `v25.0` onwards. To publish SRT stream from OBS to SRT Live Server you can use the following url:

```
srt://[your.sls.ip]:8080?streamid=uplive.sls.com/live/test
```

You can also add a SRT stream as an input source. To do this, add a `Media source` to OBS, enter `mpegts` as input format and set the following input URL:

```
srt://[your.sls.ip]:8080?streamid=live.sls.com/live/test
```

### Test with SRT Live Client

There is a test tool in SLS which can be used as a performance test - it has no codec overhead, only network overhead. The SRT Live Client can play a SRT stream to a TS file, or push a TS file to a SRT stream.

#### Push a TS file via SRT

```bash
./srt_client -r srt://[your.sls.ip]:8080?streamid=uplive.sls.com/live/test -i [the full file name of exist ts file]
```

#### Play a SRT stream

```bash
./srt_client -r srt://[your.sls.ip]:8080?streamid=live.sls.com/live/test -o [the full file name of ts file to save]
```

## Use SLS with docker

Please refer to: https://hub.docker.com/r/ravenium/srt-live-server

## Note:

- SLS refers to the RTMP url format (domain/app/stream_name), example: www.sls.com/live/test. The URL must be set in streamid parameter of SRT, which will be the unique identification a stream.

- How to distinguish the publisher and player of the same stream? In the configuration file file, you can set parameters of domain_player/domain_publisher and app_player/app_publisher to resolve it. Importantly, the two combination strings of domain_publisher/app_publisher and domain_player/app_player must not be equal in the same server block.

- I supplied a simple android app for testing SLS, which can be downloaded from https://github.com/Edward-Wu/liteplayer-srt
