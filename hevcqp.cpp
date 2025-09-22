#define BUILD 250922

#include <stdio.h>
#include <math.h>
#include <filesystem>
#include <string>

#include "utils.h"
#include "h264_parse.h"
#include "hevc_parse.h"


int main(int argv, char** argc)
{
	FILE* pipe;

	std::string ffprobe_ext;
	std::string cmd;
	char* line = new char[LINE_BUFFER_SIZE];

	//HELP + CHECK FILE IN argv[1]
	int help = 0;
	if ((argv != 2) && (argv != 3)) help = 1;
	else if (!(std::filesystem::exists(argc[1]))) //REMARK: c++17 standard must be enable in compiler/project option
	{
		help = 1;
		fprintf(stderr, "Input file:\n%s\ndoes not exist\n\n", argc[1]);
	}
	if (help == 1)
	{
		printf("hevcqp - Build %d - by HG\n\n", BUILD);
		printf("USAGE:\n");
		printf("1) hevcqp.exe inputFile\n");
		printf("2) hevcqp.exe inputFile outputStatsFile\n\n");

		printf("This script displays on stdout general QP statistics for hevc/h264 video in inputFile\n");
		printf("Optionally, it writes detailed statistics for each frame in outputStatsFile\n");
		printf("Log messages are on stderr\n");
		printf("Requires ffmpeg.exe and ffprobe or ffprobe_mod.exe in same (current) dir or PATH\n");
		//printf("See readme for compiling ffprobe_mod.exe\n\n");
		return 0;
	}

	//INIT
	int ffprobe_mod = 0;
	fprintf(stderr, "[LOG] hevcqp - Build %d - by HG\n[LOG] INPUT FILE: %s\n[LOG] OUTPUT STATS: ", BUILD, argc[1]);
	if (argv == 3)
		fprintf(stderr, argc[2]);
	else
		fprintf(stderr, "# DISABLED");
	//check ffmpeg
	fprintf(stderr, "\n[LOG] Check if ffmpeg.exe is available... ");
	if (system("ffmpeg.exe -hide_banner -version >NUL") != 0)
	{
		fprintf(stderr, "\n[LOG] ffmpeg.exe not found - Exit program\n");
		return ERR_FFMPEG_NOT_FOUND;
	}
	fprintf(stderr, "OK\n");
	//check ffprobe
	fprintf(stderr, "[LOG] Check if ffprobe_mod is available... ");
	if (system("ffprobe_mod.exe -version >NUL 2>&1") == 0)
	{
		fprintf(stderr, "OK\n");
		ffprobe_ext = std::string("_mod.exe");
		ffprobe_mod = 1;
	}
	else
	{
		if (system("ffprobe.exe -version >NUL 2>&1") != 0)
		{
			fprintf(stderr, "\n[LOG] ffprobe.exe/ffprobe_mod.exe not found - Exit program\n");
			return ERR_FFPROBE_NOT_FOUND;
		}
		ffprobe_ext = std::string(".exe");
		fprintf(stderr, "\n[LOG] WARNING : ffprobe.exe found, but ffprobe_mod.exe is not - Analysis is available only for h264\n");
	}

	//CHECK STREAM
	int flag10 = 0;
	int flagh = 0;
	cmd = std::string("[LOG] STARTING ffprobe") + ffprobe_ext + std::string(" FOR BASIC INFO\n");
	fprintf(stderr, cmd.c_str());
	cmd = std::string("ffprobe")+ffprobe_ext+ std::string(" -threads 1 -v quiet -select_streams V:0 -show_streams \"") + std::string(argc[1]) + std::string("\"");
	pipe = _popen(cmd.c_str(), "rb");
	if (pipe == NULL)
	{
		fprintf(stderr, "[LOG] Fail opening pipe for ffprobe stdout - Exit Program");
		return ERR_FFPROBE_PIPE_BASIC;
	}
	while (readline(pipe, line, LINE_BUFFER_SIZE) >= 0)
	{
		if (strstr(line, "bits_per_raw_sample=10") == line) flag10 = 1;
		else if (strstr(line, "codec_name=h264") == line) flagh = 1;
		else if (strstr(line, "codec_name=hevc") == line) flagh = 2;

		if (flag10 + flagh > 1) break;//nothing more to read
	}
	_pclose(pipe);
	if (flagh == 0)
	{
		fprintf(stderr, "[LOG] h264/hevc stream not found - Exit program\n");
		return ERR_NO_H_STREAM;
	}
	else if (flagh == 1)
	{
		fprintf(stderr, "[LOG] h264 stream found --- Bit depth = %d bit", 8 + 2 * flag10);
		if (flag10 == 1)
			fprintf(stderr, " (qp values are shifted by -12)");
		fputc('\n', stderr);
	}
	else
	{
		if (ffprobe_mod == 0)
		{
			fprintf(stderr, "[LOG] hevc stream found but ffprobe_mod is not available - Exit program\n");
			return ERR_FFPROBEMOD_NOT_FOUND;
		}
		fprintf(stderr, "[LOG] hevc stream found\n");
	}

	//ANALYSIS
	frame* f;
	frame** fp = &f;
	int frameN;
	if (flagh == 1)
		frameN = h264_parse(fp, argc[1], ffprobe_ext);
	else
		frameN = hevc_parse(fp, argc[1]);

	////////////////OUTPUT
	//WRITE GLOBAL STATISTICS
	char type[] = "0BP1KI2";
	int x,y;
	int l = (int) ( strlen(type) );
	uint64_t totalSize,totaltotalSize;
	int flag12i = 0;
	if (flag10 * flagh == 1) flag12i = 12;
	double flag12d = flag12i;


	uint32_t n;
	double mSize;
	double mQP;
	double stdevF;
	double stdevMB;
	double tmpd;
	const char* filename = argc[1];
	while (*filename != 0) { filename++; }
	fprintf(stderr,filename);
	while ((*filename != '\\') && (filename != argc[1]) ) filename--;
	if (*filename == '\\') filename++;

	fprintf(stderr, "[LOG] WRITING STATISTIC TO STDOUT\n");
	printf("hevcqp report - Build %d - by HG\n\n", BUILD);
	printf("Input file            : %s\n", filename);
	printf("Codec                 : ");
	if (flagh == 1) //h264
	{
		printf("h264 (");
		if (flag10 == 0)
			printf("8bit)\n");
		else
			printf("10bit)\n");
	}
	else
		printf("hevc\n");

	printf("Macroblocks per frame : %d\n\n", f[0].macroblocks);
	for (x = 0; x < l; x++)
	{
		n = 0;
		totalSize = 0;
		totaltotalSize = 0;
		mSize = 0.0;
		mQP = 0.0;
		stdevF = 0.0;
		stdevMB = 0.0;

		for (y = 0; y < frameN; y++)
		{
			totaltotalSize += f[y].size;
			if (checktype(&(f[y]), type[x]))
			{
				totalSize += f[y].size;
				n++;
				mQP += f[y].avg_qp;
			}
		}
		mSize = (double) (totalSize);
		mSize /= n;
		mQP /= n;
		for (y = 0; y < frameN; y++)
		{
			if (checktype(&(f[y]), type[x]))
			{
				tmpd = mQP - f[y].avg_qp;
				tmpd *= tmpd;
				stdevF += tmpd;
				stdevMB += f[y].stdevQ_qp;
				stdevMB += tmpd;
			}
		}
		stdevF /= n;
		stdevMB /= n;
		stdevF = sqrt(stdevF);
		stdevMB = sqrt(stdevMB);
		if (type[x] == '0') printf("General statistics");
		else if (type[x] == '1') printf("K+I");
		else if (type[x] == '2') printf("P+B");
		else putchar(type[x]);
		printf(":\n");

		printf("Number of frames      : %d", n);
		if (type[x] != '0')
			printf(" ( %f %% )", 100.0 * (double)(n) / (double)(frameN));
		printf("\n");
		printf("Total Size            : %s", std::to_string(totalSize).c_str());
		if (type[x] != '0')
			printf(" ( %f %% )", 100.0 * (double)(totalSize) / (double)(totaltotalSize));
		printf("\n");
		printf("Average Size          : %f\n", mSize);
		printf("AVERAGE QP            : %f\n", mQP-flag12d);
		printf("StdDev (frames)       : %f\n", stdevF);
		printf("StdDev (MB)           : %f\n\n", stdevMB);

		if (type[x] == '0') printf("--------------------------------\n\n");
	}

	printf("--------------------------------\n\n");

	//CONSECUTIVE B
	double totB = 0.0;
	int maxB = 0;
	int ccounterB = 0;
	int* cBf = new int[16];
	for (x = 0; x < 16; x++) cBf[x] = 0;
	for (y = 0; y < frameN; y++)
	{
		if (f[y].type == 'B')
		{
			ccounterB++;
		}
		else if (ccounterB > 0)
		{
			if (ccounterB <= 16) cBf[ccounterB - 1] += ccounterB;
			maxB = (ccounterB > maxB ? ccounterB : maxB);
			totB = totB + ccounterB;
			ccounterB = 0;
		}
	}
	if (ccounterB > 0)
	{
		if (ccounterB <= 16) cBf[ccounterB - 1] += ccounterB;
		maxB = (ccounterB > maxB ? ccounterB : maxB);
		totB = totB + ccounterB;
	}
	printf("Consecutive B-Frames (number of B-Frames in each group)");
	if (maxB == 0) printf("  # N/A");
	printf("\n");
	for (x = 0; x < maxB; x++)
	{
		if ( (x < 9) && (maxB>9)) printf(" ");
		printf("%d : %d ( %f %% )\n", x + 1, cBf[x], 100.0 * cBf[x] / totB);
	}
	delete[] cBf;

	//PER FRAME STATISTICS
	if (argv == 3)
	{
		FILE* out;
		errno_t e_out;
		e_out = fopen_s(&out,argc[2], "wb");
		if ((out == NULL)|| (e_out!=0))
		{
			fprintf(stderr, "[LOG] FAILED OPENING OUTPUT STATS FILE\n");
			return ERR_OUTPUT_STATS;
		}
		fprintf(stderr, "[LOG] WRITING PER-FRAME STATS FILE\n");
		fputs(filename, out);
		fputc('\n', out);
		if (flagh == 1) //h264
		{
			fputs("h264 (",out);
			if (flag10 == 0)
				fputs("8bit)\n",out);
			else
				fputs("10bit)\n",out);
		}
		else
			fputs("hevc\n",out);

		fputs(std::to_string(frameN).c_str(), out);//frames number
		fputc('\t', out);
		fputs(std::to_string(f[0].macroblocks).c_str(), out);//macroblocks per frame
		fputc('\n', out);
		for (y = 0; y < frameN; y++)
		{
			fputc(f[y].type, out);
			fputc('\t', out);
			fputs(std::to_string(f[y].size).c_str(), out);
			fputc('\t', out);
			fputs(std::to_string(f[y].byte_position).c_str(), out);
			fputc('\t', out);
			fputs(std::to_string(f[y].avg_qp-flag12d).c_str(), out);
			fputc('\t', out);
			fputs(std::to_string(sqrt(f[y].stdevQ_qp)).c_str(), out);
			fputc('\t', out);
			fputs(std::to_string(f[y].minqp-flag12i).c_str(), out);
			fputc('\t', out);
			fputs(std::to_string(f[y].maxqp-flag12i).c_str(), out);
			fputc('\n', out);
		}
		fclose(out);
	}
	//CLOSING
	delete[] *fp;
	fprintf(stderr, "[LOG] SCRIPT COMPLETE\n");
	return 0;
}

/*
BONUS

Create the file DragNDropHere.bat in the same dir as follows

cd /D "%~dp0"
hevcqp.exe %1 %1.dat >%1.txt
pause

Then drag your video file on it
*/
