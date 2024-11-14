NOTE:

The attached file hevcdec_mod.c is a modification of hevcdec.c from ffmpeg 7.1 (libavcodec/hevc/)

Modification are from me (HG) and stg7 repository https://github.com/stg7/hevc_bitstreamparser

Use the modified version only with hevcqp script

****************************

Instruction for compiling ffprobe_mod.exe

- Create an environment to build ffmpeg (static). E.g. on Windows follow step-by-step instructions here: https://www.youtube.com/watch?v=OIYGjzmJ2GI (thanks to DevStefIt). Pause before compiling ffmpeg.
- Copy the following modifications from hevcdec_mod.c into ffmpeg hevcdec.c
   - line 60-61
   - line 2546 and 2553-2557
   - line 3184-3188
   - line 3443
- Note that you have to find where to put the changes, since hevcdec.c depends on the ffmpeg version you are building and may change in future. You cannot simply copy the new file.
- Continue compiling ffmpeg. If you follow DevStefIT tutorial, in order to work I had to replace --enable-yasm with --enable-x86asm. Also you can remove --enable-nonfree
- Rename ffprobe.exe in ffprobe_mod.exe and place in the same dir as the script, or in PATH
- Thanks to ffmpeg team, and to stg7 for his mod