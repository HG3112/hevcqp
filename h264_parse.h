#pragma once
#include <string>
#include "utils.h"

struct shared
{
	FILE* pipeA;
	HANDLE pipeB;
	frameA* chainA;
	frameB* chainB;
	int counterA;
	int counterB;
};

int h264_parse(frame** fp, char* file_string, std::string ffprobe_ext);
void threadF(shared* sD);