
#include <string>
#include "utils.h"


int hevc_parse(frame** fp, char* file_string)
{
	frameA* chainA = NULL;
	frameB* chainB = NULL;
	frameA* currentFrameA=NULL;
	frameB* currentFrameB=NULL;
	std::string cmd;

	int x,y,c,counter;

	int frameCountA = 0;
	int frameCountB = 0;
	double tmpd=0.0;
	int mbcount = 0;
	int mbcount_check = -1;

	char* line = new char[LINE_BUFFER_SIZE];
	char* cptr;

	FILE* pipe;

	fprintf(stderr, "[LOG] STARTING ffmpeg.exe | ffprobe_mod.exe FOR ANALYSIS\n");
	cmd = std::string("ffmpeg.exe -hide_banner -loglevel error -threads 1 -i \"")
		+ std::string(file_string)
		+ std::string("\" -map 0:V:0 -c:v copy -f hevc -bsf:v hevc_mp4toannexb pipe: | ffprobe_mod.exe -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -i pipe:");
	pipe = _popen(cmd.c_str(), "rb");
	if (pipe == NULL)
	{
		fprintf(stderr, "[LOG] Fail opening pipe for ffprobe stdout - Exit Program");
		exit(ERR_FFPROBE_PIPE_ANALYSIS);
	}


	x = 0;
	while (readline(pipe, line, LINE_BUFFER_SIZE) >= 0)
	{
		cptr = line;
		if (strstr(line, "ffprobe_mod: ") == line)
		{
			if (chainB == NULL)//first round
			{
				chainB = new frameB;
				currentFrameB = chainB;
			}
			else
			{
				currentFrameB->next = new frameB;
				currentFrameB = currentFrameB->next;
			}
			frameCountB++;
			currentFrameB->marked = 0;

			while (*cptr != ' ') cptr++; cptr++;
			mbcount = (int) (sttoint(cptr));
			while (*cptr != ' ') cptr++; cptr++;
			currentFrameB->avg_qp = sttodouble(cptr);
			while (*cptr != ' ') cptr++; cptr++;
			currentFrameB->stdevQ_qp = sttodouble(cptr);
			while (*cptr != ' ') cptr++; cptr++;
			currentFrameB->minqp = (int) (sttoint(cptr));
			while (*cptr != ' ') cptr++; cptr++;
			currentFrameB->maxqp = (int) (sttoint(cptr));

			if (mbcount_check == -1)
			{
				mbcount_check = mbcount;
				fprintf(stderr, "[LOG] Macroblocks per frame: %d\n", mbcount);
			}
			else if (mbcount_check != mbcount)
			{
				delete[] line;
				fprintf(stderr, "[LOG] Something went wrong while parsing (mbcount_check). Closing app.");
				_pclose(pipe);
				exit(ERR_MB_CHECK);
			}
			currentFrameB->avg_qp /= mbcount;
			currentFrameB->stdevQ_qp /= mbcount;
			currentFrameB->stdevQ_qp -= currentFrameB->avg_qp * currentFrameB->avg_qp;
			currentFrameB->macroblocks = mbcount;
		}
		else if (strstr(line, "[FRAME]") == line)
		{
			frameCountA++;
			if (chainA == NULL)//first round
			{
				chainA = new frameA;
				currentFrameA = chainA;
			}
			else
			{
				currentFrameA->next = new frameA;
				currentFrameA = currentFrameA->next;
			}
			currentFrameA->type = '?';
			currentFrameA->next = NULL;

			if (frameCountA % 200 == 0)
				fprintf(stderr, "[LOG] Proccessing FRAME %d\n", frameCountA);
		}
		else if (strstr(line, "key_frame=") == line)
		{
			while (*cptr != '=') cptr++;
			cptr++;
			if (*cptr == '1') currentFrameA->type = 'K';

		}
		else if (strstr(line, "pkt_pos=") == line)
		{
			while (*cptr != '=') cptr++;
			cptr++;
			currentFrameA->byte_position = sttoint(cptr);
		}
		else if (strstr(line, "pkt_size=") == line)
		{
			while (*cptr != '=') cptr++;
			cptr++;
			currentFrameA->size = (int) (sttoint(cptr));
		}
		else if (strstr(line, "pict_type=") == line)
		{
			if (currentFrameA->type != 'K')
			{
				while (*cptr != '=') cptr++;
				cptr++;
				currentFrameA->type = *cptr;
			}
		}
		else if (strstr(line, "[STREAM]") == line)
		{
			x = 1;//END OK
			break;
		}
	}
	delete[] line;
	if (x != 1)
	{
		fprintf(stderr, "[LOG] Something went wrong while parsing. Closing app.");
		_pclose(pipe);
		exit( ERR_STDOUT_END_INVALID);
	}

	//COMPARE ffprobe vs hevcdec
	_pclose(pipe);
	if (currentFrameA!=NULL) if (currentFrameA->type == 'B')
		fprintf(stderr, "[LOG] ** W A R N I N G ** Last frame is a B frame. Analysis could be wrong\n");
	fprintf(stderr, "[LOG] Frames counted: %d ---  Frames decoded: %d\n", frameCountA, frameCountB);
	if (frameCountB < frameCountA)
	{
		fprintf(stderr, "[LOG] FAIL ( decoded < counted ) - Exit Program\n");
		exit( ERR_FRAME_MISMATCH);
	}
	counter = frameCountB - frameCountA;
	if (counter > 0) fprintf(stderr, "[LOG] Discarding %d decoded frames (initial parsing)\n", counter);
	for (x = 0; x < counter; x++)
	{
		currentFrameB = chainB->next;//tmp
		delete chainB;
		chainB = currentFrameB;
	}
	counter = frameCountA;
	fprintf(stderr, "[LOG] Total frames %d\n", counter);


	//////////SORT DECODED FRAMES
	fprintf(stderr, "[LOG] SORTING DECODED FRAMES\n");
	int firstUnmarked = 0;
	char Kc;

	//arrange in array
	frame* arrayA = new frame[counter];
	frameB* arrayB = new frameB[counter];
	*fp = arrayA;

	for (x = 0; x < counter; x++)
	{
		arrayA[x].type = chainA->type;
		arrayA[x].size = chainA->size;
		arrayA[x].byte_position = chainA->byte_position;

		arrayB[x].avg_qp = chainB->avg_qp;
		arrayB[x].stdevQ_qp = chainB->stdevQ_qp;
		arrayB[x].marked = 0;
		arrayB[x].minqp = chainB->minqp;
		arrayB[x].maxqp = chainB->maxqp;
		arrayB[x].macroblocks = chainB->macroblocks;

		currentFrameA = chainA->next;
		currentFrameB = chainB->next;
		delete chainA;
		delete chainB;
		chainA = currentFrameA;
		chainB = currentFrameB;
	}
	//sort
	for (x = 0; x < counter; x++)
	{
		c = firstUnmarked;
		Kc = 0;
		for (y = firstUnmarked; y < counter; y++)
		{
			if (arrayA[y].byte_position < arrayA[x].byte_position) c++;

			if (arrayA[y].type == 'K') Kc++;
			if (Kc == 2) break;  //search max in following gop, up to last K included	
		}
		arrayA[x].avg_qp = arrayB[c].avg_qp;
		arrayA[x].stdevQ_qp = arrayB[c].stdevQ_qp;
		arrayA[x].minqp = arrayB[c].minqp;
		arrayA[x].maxqp = arrayB[c].maxqp;
		arrayA[x].macroblocks = arrayB[c].macroblocks;

		if (arrayB[c].marked == 1)
		{
			fprintf(stderr, "[LOG] ERROR WHILE SORTING DECODED FRAMES - Exit program\n");
			exit(ERR_ALREADY_MARKED);
		}

		arrayB[c].marked = 1;
		for (y = firstUnmarked; y < counter; y++)
		{
			if (arrayB[y].marked == 0)
			{
				firstUnmarked = y;
				break;
			}
		}
	}
	for (x = 0; x < counter; x++)// check all frames have been counted
	{
		if (arrayB[x].marked == 0)
		{
			fprintf(stderr, "[LOG] ERROR AFTER SORTING DECODED FRAMES - Exit program\n");
			exit(ERR_NOT_MARKED);
		}
	}
	delete[] arrayB;

	return counter;
}