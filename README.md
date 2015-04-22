snsfopt
=======

SNSF set optimizer / timer, derived from snsf9x and gsfopt.

All ripped sets should be optimized before release, to remove unused code/data unrelated to music playback.

Downloads
---------

- [Latest release](https://github.com/loveemu/snsfopt/releases/latest)

Usage
-----

Syntax: `snsfopt [options] [-s or -l or -f or -r or -x or -t] [snsf files]`

### Options

`-T [time]`
  : Runs the emulation till no new data has been found for [time] specified.
    Time is specified in mm:ss.nnn format   
    mm = minutes, ss = seoconds, nnn = milliseconds

`-P [bytes]`
  : I am paranoid, and wish to assume that any data within [bytes] bytes of a used byte,
    is also used

`--offset [load offset]`
  : Load offset of the base snsflib file.
    (The option works only if the input is SNES ROM file)

#### File Processing Modes

`-f [snsf files]`
  : Optimize single files, and in the process, convert minisnsfs/snsflibs to single snsf files

`-l [snsf files]`
  : Optimize the snsflib using passed snsf files.

`-r [snsf files]`
  : Convert to Rom files, no optimization

`-x [snsf files]`
  : Convert to SPC files

`-s [snsflib] [Hex offset] [Count]`
  : Optimize snsflib using a known offset/count

`-t [options] [snsf files]`
  : Times the SNSF files. (for auto tagging, use the -T option)
    Unlike psf playback, silence detection is MANDATORY
    Do NOT try to evade this with an excessively long silence detect time.
    (The max time is less than 2*Verify loops for silence detection)

##### Options for -t

`-V [time]`
  : Length of verify loops at end point. (Default 20 seconds)

`-L [count]`
  : Number of loops to time for. (Default 2, max 255)

`-T`
  : Tag the songs with found time.
    A Fade is also added if the song is not detected to be one shot.

`-F [time]`
  : Length of looping song fade. (default 10.000)

`-f [time]`
  : Length of one shot song postgap. (default 1.000)

`-s [time]`
  : Time in seconds for silence detection (default 15 seconds)
    Max (2*Verify loop count) seconds.

##### Options for -x

`-d`
  : Delayed SPC capture, delay-time can be specified by `-T`
