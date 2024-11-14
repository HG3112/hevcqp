hevcqp by HellGauss (HG)

********************************

USAGE:
1) hevcqp.exe inputFile
2) hevcqp.exe inputFile outputStatsFile

This script displays on stdout general QP statistics for hevc video in inputFile

Optionally, it writes detailed statistics for each frame in outputStatsFile

Log messages are on stderr

Requires ffmpeg.exe and ffprobe_mod.exe in same (current) dir or PATH (tested with ffmpeg 7.1 on Windows 10)

Instructions for compiling ffprobe_mod are in hevcdec_mod folder.

********************************

Output format for general statistics (stdout):

{Frametype}: This select the group of frames considered

{N} = Number of frames in the considered group

{N/TOT} = % of N over total number frames

{Total Size} = Sum of size in bytes of frames in the considered group, as measured by ffprobe in frame->pkt_size

{Total Size/TOT} = % Total size of frame in group over total size of every frame

{Average Size} = {Total Size} / {N}

{AVERAGE QP} = Average QP of all macroblocks in the group.

{StdDev (frames)} = Standard deviation of the average QP of each frames

{StdDev (MB)} = Standard deviation of all macroblocks in all frames in the group

At the end, statistics on consecutive BFrames groups are reported.

********************************

Output format for per-frame statistics (outputStatsFile):

First row is number of frames. Next rows are info on each frame

{TYPE} {SIZE} {POSITION} {AVG QP} {STDEV QP-MB} {MIN-QP} {MAX-QP}

Values are separated by tab.

{TYPE} = K for keyframes. If not a key frame, as reported in pict_type by ffprobe

{SIZE} = pkt_size in FRAME field by ffprobe

{POSITION} = pkt_pos in FRAME field by ffprobe

{AVG QP} = Average QP of all macroblocks in the frame.

{STDEV QP-MB}: Standard deviation of macroBlocks QP values in the frame

{MIN-QP} {MAX-QP}: Minimum and maximum quantizer of macroblocks qp in frame.

********************************

TECH

The script launches the command

ffmpeg.exe -hide_banner -loglevel error -threads 1 -i inputFile -map 0:V:0 -c:v copy -f hevc -bsf:v hevc_mp4toannexb pipe: | ffprobe_mod.exe -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -i pipe:

and parses stdout.

********************************

NOTES:
- Progress LOG messages are reported each 100 frames analyzed
- Could not work with trimmed video, or with encoded but not presented frames. Usually the decoded frames should be +1 with respect to presented frames (see LOG messages). Last frame should not be a Bframe. 
