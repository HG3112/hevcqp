hevcqp by HellGauss (HG)

********************************

USAGE:
1) hevcqp.exe inputFile
2) hevcqp.exe inputFile outputStatsFile

This script displays on stdout general QP statistics for hevc/h264 video in inputFile

Optionally, it writes detailed statistics for each frame in outputStatsFile

Log messages are on stderr

Requires ffmpeg.exe and ffprobe or ffprobe_mod.exe in same (current) dir or PATH

********************************

Output format for per-frame statistics (outputStatsFile):

First row is number file name

Second row is codec type

Third row is number of frames and macroblocks per frame, separated by a tab character

Then, for each frame, the following info are reported

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

- For hevc, the script launches the command

ffmpeg.exe -hide_banner -loglevel error -threads 1 -i inputFile -map 0:V:0 -c:v copy -f hevc -bsf:v hevc_mp4toannexb pipe: | ffprobe_mod.exe -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -i pipe:

and parses stdout.

- For h264, the script launches the command

ffmpeg -hide_banner -loglevel error -threads 1 -i inputFile -map 0:V:0 -c:v copy -f h264 -bsf:v h264_mp4toannexb pipe: | ffprobe_mod -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -debug qp -i pipe:

and parses the outputs in stdout and stderr. If ffprobe_mod is not available ffprobe is used (h264 only) 

********************************

NOTES:
- For fast use, drag and drop video file in DragNDropHere.bat. A .txt file with general statistic will be generated in the same dir as the input
- Progress LOG messages are reported every 200 frames analyzed
- Could not work with trimmed video, or with encoded but not presented frames. For hevc, usually the decoded frames should be +1 with respect to presented frames (see LOG messages). Last frame should not be a Bframe.
- If ffprobe_mod is not available, h264 analysis is still possible through ffprobe. hevc analysis is available only through ffprobe_mod
- For 8bit x264 encoding, QP analysis coincides with the encoding log of x264. For 10bit QP are shifted by -12.
- ffprobe_mod binary is included (see LICENSE_ffprobe_mod for separate license). ffprobe_mod is a derived work from ffprobe (ffmpeg). Please use ffprobe_mod only with hevcqp.
