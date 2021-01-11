
#ifndef __MIC_FRAME_H__
#define __MIC_FRAME_H__

#include <stdint.h>
#include <stdbool.h>
typedef struct
{
	uint8_t * data;
	size_t data_alloc;
	size_t data_len;

	bool escape_flag;
	bool data_ready;
	bool in_frame;
} frame_t;

extern void frame_reset(frame_t * frame);
extern void frame_setbuffer(frame_t * frame, uint8_t * buffer, size_t len);
extern int frame_process_buffer(frame_t * frame, uint8_t * buffer, size_t len);

extern  int frame_encode(uint8_t * s, uint8_t * d, int len);
extern bool frame_data_ready(frame_t * frame);

#endif //__MIC_FRAME_H__

