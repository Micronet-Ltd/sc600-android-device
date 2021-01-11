#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <libgen.h>
#include <linux/limits.h>

#include <limits.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <pthread.h>
#include <sched.h>

#include <string.h>
#include <errno.h>


#include "log.h"
#include "misc.h"
#include "queue.h"
#include "frame.h"
#include "util.h"
#include "tty.h"

struct accel_thread_context
{
	char name[PATH_MAX];
};

/// functions



void * accel_proc(void * cntx)
{
	struct accel_thread_context * context = cntx;
	int r;
	int fd_mcu = -1;
	int fd_dev = -1;
	frame_t frame;
	uint8_t databuffer[1024]; // >= 10 * (8*2)
	uint8_t readbuffer[1024]; // >= 10 * (8 * 2) * 2 + 2 + slop ( * 2 + XX)

	frame_setbuffer(&frame, databuffer, sizeof(databuffer));


	while(!file_exists(context->name)) {
		//DINFO("Waiting for '%s'", context->name);
		sleep(1);
	}


	do
	{
		int max_fd = -1;
		// Accelerometer port
		if(-1 == fd_mcu)
		{
			if(file_exists(context->name))
			{
				// TODO: this should be runtime configurable
				// Don't open tty device file if disabled for diagnostics
				//if(!file_exists("/data/disable_accel_tty_open"))
					fd_mcu = open_serial(context->name);
			}
		}

		// Accel kernel driver interface to apps
		if(-1 == fd_dev)
		{
			if(file_exists("/dev/vaccel"))
			{
				// Don't open device file if disabled for diagnostics
				//if(!file_exists("/data/disable_accel_dev_open"))
					fd_dev = open("/dev/vaccel", O_RDWR, O_NDELAY);
			}
		}

		// NOTE: fd_dev is not an fd that will be waited on
		if(fd_mcu > max_fd) max_fd = fd_mcu;
		DTRACE("max_fd = %d", max_fd);

		/*
		if(max_fd < 0)
		{
			//DTRACE("Wait for device\n");
			sleep(1);
			continue;
		}
		*/

		do
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd_mcu, &fds);
			r = select(fd_mcu + 1, &fds, NULL, NULL, NULL);
		} while(r == -1 && EINTR == errno);

		if(1 != r)
		{
			if(-1 == r)
				DERR("select: %s", strerror(errno));
			else
				DERR("select did not return 1");
			abort();
		}

		r = read(fd_mcu, readbuffer, sizeof(readbuffer));
		if( r < 0)
		{
			if(EAGAIN == errno)
				continue;
			DERR("read: %s", strerror(errno));
			close(fd_mcu);
			fd_mcu = -1;
			continue;
		}
		if(0 == r)
		{
			DTRACE("port closed");
			close(fd_mcu);
			fd_mcu = -1;
			continue;
		}

		int offset;

		offset = 0;
		// TODO: verifiy signedness bugs
		while(r - offset > 0)
		{
			int tmp_offset;

			tmp_offset = frame_process_buffer(&frame, readbuffer + offset, r - offset);

			if(tmp_offset <= 0)
			{
				DERR("offset is <= 0");
				abort();
			}
			offset += tmp_offset;

			if(frame_data_ready(&frame))
			{
				if(frame.data_len == 68)
				{
					int w;
					uint8_t data[10*16];
					int i;
					uint8_t ts_data[8];
					/*
					 *  Accel data input format:
					 *   Header:
					 *     <u64> timestamp  
					 *   Data samples * 10
					 *     <u16> <u16> <u16> x,y,z sample
					 *
					 *  Output format:
					 *   Data * 10  (timestamp per sample)
					 *     <u64> timestamp
					 *     <u16> <u16> <u16> x,y,z sample (Bytes swapped for compatability)
					 *     <u16>=0 padding
					 */

					memset(data, 0, sizeof(data));
					memcpy(data, frame.data, 8); // Copy timestamp as is and first xyz sample 
					memcpy(data + 8, frame.data + 8, 6);

					memcpy(ts_data, data, 8); // copy Just time stampe
					// TODO: make functions, verify this is correct
					for(i = 0; i < 10; ++i)
					{
						uint8_t * p_samp_in = frame.data + 8 + (i*6);
						uint8_t * p_samp_out = data + (i*16)+8;
						// NOTE: IVMM does not icrement ts per bundle
						memcpy(data + (i*16), ts_data, 8); // copy timestamp

						// Inthinc DMM reverses the bytes, this just reverses the the pairs
						p_samp_out[0] = p_samp_in[1];
						p_samp_out[1] = p_samp_in[0];

						p_samp_out[2] = p_samp_in[3];
						p_samp_out[3] = p_samp_in[2];

						p_samp_out[4] = p_samp_in[5];
						p_samp_out[5] = p_samp_in[4];

						p_samp_out[6] = 0;
						p_samp_out[7] = 0;

					}

					if (fd_dev > -1)
					{
						w = write(fd_dev, data, sizeof(data));
						if(w != sizeof(data))
						{
							if(-1 == w)
							{
								DERR("write: %s", strerror(errno));
								close(fd_dev);
								fd_dev = -1;
								break;
							}
						}
					}
				}
				frame_reset(&frame);
			}
		}

	} while(1);

	return NULL;
}
