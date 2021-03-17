#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "frame.h"

// Frame characters
// See PPP
#define FEND 0x7e
#define FESC 0x7d
#define TXOR 0x20
#define XOR_VALUE(x) ((x) ^ TXOR)
#define FTEND XOR_VALUE(FEND)	// 0x5e
#define FTESC XOR_VALUE(FESC)	// 0x5d

// TODO: this is based on old code, may have some bugs which were fixed.

// buffer d is atleast 2*len+2, assuming all bytes are escaped and 2 FEND.
int frame_encode(uint8_t *s, uint8_t *d, int len)
{
	uint8_t *p = d;
	uint8_t c;

	*p++ = FEND;

	while (len-- > 0)
	{
		c = *s++;
		switch (c)
		{
			case FEND:
			case FESC:
				*p++ = FESC;
				*p++ = XOR_VALUE(c);
				break;

			default:
				*p++ = c;
				break;
		}
	}
	*p++ = FEND;

	return p - d;
}

void frame_reset(frame_t * frame)
{
	frame->data_len = 0;
	frame->escape_flag = false;
	frame->data_ready = false;
	frame->in_frame = false;
}

void frame_setbuffer(frame_t * frame, uint8_t *buffer, size_t len)
{
	frame_reset(frame);
	frame->data = buffer;
	frame->data_alloc = len;
}

inline static bool frame_input(frame_t * frame, uint8_t c)
{
	if (FEND == c)
	{
		if (frame->data_len > 0)
		{
			frame->data_ready = true;
			return true;
		}
		frame_reset(frame);
		// set in_frame since frame_reset will unset it, this should be start FEND
		frame->in_frame = true;
		return false;
	}
	// ensure we have recieved a FEND since a reset
	if(!frame->in_frame)
		return false;
	if (FESC == c)
	{
		frame->escape_flag = true;
		return false;
	}

	if (frame->escape_flag)
	{
		c = XOR_VALUE(c);
		frame->escape_flag = false;
	}


	if(frame->data_len >= frame->data_alloc)
	{
		frame_reset(frame);
		return false;
	}
	frame->data[frame->data_len++] = c;

	return false;
}


int frame_process_buffer(frame_t * frame, uint8_t *buffer, size_t len)
{
	int i;

	for(i = 0; i < len; i++)
	{
		if(frame_input(frame, buffer[i]))
			return i+1;
	}

	return i;
}


bool frame_data_ready(frame_t * frame)
{
	return frame->data_ready;
}
