#pragma once

#include "stdio.h"
#include "windows.h"
#include "stdint.h"

struct frame
{
	char type;
	uint32_t macroblocks;
	uint32_t size;
	uint64_t byte_position;
	double avg_qp;
	double stdevQ_qp;
	int minqp;
	int maxqp;
};

struct frameA
{
	char type;
	uint32_t size;
	uint64_t byte_position;

	frameA* next;
};

struct frameB
{
	double avg_qp;
	double stdevQ_qp;
	char marked;//for sorting hevc
	int minqp;
	int maxqp;
	uint32_t macroblocks;

	frameB* next;
};

bool checktype(frame* f, char c);
int readline(FILE* stream, char* buffer, int buf_size);
int readline(HANDLE stream, char* buffer, int buf_size);

int64_t sttoint(char* pp);
double sttodouble(char* pp);

#define LINE_BUFFER_SIZE 7500 //sufficiente anche per video 8k
#define ISNUM(x) ((x>='0')&&(x<='9'))
#define ISNUM_(x) (((x>='0')&&(x<='9'))||(x==' '))
#define QPV(x) (x==' ' ? 0 : x-'0')

//return errors
#define ERR_MB_CHECK 12345
#define ERR_FFPROBE_NOT_FOUND 100
#define ERR_FFMPEG_NOT_FOUND 101
#define ERR_FFPROBEMOD_NOT_FOUND 102
#define ERR_FFPROBE_PIPE_BASIC 3
#define ERR_NO_H_STREAM 4
#define ERR_NAMEDPIPE 5
#define ERR_FFPROBE_PIPE_ANALYSIS 6
#define ERR_STDOUT_END_INVALID 7
#define ERR_FRAME_MISMATCH 8
#define ERR_OUTPUT_STATS 9
#define ERR_ALREADY_MARKED 20
#define ERR_NOT_MARKED 21
