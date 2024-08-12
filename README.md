# ndi-utils

Simple utilities to assist with testing and experimenting with NDI.

## Building

Download and install the NDI Advanced SDK.  I typically install the include
files in `/usr/local/include` and the libraries in `/usr/local/lib`.

Then just:
```
make
sudo make install
```

## nditx

The `nditx` utility reads 16-bit P216 video from stdin and transmits it as an
NDI stream.  The Advanced SDK interface is used which allows specifying a
bitrate multiplier and optional forcing of 4:2:2 or 4:2:0 sampling modes.

The `ffmpeg` utility can be used to playback media files, writing the raw video
frames to stdout which can then be piped to nditx.

When working with clip files instead of continuous video streams, the `-w`
switch can be used to make the `nditx` utility wait until an NDI receiver
connectes before starting to send frames.  This prevents the loss of a few
frames from the beginning of the clip when the NDI receiver and transmitter are
negotiating a connection.

```
# Example playback of a v210 mov file using ffmpeg and nditx
# Send using default settings (100% bitrate and auto 422/420 selection)
ffmpeg -stream_loop -1 -i ~/crowdrun-1080p50-v210.mov -f image2pipe -vcodec rawvideo -pix_fmt p216le - | \
nditx/nditx -r 50/1 -b 100 -s auto
```

## ndirx

The `ndirx` utility receives an NDI stream, decodes it, and writes the raw video
frames to stdout as 16-bit P216 data.  Separate threads are used for receiving
the NDI frames and writing the raw frames to sdtout, so frames are not dropped
if the file writing process can't keep up with real time, as long as the video
clip is short enough to not exhaust all of your system memory.

The `ffmpeg` utility can be used to record these frames to any supported format,
but for quality testing uncompressed formats such as v210 are preferred.

```
# Example recording of a v210 mov file using ndirx and ffmpeg
ndirx/ndirx -s "NDI_Source (channel)" -c 1000 | \
ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt p216le -s 1920x1080 -r 50 -i - \
-movflags write_colr -c:v v210 -color_primaries bt709 -color_trc bt709 \
-colorspace bt709 -color_range tv \
-metadata:s:v:0 "encoder=Uncompressed 10-bit 4:2:2" -c:a copy -vf \
setdar=16/9 -f mov /tmp/output.mov
```

## nditest.sh

The `nditest.sh` utility is a simple shell script which automates testing of
multiple generations of NDI transmission.  Video clip details are extracted from
the source clip and used to run the `nditx` and `ndirx` utilities with the frame
count flag to generate a "copy" of the video clip after being passed through an
NDI signal path.

This process is then optionally repeated, using the just
recorded video clip as the new source.  The target bitrate multiplier and
SpeedHQ sampling mode can optionally be set to allow easy creation of a matrix
of test points for quality evaluation.

Log files are created for each video clip and stored in the same directory (with
the extensions `.nditx.log` and `.ndirx.log`) for each video clip processed.

```
# Example to create 10 generations of video clips for quality testing
nditest.sh -i ~/crowdrun-1080p50-v210.mov -o /tmp/nditest.v210.b100.auto -b 100 -s auto -g 10
```

## ffmpeg

The `ffmpeg` utility needs to be new enough to support the required p216le pixel
format.  Version 5.1.6 included with Debian 12 (Bookworm) works fine, but
version 4.3 included with Debian 11 (Bullseye) is too old.
