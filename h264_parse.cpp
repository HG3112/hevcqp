#include <stdio.h>
#include <windows.h>
#include <thread>

#include "utils.h"
#include "h264_parse.h"

void threadF(shared* sD)
{
	char* line = new char[LINE_BUFFER_SIZE];
	char* cptr;
	int mbcount=-1;
	int newframeflag = 0;
	int startflag = 0;//discard reduntant initial frames
	int mbcount_check = -1;

	int tmpi;

	frameB* currentFrameB = NULL;
	sD->chainB = currentFrameB;
	ConnectNamedPipe(sD->pipeB, NULL);

	while (1)//FRAME CYCLE
	{
		if (newframeflag == 0) if (readline(sD->pipeB, line, LINE_BUFFER_SIZE) < 0) break; //END

		newframeflag = 0;

		if (strstr(line, "Processing read interval") != NULL) startflag = 1; //discard reduntant initial frames, start after this line

		if (startflag == 1) if (line[0] == '[') if (strstr(line, "[h264 @ ") == line) if (strstr(line, "New frame, type:") != NULL)
		{
			if (currentFrameB == NULL) //first round
			{
				currentFrameB = new frameB;
				sD->chainB = currentFrameB;
			}
			else
			{
				currentFrameB->next = new frameB;
				currentFrameB = currentFrameB->next;
				mbcount_check = mbcount;
			}
			currentFrameB->avg_qp = 0.0;
			currentFrameB->stdevQ_qp = 0.0;
			currentFrameB->minqp = 999;
			currentFrameB->maxqp = -999;
			currentFrameB->next = NULL;
			sD->counterB++;

			readline(sD->pipeB, line, LINE_BUFFER_SIZE);//useless coordinate row

			mbcount = 0;
			while (1)//MACROBLOCK CYCLE
			{
				readline(sD->pipeB, line, LINE_BUFFER_SIZE);
				if (strstr(line, "[h264 @ ") == line) if (strstr(line, "New frame, type:") != NULL)
				{
					newframeflag = 1;
					break; //stop current frame parsing, do not read another line since we alreay have a new frame
				}
				if ((strstr(line, " nal_unit_type:") != NULL) || (strstr(line, "[h264 @ ") != line) ) break; //stop current f. parsing
				cptr = line;
				while (*cptr != ']') cptr++;
				cptr++;
				while (*cptr == ' ') cptr++;
				if (!ISNUM(*cptr)) break; //stop current f. parsing
				while (ISNUM(*cptr)) cptr++;
				if (*cptr != ' ') break; //stop current f. parsing
				cptr++;

				while (ISNUM_(*cptr))
				{
					tmpi = 10 * (QPV(*cptr));
					cptr++;
					tmpi += QPV(*cptr);
					cptr++;
					currentFrameB->avg_qp += tmpi;
					currentFrameB->stdevQ_qp += tmpi * tmpi;
					currentFrameB->minqp = currentFrameB->minqp < tmpi ? currentFrameB->minqp : tmpi;
					currentFrameB->maxqp = currentFrameB->maxqp > tmpi ? currentFrameB->maxqp : tmpi;

					mbcount++;
				}
			}

			if (mbcount_check == -1) fprintf(stderr, "[LOG] Macroblocks per frame: %d\n", mbcount);

			if (mbcount_check != -1) if (mbcount_check != mbcount)
			{
				sD->counterB = -1;
				delete[] line;
				fprintf(stderr, "[LOG] Something went wrong while parsing (mbcount_check). Closing app.");
				_pclose(sD->pipeA);
				CloseHandle(sD->pipeB);
				exit(ERR_MB_CHECK);
			}
			currentFrameB->avg_qp /= mbcount;
			currentFrameB->stdevQ_qp /= mbcount;
			currentFrameB->stdevQ_qp -= currentFrameB->avg_qp * currentFrameB->avg_qp;
		}
	}
	delete[] line;
	return;
}

int h264_parse(frame** fp, char* file_string, std::string ffprobe_ext)
{
	shared sharedData;
	sharedData.counterA = 0;
	sharedData.counterB = 0;
	sharedData.chainA = NULL;
	sharedData.chainB = NULL;


	frameA* currentFrameA=NULL;
	frameB* currentFrameB=NULL;
	std::string cmd;
	char* cptr;
	int x;
	char* line = new char[LINE_BUFFER_SIZE];

	//create named pipe for stderr	
	int pid = _getpid();//allows for multiple runs
	sharedData.pipeB = CreateNamedPipeA(
		std::string("\\\\.\\pipe\\ffprobe_stderr_PID" + std::to_string(pid)).c_str(),
		PIPE_ACCESS_INBOUND,
		PIPE_TYPE_BYTE,
		1,
		2048,//IS 2048 OK?
		2048,
		0,
		NULL);
	sharedData.counterB = 0;
	if (sharedData.pipeB == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "[LOG] Fail opening NamedPipe for ffprobe stderr - Exit Program");
		exit(ERR_NAMEDPIPE);
	}

	//standard pipe for stdout, start ffprobe analysis
	cmd = std::string("[LOG] STARTING ffmpeg.exe | ffprobe") + ffprobe_ext + std::string(" FOR ANALYSIS\n");
	fprintf(stderr, cmd.c_str());
	cmd = std::string("ffmpeg.exe -hide_banner -loglevel error -threads 1 -i \"")
		+ std::string(file_string)
		+ std::string("\" -map 0:V:0 -c:v copy -f h264 pipe: | ffprobe")+ffprobe_ext+std::string(" -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -debug qp -i pipe: 2>>\\\\.\\pipe\\ffprobe_stderr_PID")
		+ std::to_string(pid);
	sharedData.pipeA = _popen(cmd.c_str(), "rb");
	if (sharedData.pipeA == NULL)
	{
		fprintf(stderr, "[LOG] Fail opening pipe for ffprobe stdout - Exit Program");
		CloseHandle(sharedData.pipeB);
		exit(ERR_FFPROBE_PIPE_ANALYSIS);
	}

	//THREAD for stderr
	std::thread* tB;
	tB = new std::thread(threadF, &sharedData);

	//PARSING STDOUT
	x = 0;
	while (readline(sharedData.pipeA, line, LINE_BUFFER_SIZE) >= 0)
	{
		cptr = line;
		if (strstr(line, "[FRAME]") == line)
		{
			sharedData.counterA++;
			if (sharedData.chainA == NULL)//first round
			{
				sharedData.chainA = new frameA;
				currentFrameA = sharedData.chainA;
			}
			else
			{
				currentFrameA->next = new frameA;
				currentFrameA = currentFrameA->next;
			}
			currentFrameA->type = '?';
			currentFrameA->next = NULL;

			if (sharedData.counterA % 200 == 0)
				fprintf(stderr, "[LOG] Proccessing FRAME %d\n", sharedData.counterA);
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
		_pclose(sharedData.pipeA);
		CloseHandle(sharedData.pipeB);
		exit(ERR_STDOUT_END_INVALID);
	}

	//COMPARE STDOUT AND STDERR + CLOSE PIPE STUFF
	fprintf(stderr, "[LOG] Waiting for stderr thread to end and check frames... ");
	tB->join();
	_pclose(sharedData.pipeA);
	CloseHandle(sharedData.pipeB);
	delete tB;
	if (sharedData.counterB != sharedData.counterA)
	{
		fprintf(stderr, "FAIL (frames mismatch stdout vs stderr) %d VS %d\n", sharedData.counterA, sharedData.counterB);
		exit(ERR_FRAME_MISMATCH);
	}
	int counter = sharedData.counterA;
	fprintf(stderr, "OK %d TOTAL FRAMES\n", counter);

	//ORGANIZE ARRAY
	frame* arrayA = new frame[counter];
	//frameB* arrayB = new frameB[counter];
	*fp = arrayA;

	//*fp = new frame[counter];
	//frame* f = *fp;
	currentFrameA = sharedData.chainA;
	currentFrameB = sharedData.chainB;
	for (x=0;x<counter;x++)
	{
		arrayA[x].type = currentFrameA->type;
		arrayA[x].size = currentFrameA->size;
		arrayA[x].byte_position = currentFrameA->byte_position;
		arrayA[x].avg_qp = currentFrameB->avg_qp;
		arrayA[x].stdevQ_qp = currentFrameB->stdevQ_qp;
		arrayA[x].minqp = currentFrameB->minqp;
		arrayA[x].maxqp = currentFrameB->maxqp;
		sharedData.chainA = currentFrameA;
		sharedData.chainB = currentFrameB;
		currentFrameA = currentFrameA->next;
		currentFrameB = currentFrameB->next;
		delete sharedData.chainA;
		delete sharedData.chainB;
	}

	return counter;
}