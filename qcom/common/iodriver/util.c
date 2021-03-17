#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <string.h>
#include <errno.h>

#include <ctype.h>

#include <libgen.h>

#include "log.h"


bool file_exists(const char * filename)
{
	int s;
	struct stat st;

	s = stat(filename, &st);
	if(0 == s)
		return true;
	return false;
}

void log_hex(void * vdata, size_t len)
{
	uint8_t *data = vdata;
	char buffer[81];
	int i;

	for(i = 0; i < (int)len; i += 16)
	{
		int j;
		for(j = 0; j < 16; ++j)
		{
			if(i+j<(int)len)
				sprintf(buffer + (j*3), "%02x ", data[i+j]);
			else
				sprintf(buffer + (j*3), "   ");
		}

		for(j = 0; j < 16; ++j)
		{
			if(i+j<(int)len)
				sprintf(buffer + (16*3) + j, "%c", isprint(data[i+j]) ? data[i+j] : '.');
			else
				sprintf(buffer + (16*3) + j, " ");
		}
		DINFO("%s", buffer);
	}
}
