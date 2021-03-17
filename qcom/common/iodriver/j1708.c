/* J1708 IOCONTROL driver
   Ruslan Sirota <ruslan.sirota@micronet-inc.com
   */


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
#include "j1708.h"

//#define J1708_DEBUG

#define MIC_J1708_MAX_DATA_LEN ((21<<1)+2)

void * j1708_proc(void * cntx)
{
	struct j1708_thread_context * context = cntx;
	int fd_mcu = 		-1;
	int fd_dev = 		-1;
	int fd_max =		-1;
	int ready, tmp;
	struct timeval tv;
	fd_set fds;

	frame_t frame;
	uint8_t databuffer[MIC_J1708_MAX_DATA_LEN];
	uint8_t readbuffer[MIC_J1708_MAX_DATA_LEN];

	do {
        while (!file_exists(context->name) && context->run) {
			DTRACE("Waiting for '%s'", context->name);
			if (-1 != fd_mcu) {
				close (fd_mcu);
				fd_mcu = -1;
			}
			if (-1 != fd_dev) {
				close (fd_dev);
				fd_dev = -1;
			}
			fd_max = -1;
			sleep(1);
		}
        if (!context->run) {
            if (-1 != fd_mcu) {
                close (fd_mcu);
                fd_mcu = -1;
            }
            if (-1 != fd_dev) {
                close (fd_dev);
                fd_dev = -1;
            }
            break;
        }

		//MCU tty port
		if(-1 == fd_mcu)
		{
			if(file_exists(context->name))
			{
				fd_mcu = open_serial(context->name);
				if (0 > fd_mcu ) {
					DERR("Error open %s", context->name);
					sleep(5);
					continue;
				}
			}
			if (fd_mcu > fd_max)
				fd_max = fd_mcu;
		}

		//app port data out
		if(-1 == fd_dev)
		{
			if(file_exists("/dev/mcu_j1708"))
			{
				fd_dev = open("/dev/mcu_j1708", O_RDWR, O_NDELAY);
				if (0 > fd_dev ) {
					DERR("Error open /dev/mcu_j1708 %s", strerror(errno));
					sleep(5);
					continue;
				}
				if (fd_dev > fd_max)
					fd_max = fd_dev;
			}
		}

		do
		{
			/* Wait up to five seconds. */
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			FD_ZERO(&fds);
			FD_SET(fd_mcu, &fds);
			FD_SET(fd_dev, &fds);
			ready = select(fd_max + 1, &fds, NULL, NULL, &tv);
		} while(ready == -1 && EINTR == errno);

		if (0 == ready) {
			//Time out on waiting
			continue;
		}

		if (0 > ready) {
			DERR("Error select  %s", strerror(errno));
			sleep (5);
			continue;
		}

		if ( FD_ISSET (fd_mcu, &fds) ) {

			ready = read(fd_mcu, readbuffer, sizeof(readbuffer));
			if( 0 > ready) {
				if(EAGAIN == errno)
					continue;
				DERR("read fd_mcu: %s", strerror(errno));
				close(fd_mcu);
				fd_mcu = -1;
				continue;
			}
			if(0 == ready) {
				DTRACE("port fd_mcu closed");
				close(fd_mcu);
				fd_mcu = -1;
				continue;
			}
			if (MIC_J1708_MAX_DATA_LEN < ready) {
				DERR ("Error data read len %d", ready);
				abort();
			}

#ifdef J1708_DEBUG
			//Print message
			{
				int count;
				for (count = 0; count < ready; count++) {
					DERR("Byte %d -- %x", count, readbuffer[count] );
				}
			}
#endif
			frame_setbuffer(&frame, databuffer, sizeof(databuffer));

			ready = frame_process_buffer(&frame, readbuffer, ready);

#ifdef J1708_DEBUG
			//Print message
			{
				int count;
				for (count = 0; count < ready; count++) {
					DERR("RByte %d -- %x", count, readbuffer[count] );
				}
			}
#endif

			if(0 >= ready) {
				DERR("frame_process_buffer return ready is <= 0");
				abort();
			}

			if(frame_data_ready(&frame)) {
				tmp = write(fd_dev, frame.data, frame.data_len);
				if(-1 == tmp)
				{
					DERR("fd_dev %x datalen %x",(unsigned int)fd_dev, (unsigned int)(frame.data_len) );
					DERR("write: %s", strerror(errno));
					close(fd_dev);
					fd_dev = -1;
					continue;
				}
			}
			else {
				DERR("frame_process_buffer data integrity error");
				abort();
			}
			frame_reset(&frame);
		} // if ( FD_ISSET (fd_mcu, &fds) )

		if ( FD_ISSET (fd_dev, &fds) ) {
			ready = read(fd_dev, readbuffer, sizeof(readbuffer));
			if( 0 > ready) {
				if(EAGAIN == errno)
					continue;
				DERR("read fd_dev: %s", strerror(errno));
				close(fd_mcu);
				fd_mcu = -1;
				continue;
			}
			if(0 == ready) {
				DTRACE("port fd_dev closed");
				close(fd_mcu);
				fd_mcu = -1;
				continue;
			}
			if (MIC_J1708_MAX_DATA_LEN < ready) {
				DERR ("Error data read len %d", ready);
				abort();
			}

			ready = frame_encode (readbuffer, databuffer, ready );
			if (0 < ready) {
				tmp = write(fd_mcu, databuffer, ready);
				if(-1 == tmp)
				{
					DERR("write encoded: %s", strerror(errno));
					close(fd_dev);
					fd_dev = -1;
					continue;
				}
			}

		}
		
	} while (1);

	return NULL;
}

