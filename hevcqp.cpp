//hevcqp.cpp
#include "stdio.h"
#include "math.h"
#include <filesystem>

#define BUILD 241114

#define LINE_BUFFER_SIZE 1000

//return errors
#define ERR_MB_CHECK 12345
#define ERR_FFPROBE_NOT_FOUND 100
#define ERR_FFMPEG_NOT_FOUND 101
#define ERR_FFPROBE_PIPE_BASIC 3
#define ERR_NO_HEVC_STREAM 4
#define ERR_FFPROBE_PIPE_ANALYSIS 6
#define ERR_STDOUT_END_INVALID 7
#define ERR_FRAME_MISMATCH 8
#define ERR_OUTPUT_STATS 9
#define ERR_ALREADY_MARKED 20
#define ERR_NOT_MARKED 21

struct frameA
{
	char type;
	uint32_t size;
	uint64_t byte_position;
	double avg_qp;
	double stdevQ_qp;
	int qpmin;
	int qpmax;
	
	frameA* next;
};
struct frameB
{
	double avg_qp;
	double stdevQ_qp;
	char marked;//for sorting
	int qpmin;
	int qpmax;

	frameB* next;
};

bool checktype(frameA* f, char c)//used in global analysis
{
	if (c=='0') return true; //TOT
	if (c=='1') return ((f->type=='K')||(f->type=='I')); //K+I
	if (c=='2') return ((f->type=='P')||(f->type=='B')); //P+B
	return (f->type==c);
}

int readline(FILE* stream, char* buffer, int buf_size) { //standard pipe
	//Legge i caratteri finchè non trova '\n'
	//ritorna il numero di caratteri letti (eventualmente 0)
	//ritorna -1 in caso di errore o EOF
	//ritorna -2 in caso di overflow

	int ccounter=0;
	int c;
	char* cptr=buffer;

	while(1)
	{
		if (ccounter==buf_size-1) return -2;
		c=fgetc(stream);
		if (c==EOF)
		{
			*cptr=0;
			return -1;
		}
		if ((c=='\n')||(c==0))
		{
			*cptr=0;
			return ccounter;
		}
		*cptr=c;
		cptr++;
		ccounter++;
	}
}

int64_t sttoint(char* pp)
{
	int64_t segno = 1;
	int64_t n = 0;
	if (*pp == '-')
	{
		segno = -1;
		pp++;
	}
	while ((*pp - '0' >= 0) && (*pp - '0' < 10))
	{
		n *= 10;
		n += *pp - '0';
		pp++;
	}
	return segno * n;
}
double sttodouble(char* pp)
{
	int segno = 1;
	int expn = 0;
	double n = 0;
	int nn = -1;
	if (*pp == '-') { segno = -1; pp++; }
	while ((*pp - '0' >= 0) && (*pp - '0' < 10))
	{
		n *= 10; n += *pp - '0';
		pp++;
	}
	if ((*pp == '.') || (*pp == ',')) pp++;
	while ((*pp - '0' >= 0) && (*pp - '0' < 10))
	{
		n += (*pp - '0') * pow(10.0, nn);
		nn--;
		pp++;
	}	
	if ((*pp == 'e') || (*pp == 'E'))
	{
		expn = sttoint(pp + 1);
	}
	return segno * n * pow(10.0, expn);
}

int main(int argv,char** argc) {
	//////////////////////////////////////////////////////////////////////////// M A I N //////
	
	char* line=new char [LINE_BUFFER_SIZE];
	frameA* chainA = NULL;
	frameB* chainB = NULL;
	frameA* currentFrameA;
	frameB* currentFrameB;
	frameA* currentFrameA2;
	
	FILE* pipe;
	char* cptr;//cursore
	double tmpd;
	int mbcount=0;
	int mbcount_check=-1;
	int frameCountA=0;
	int frameCountB=0;
	
	std::string cmd;
	int c,x,y;
	
	//HELP + CHECK FILE IN argv[1]
	int help=0;
	if ((argv!=2)&&(argv!=3)) help=1;
	else if (!(std::filesystem::exists(argc[1])))
	{
		help=1;
		fprintf(stderr,"Input file:\n%s\ndoes not exist\n\n",argc[1]);
	}	
	if (help==1)
	{
		printf("hevcqp - Build %d - by HG\n\n",BUILD);
		printf("USAGE:\n");
		printf("1) hevcqp.exe inputFile\n");
		printf("2) hevcqp.exe inputFile outputStatsFile\n\n");
		
		printf("This script displays on stdout general QP statistics for hevc video in inputFile\n");
		printf("Optionally, it writes detailed statistics for each frame in outputStatsFile\n");
		printf("Log messages are on stderr\n");
		printf("Requires ffmpeg.exe and ffprobe_mod.exe in same (current) dir or PATH\n");
		printf("See readme for compiling ffprobe_mod.exe\n\n");
		return 0;
	}
	
	//INIT
	fprintf(stderr,"[LOG] hevcqp - Build %d - by HG\n[LOG] INPUT FILE: %s\n[LOG] OUTPUT STATS: ",BUILD,argc[1]);
	if (argv==3)
		fprintf(stderr,argc[2]);
	else
		fprintf(stderr,"# DISABLED");
	//check ffmpeg
	fprintf(stderr,"\n[LOG] Check if ffmpeg.exe is available... ");
	if (system("ffmpeg.exe -hide_banner -version >NUL")!=0)
	{
		fprintf(stderr,"\n[LOG] ffmpeg.exe not found - Exit program\n");
		return ERR_FFMPEG_NOT_FOUND;
	}
	fprintf(stderr,"OK\n");
	//check ffprobe_mod
	fprintf(stderr,"[LOG] Check if ffprobe_mod.exe is available... ");
	if (system("ffprobe_mod.exe -version >NUL")!=0)
	{
		fprintf(stderr,"\n[LOG] ffprobe_mod.exe not found - Exit program\n");
		return ERR_FFPROBE_NOT_FOUND;
	}
	fprintf(stderr,"OK\n");

	//check if hevc
	int flaghevc=0;
	fprintf(stderr,"[LOG] STARTING ffprobe_mod.exe FOR BASIC INFO\n");
	cmd=std::string("ffprobe_mod.exe -threads 1 -v quiet -select_streams V:0 -show_streams \"")+std::string(argc[1])+std::string("\"");
	pipe=popen(cmd.c_str(),"rb");
	if(pipe==NULL)
	{
		fprintf(stderr,"[LOG] Fail opening pipe for ffprobe stdout - Exit Program");
		return ERR_FFPROBE_PIPE_BASIC;
	}
	while(readline(pipe,line,LINE_BUFFER_SIZE)>=0)
	{
		if (strstr(line,"codec_name=hevc")==line)
		{
			flaghevc=1;
			break;
		}
	}
	pclose(pipe);
	if (flaghevc==0)
	{
		fprintf(stderr,"[LOG] hevc stream not found - Exit program\n");
		return ERR_NO_HEVC_STREAM;
	}
	fprintf(stderr,"[LOG] hevc stream found\n");

	
	
	//standard pipe for stdout, start ffprobe analysis
	fprintf(stderr,"[LOG] STARTING ffmpeg.exe | ffprobe_mod.exe FOR ANALYSIS\n");
	cmd=std::string("ffmpeg.exe -hide_banner -loglevel error -threads 1 -i \"")
			+std::string(argc[1])
			+std::string("\" -map 0:V:0 -c:v copy -f hevc -bsf:v hevc_mp4toannexb pipe: | ffprobe_mod.exe -threads 1 -v quiet -show_frames -show_streams -show_entries frame=key_frame,pkt_pos,pkt_size,pict_type -i pipe:");
	pipe=popen(cmd.c_str(),"rb");
	if(pipe==NULL)
	{
		fprintf(stderr,"[LOG] Fail opening pipe for ffprobe stdout - Exit Program");
		return ERR_FFPROBE_PIPE_ANALYSIS;
	}
	
	////////////////////////////
	//////////////PARSING STDOUT
	x=0;
	while(readline(pipe,line,LINE_BUFFER_SIZE)>=0)
	{
		cptr=line;
		if (strstr(line,"ffprobe_mod: ")==line)
		{
			if (chainB==NULL)//first round
			{
				chainB=new frameB;
				currentFrameB=chainB;
			}
			else
			{
				currentFrameB->next=new frameB;
				currentFrameB=currentFrameB->next;
			}
			frameCountB++;
			currentFrameB->marked=0;
		
			while (*cptr!=' ') cptr++; cptr++;
			mbcount=sttoint(cptr);
			while (*cptr!=' ') cptr++; cptr++;
			currentFrameB->avg_qp=sttodouble(cptr);
			while (*cptr!=' ') cptr++; cptr++;
			currentFrameB->stdevQ_qp=sttodouble(cptr);
			while (*cptr!=' ') cptr++; cptr++;
			currentFrameB->qpmin=sttoint(cptr);
			while (*cptr!=' ') cptr++; cptr++;
			currentFrameB->qpmax=sttoint(cptr);		
			
			if (mbcount_check==-1)
			{
				mbcount_check=mbcount;
				fprintf(stderr,"[LOG] Macroblocks per frame: %d\n",mbcount);
			}
			else if (mbcount_check!=mbcount)
			{
				delete [] line;
				fprintf(stderr,"[LOG] Something went wrong while parsing (mbcount_check). Closing app.");
				pclose(pipe);
				exit(ERR_MB_CHECK);
			}
			currentFrameB->avg_qp/=mbcount;
			currentFrameB->stdevQ_qp/=mbcount;
			currentFrameB->stdevQ_qp-=currentFrameB->avg_qp*currentFrameB->avg_qp;
		}
		else if (strstr(line,"[FRAME]")==line)
		{
			frameCountA++;
			if (chainA==NULL)//first round
			{
				chainA=new frameA;
				currentFrameA=chainA;
			}
			else
			{
				currentFrameA->next=new frameA;
				currentFrameA=currentFrameA->next;
			}
			currentFrameA->type='?';
			currentFrameA->next=NULL;

			if (frameCountA%100==0)
				fprintf(stderr,"[LOG] Proccessing FRAME %d\n",frameCountA);
		}
		else if (strstr(line,"key_frame=")==line)
		{
			while(*cptr!='=') cptr++;
			cptr++;
			if (*cptr=='1') currentFrameA->type='K';

		}
		else if (strstr(line,"pkt_pos=")==line)
		{
			while(*cptr!='=') cptr++;
			cptr++;
			currentFrameA->byte_position=sttoint(cptr);
		}
		else if (strstr(line,"pkt_size=")==line)
		{
			while(*cptr!='=') cptr++;
			cptr++;
			currentFrameA->size=sttoint(cptr);
		}
		else if (strstr(line,"pict_type=")==line)
		{
			if(currentFrameA->type!='K')
			{
				while(*cptr!='=') cptr++;
				cptr++;
				currentFrameA->type=*cptr;
			}
		}
		else if (strstr(line,"[STREAM]")==line)
		{
			x=1;//END OK
			break;
		}
	}
	delete [] line;
	if (x!=1)
	{
		fprintf(stderr,"[LOG] Something went wrong while parsing. Closing app.");
		pclose(pipe);
		return ERR_STDOUT_END_INVALID;
	}
	
	//COMPARE ffprobe vs hevcdec
	pclose(pipe);
	if(currentFrameA->type=='B') fprintf(stderr,"[LOG] ** W A R N I N G ** Last frame is a B frame. Analysis could be wrong\n");
	fprintf(stderr,"[LOG] Frames counted: %d ---  Frames decoded: %d\n",frameCountA, frameCountB);
	if (frameCountB<frameCountA)
	{
		fprintf(stderr,"[LOG] FAIL ( decoded < counted ) - Exit Program\n");
		return ERR_FRAME_MISMATCH;
	}
	c=frameCountB-frameCountA;
	if (c>0) fprintf(stderr,"[LOG] Discarding %d decoded frames (initial parsing)\n",c);
	for (x=0;x<c;x++)
	{
		currentFrameB=chainB->next;//tmp
		delete chainB;
		chainB=currentFrameB;
	}		
	frameCountB=frameCountA;
	fprintf(stderr,"[LOG] Total frames %d\n",frameCountA);
	
	
	//////////SORT DECODED FRAMES
	fprintf(stderr,"[LOG] SORTING DECODED FRAMES\n");
	int firstUnmarked=0;
	char Kc;
	uint64_t totaltotalSize=0;

	//arrange in array
	frameA* arrayA= new frameA[frameCountA];
	frameB* arrayB= new frameB[frameCountB];
	for (x=0;x<frameCountA;x++)
	{
		arrayA[x].type=chainA->type;
		arrayA[x].size=chainA->size;
		totaltotalSize+=chainA->size;
		arrayA[x].byte_position=chainA->byte_position;
		
		arrayB[x].avg_qp=chainB->avg_qp;
		arrayB[x].stdevQ_qp=chainB->stdevQ_qp;
		arrayB[x].marked=0;
		arrayB[x].qpmin=chainB->qpmin;
		arrayB[x].qpmax=chainB->qpmax;
		
		currentFrameA=chainA->next;
		currentFrameB=chainB->next;
		delete chainA;
		delete chainB;
		chainA=currentFrameA;
		chainB=currentFrameB;		
	}
	//sort
	for(x=0;x<frameCountA;x++)
	{
		c=firstUnmarked;
		Kc=0;
		for(y=firstUnmarked;y<frameCountA;y++)
		{			
			if (arrayA[y].byte_position<arrayA[x].byte_position) c++;
			
			if (arrayA[y].type=='K') Kc++;
			if (Kc==2) break;  //search max in following gop, up to last K included	
		}
		arrayA[x].avg_qp=arrayB[c].avg_qp;
		arrayA[x].stdevQ_qp=arrayB[c].stdevQ_qp;
		arrayA[x].qpmin=arrayB[c].qpmin;
		arrayA[x].qpmax=arrayB[c].qpmax;
				
		if(arrayB[c].marked==1)
		{
			fprintf(stderr,"[LOG] ERROR WHILE SORTING DECODED FRAMES - Exit program\n");
			return ERR_ALREADY_MARKED;
		}
		
		arrayB[c].marked=1;
		for(y=firstUnmarked;y<frameCountA;y++)
		{
			if(arrayB[y].marked==0)
			{
				firstUnmarked=y;
				break;
			}
		}
	}
	for(x=0;x<frameCountA;x++)
	{
		if(arrayB[x].marked==0)
		{
			fprintf(stderr,"[LOG] ERROR AFTER SORTING DECODED FRAMES - Exit program\n");
			return ERR_NOT_MARKED;
		}
	}
	delete [] arrayB;
	
	
	////////////////////////////OUTPUT
		
	//WRITE GLOBAL STATISTICS
	char type[] ="0BP1KI2";
	int l=strlen(type);
	uint64_t totalSize;
	
	uint32_t n;
	double mSize;
	double mQP;
	double stdevF;
	double stdevMB;

	fprintf(stderr,"[LOG] WRITING STATISTIC TO STDOUT\n");
	for(x=0;x<l;x++)
	{
		n=0;
		totalSize=0;
		mSize=0.0;
		mQP=0.0;
		stdevF=0.0;
		stdevMB=0.0;
		
		for (y=0;y<frameCountA;y++)
		{
			if(checktype(&(arrayA[y]),type[x]))
			{
				totalSize+=arrayA[y].size;
				n++;
				mQP+=arrayA[y].avg_qp;				
			}
		}
		mSize=totalSize;
		mSize/=n;
		mQP/=n;
		for (y=0;y<frameCountA;y++)
		{
			if(checktype(&(arrayA[y]),type[x]))
			{
				tmpd=mQP-arrayA[y].avg_qp;
				tmpd*=tmpd;
				stdevF+=tmpd;
				stdevMB+=arrayA[y].stdevQ_qp;			
				stdevMB+=tmpd;				
			}
		}
		stdevF/=n;
		stdevMB/=n;
		stdevF=sqrt(stdevF);
		stdevMB=sqrt(stdevMB);
		if (type[x]=='0') printf("TOT");
		else if (type[x]=='1') printf("K+I");
		else if (type[x]=='2') printf("P+B");
		else putchar(type[x]);
		printf(":\n");
		
		printf("N               : %d\n",n);
		printf("N/TOT           : %f %%\n",100.0*(double)(n)/(double)(frameCountA));
		printf("Total Size      : %s\n",std::to_string(totalSize).c_str());
		printf("Total Size/TOT  : %f %%\n",100.0*(double)(totalSize)/(double)(totaltotalSize));
		printf("Average Size    : %f\n",mSize);
		printf("AVERAGE QP      : %f\n",mQP);
		printf("StdDev (frames) : %f\n",stdevF);
		printf("StdDev (MB)     : %f\n\n",stdevMB);
	}
	//CONSECUTIVE B
	double totB=0.0;
	int maxB=0;
	int ccounterB=0;
	int* cBf=new int[16];
	for (x=0;x<16;x++) cBf[x]=0;
	for (y=0;y<frameCountA;y++)
	{
		if (arrayA[y].type=='B')
		{
			ccounterB++;
		}
		else if(ccounterB>0)
		{
			if (ccounterB<=16) cBf[ccounterB-1]+=ccounterB;
			maxB=( ccounterB > maxB ? ccounterB : maxB );
			totB=totB+ccounterB;
			ccounterB=0;			
		}	
	}
	printf("Consecutive B-Frames (number of B-Frames in each group) :");
	if (maxB==0) printf("  # N/A");
	printf("\n");
	for(x=0;x<maxB;x++)
		printf("%d : %d ( %f %% )\n",x+1,cBf[x],100.0*cBf[x]/totB);
	delete [] cBf;
	
	//PER FRAME STATISTICS
	if (argv==3)
	{			
		FILE* out=fopen(argc[2],"wb");
		if (out==NULL)
		{
			fprintf(stderr,"[LOG] FAILED OPENING OUTPUT STATS FILE\n");
			return ERR_OUTPUT_STATS;	
		}
		fprintf(stderr,"[LOG] WRITING PER-FRAME STATS FILE\n");
		fputs(std::to_string(frameCountA).c_str(),out);//frames number at first line
		fputc('\n',out);
		for (y=0;y<frameCountA;y++)
		{			
			fputc(arrayA[y].type,out);
			fputc('\t',out);
			fputs(std::to_string(arrayA[y].size).c_str(),out);
			fputc('\t',out);
			fputs(std::to_string(arrayA[y].byte_position).c_str(),out);
			fputc('\t',out);
			fputs(std::to_string(arrayA[y].avg_qp).c_str(),out);
			fputc('\t',out);
			fputs(std::to_string(sqrt(arrayA[y].stdevQ_qp)).c_str(),out);
			fputc('\t',out);
			fputs(std::to_string(arrayA[y].qpmin).c_str(),out);
			fputc('\t',out);
			fputs(std::to_string(arrayA[y].qpmax).c_str(),out);
			fputc('\n',out);	
		}
		fclose(out);
	}	
	//CLOSING
	fprintf(stderr,"[LOG] SCRIPT COMPLETE\n");	
	return 0;
}/////////////END

