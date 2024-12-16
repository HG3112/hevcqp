#include "utils.h"
#include "math.h"

bool checktype(frame* f, char c)//used in global analysis
{
	if (c == '0') return true; //TOT
	if (c == '1') return ((f->type == 'K') || (f->type == 'I')); //K+I
	if (c == '2') return ((f->type == 'P') || (f->type == 'B')); //P+B
	return (f->type == c);
}

int readline(FILE* stream, char* buffer, int buf_size) { //standard pipe
	//Legge i caratteri finchè non trova '\n'
	//ritorna il numero di caratteri letti (eventualmente 0)
	//ritorna -1 in caso di errore o EOF
	//ritorna -2 in caso di overflow

	int ccounter = 0;
	int c;
	char* cptr = buffer;

	while (1)
	{
		if (ccounter == buf_size - 1) return -2;
		c = fgetc(stream);
		if (c == EOF)
		{
			*cptr = 0;
			return -1;
		}
		if ((c == '\n') || (c == 0))
		{
			*cptr = 0;
			return ccounter;
		}
		*cptr = c;
		cptr++;
		ccounter++;
	}
}

int readline(HANDLE stream, char* buffer, int buf_size) { //named pipe
	//Legge i caratteri finchè non trova '\n'
	//ritorna il numero di caratteri letti (eventualmente 0)
	//ritorna -1 in caso di errore o EOF
	//ritorna -2 in caso di overflow

	int ccounter = 0;
	//int c;
	char* cptr = buffer;

	while (1)
	{
		if (ccounter == buf_size - 1) return -2;

		if (!ReadFile(stream, cptr, 1, NULL, NULL))
			return -1;
		if ((*cptr == '\n') || (*cptr == 0))
		{
			*cptr = 0;
			return ccounter;
		}
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
	int64_t segno = 1;
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
		expn = (int) (sttoint(pp + 1));
	}
	return segno * n * pow(10.0, expn);
}