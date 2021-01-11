/** Anthony Best <anthony.best@micronet-inc.com>
 *  Userspace driver for OBC MCU
 */
// FIXME: This should be the POSIX compatability flag, and the function that depends on this should be commented
//#define _GNU_SOURCE
#include <stdio.h>

#define DEBUG_TRACE

#define __need_timespec
#define __USE_POSIX199309
#define __USE_XOPEN2K
#define __USE_MISC

#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

#include <limits.h>

#include <sys/un.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <pthread.h>
#include <sched.h>

#include <string.h>
#include <strings.h>
#include <errno.h>

#include <sys/select.h>

#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "log.h"
#include "misc.h"
#include "queue.h"
#include "frame.h"
#include "util.h"
#include "tty.h"

#include "control.h"
#include "api_constants.h"
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <cutils/properties.h>

//#define IO_CONTROL_RECOVERY_DEBUG 1
#define TIME_BETWEEN_MCU_PINGS 2 /* 2 sec */
#define ONE_WIRE_PKT_SIZE 		8

#if defined (IO_CONTROL_RECOVERY_DEBUG)
#define IO_CONTROL_LOG "/cache/io_control.log"
static void redirect_stdio(const char* filename)
{
    // If these fail, there's not really anywhere to complain...
    freopen(filename, "a", stdout);
    setbuf(stdout, 0);
    freopen(filename, "a", stderr);
    setbuf(stderr, 0);
}
#endif

static int send_sock_data(struct control_thread_context * context, struct sockaddr_un * addr, uint8_t * data, size_t len);
static bool set_app_watchdog_expire_time(int max_time);
static int get_app_watchdog_expire_time(void);
static bool set_app_watchdog_count(int count);
static bool get_app_watchdog_count(int * count);

static uint8_t control_get_seq(struct control_thread_context * context)
{
	return context->seq++;
}

static int control_get_status_message(char * buff, size_t size)
{
	snprintf(buff, size, "No status");
	return 0;
}

static int control_open_socket(struct control_thread_context * context __attribute__((unused)))
{
	struct sockaddr_un s_addr = {0};
	int fd;
	//int option = 1;

	s_addr.sun_family = AF_UNIX;
	strncpy(s_addr.sun_path, UD_NAMESPACE, sizeof(s_addr.sun_path) - 1);
	s_addr.sun_path[0] = '\0'; // abstract socket namespace, replace '#' with '\0'

	if( -1 == (fd = socket(AF_UNIX, SOCK_DGRAM, 0)))
	{
		DERR("socket: %s", strerror(errno));
		exit(-1);
	}

	//setsockopt(fd,SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option));

	if(-1 == bind(fd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr_un)))
	{
		DERR("bind: %s", strerror(errno));
		close(fd);
		exit(-1);
	}

	return fd;
}

// returns < 0 on error, 0 on io, others undefined
static int control_thread_wait(struct control_thread_context * context)
{
	int r;
	int max_fd = -1;

	if(max_fd < context->mcu_fd)
		max_fd = context->mcu_fd;
	if(max_fd < context->sock_fd)
		max_fd = context->sock_fd;
	if(max_fd < context->gpio_fd)
		max_fd = context->gpio_fd;
    if(max_fd < context->vled_fd)
        max_fd = context->vled_fd;

    struct timeval tv = {1, 0};

	//DTRACE("max_fd=%d", max_fd);

	// TODO: this may need to handle other conditions, it may need to be refactored
	if(max_fd < 0)
		return -1;

	do
	{
		FD_ZERO(&context->fds);

		if(context->mcu_fd >= 0)
		{
			FD_SET(context->mcu_fd, &context->fds);
			//DTRACE("FD_SET fd");
		}

		if(context->sock_fd >= 0)
		{
			FD_SET(context->sock_fd, &context->fds);
			//DTRACE("FD_SET sock");
		}

		if(context->gpio_fd >= 0)
		{
			FD_SET(context->gpio_fd, &context->fds);
			DTRACE("FD_SET gpio");
		}

        if (context->vled_fd >= 0) {
            FD_SET(context->vled_fd, &context->fds);
            DTRACE("select vled_fd");
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;
		//DTRACE("max_fd=%d about to select %d:%d", max_fd, (int)tv.tv_sec, (int)tv.tv_usec);
		r = select(max_fd+1, &context->fds, NULL, NULL, &tv);

	} while(-1 == r && EINTR == errno);

	//DTRACE("r = %d", r);

	if(r == -1)
	{
		DERR("select: %s", strerror(errno));
		return -1;
	}

	if( r > 0)
	{
		if( (context->mcu_fd > -1) && FD_ISSET(context->mcu_fd, &context->fds))
			return 0;
		if( (context->sock_fd > -1) && FD_ISSET(context->sock_fd, &context->fds))
			return 0;
		if( (context->gpio_fd > -1) && FD_ISSET(context->gpio_fd, &context->fds))
			return 0;
        if ((context->vled_fd > -1) && FD_ISSET(context->vled_fd, &context->fds)) {
            DTRACE("vled selected");
            return 0;
        }

		//DTRACE("select returned %d but no fd is set", r);
	}
	else
	{
		//DTRACE("no data idle");
		return 0;
	}
	return -1;
}

static int  control_send_mcu(struct control_thread_context * context, uint8_t * msg, size_t len)
{
	uint8_t encoded_buffer[1024*2+2];
	int r = -1;

	r = frame_encode(msg, encoded_buffer, len);
	if(r > 0)
	{
		size_t st;
		st = write(context->mcu_fd, encoded_buffer, r);
		if(st != (size_t)r)
		{
			DERR("Bytes written don't match request: %s", strerror(errno));
			return errno;
		}
		return 0;
	}
	return -1;
}

static int control_gpio_input(struct control_thread_context * context, uint8_t mask, uint8_t value)
{
	if(context->gpio_fd >= 0)
	{
		int r;
		uint8_t data[4] = {0, 0, 0, 0};
		data[2] = mask;
		data[3] = value;
		// The driver should never block, or return -EAGAIN, if the driver changes
		// this will need to be updated. NOTE: this can not block, so take care
		DTRACE("%s [%x, %x]", __func__, mask, value);
		r = write(context->gpio_fd, data, sizeof(data));
		if(r != sizeof(data))
		{
			DTRACE("%s: write error", __func__);
			if(-1 == r && -EAGAIN == errno)
				DTRACE("%s:EAGAIN should not happen", __func__);
			else if(-1 == r && -EACCES == errno)
				DTRACE("%s: EACCES memory is likely corrupted", __func__);
			else if(-1 == r && -EINVAL == errno)
				DTRACE("%s: EINVAL data is invalid", __func__);
			else if(-1 == r)
			{
				DERR("%s: write: %s", __func__, strerror(errno));
				return r;
			}
			DTRACE("%s: r = %d", __func__, r);
		}
	}
	return 0;
}

static int control_one_wire_data(struct control_thread_context * context, uint8_t * data, uint8_t data_size)
{
	if(context->one_wire_fd >= 0)
	{
		int r;
		// The driver should never block, or return -EAGAIN, if the driver changes
		// this will need to be updated. NOTE: this can not block, so take care
		DINFO("%s: [%x, %x, %x, %x, %x, %x, %x, %x]", __func__,
				data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		r = write(context->one_wire_fd, data, data_size);
		if(r != data_size)
		{
			DTRACE("%s: write error", __func__);
			if(-1 == r && -EAGAIN == errno)
				DTRACE("%s:EAGAIN should not happen", __func__);
			else if(-1 == r && -EACCES == errno)
				DTRACE("%s: EACCES memory is likely corrupted", __func__);
			else if(-1 == r && -EINVAL == errno)
				DTRACE("%s: EINVAL data is invalid", __func__);
			else if(-1 == r)
			{
				DERR("%s: write: %s", __func__, strerror(errno));
				return r;
			}
			DTRACE("%s: r = %d", __func__, r);
		}
	}
	else
	{
		DERR("%s : invalid one_wire_fd %d", __func__, context->one_wire_fd);
		return -1;
	}
	return 0;
}


static inline retrieve_gpio_value_from_mcu(struct control_thread_context *context, uint8_t value)
{
	if (context->gpio_fd >= 0)
	{
		int r;
		uint8_t data[4] = {0, 0, 0, 0};//the reason why I didn't just write a single value is to keep this function
									   //synchronized with control_gpio_input
		data[2] = value;

		// The driver should never block, or return -EAGAIN, if the driver changes
		// this will need to be updated. NOTE: this can not block, so take care
		DTRACE("%s [%x]", __func__, value);
		r = write(context->gpio_fd, data, sizeof(data));
		if (r != sizeof(data))
		{
			DTRACE("%s: write error", __func__);
			if (-1 == r && -EAGAIN == errno)
				DTRACE("%s:EAGAIN should not happen", __func__);
			else if (-1 == r && -EACCES == errno)
				DTRACE("%s: EACCES memory is likely corrupted", __func__);
			else if (-1 == r && -EINVAL == errno)
				DTRACE("%s: EINVAL data is invalid", __func__);
			else if (-1 == r)
			{
				DERR("%s: write: %s", __func__, strerror(errno));
				return r;
			}
			DTRACE("%s: r = %d", __func__, r);
		}
	}
	return 0;
}

static int control_frame_process(struct control_thread_context *context, uint8_t *data, size_t len)
{
	// NOTE: Do not block here.
	// TODO: check for sequence gaps
	//int seq = data[1]; // TODO:
	packet_type_enum packet_type;

	if(len < 1)
		return -1;
	if(len > (1024 + 2))
		return -2;

	packet_type = (packet_type_enum)data[1];

	// TODO: handle messages
	switch (packet_type)
	{
		case SYNC_INFO:	// Sync/Info
			DTRACE("control_frame_process: Packet type SYNC_INFO !!!");
			break;

		case COMM_WRITE_REQ: // Write register
			data[0] = control_get_seq(context);
			data[1] = (uint8_t)COMM_WRITE_REQ;
			control_send_mcu(context, data, len);
			break;

		case COMM_READ_REQ: // Register read request
			data[0] = control_get_seq(context);
			data[1] = (uint8_t)COMM_READ_REQ;
			control_send_mcu(context, data, len);
			break;

	case COMM_READ_RESP: // register read response
		DERR("COMM_READ_RESPONSE: %x, %x, %x ... %x, %x, len= %d",
			   data[2], data[3], data[4], data[len - 2], data[len - 1], (int)len);
		if (context->rtc_req == true)
		{
			memcpy(context->rtc_init_val, &data[3], sizeof(context->rtc_init_val));
			context->rtc_req = false;
		}
		else if (true == context->dont_send)
		{
			context->dont_send = false;
		}
		else if (MAPI_GET_MCU_GPIO_STATE_DBG == data[2])
		{
			retrieve_gpio_value_from_mcu(context, data[3]);
		}
		else
		{
			send_sock_data(context, context->sock_resp_addr, &data[2], len);
		}
		context->sock_resp_addr = 0;
		break;

		case PING_REQ: // PING request
			{
				uint8_t msg[2];
				msg[0] = control_get_seq(context);
				msg[1] = (uint8_t)PING_RESP;
				return control_send_mcu(context, msg, sizeof(msg));
			}
			break;

		case PING_RESP: // PING response
			DTRACE("PING_RESP: %d", context->pong_recv);
			context->pong_recv++;
			break;

		case GPIO_INT_STATUS: // GPIO Interrupt
			DTRACE("%s: GPIO_INT_STATUS data2 %d data3 %d", __func__, data[2], data[3]);
			control_gpio_input(context, data[2], data[3]);
			break;

		case ONE_WIRE_DATA: // one-wire data has been received
			DTRACE("%s: ONE_WIRE_DATA %02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x \n",__func__,\
					data[2], data[3], data[4], data[5], \
					data[6], data[7], data[8], data[9]);
			control_one_wire_data(context, &data[2], ONE_WIRE_PKT_SIZE);
			break;

		default:
			DTRACE("%s: invalid case %d\n", __func__, packet_type);
			break;
	}
	return 0;
}

static int control_receive_mcu(struct control_thread_context * context)
{
	ssize_t bytes_read; // NOTE signed type
	int offset;
	uint8_t readbuffer[1024];
	int ret;
	fd_set set;
	struct timeval timeout;
	/* Initialize the file descriptor set. */
	FD_ZERO (&set);
	FD_SET (context->mcu_fd, &set);

	/* Initialize the timeout data structure. */
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	DTRACE("");
	/* select returns 0 if timeout, 1 if input available, -1 if error. */
	ret = select(FD_SETSIZE, (fd_set *)&set, NULL, NULL, &timeout);
	if (ret != 1) {
		DERR("control_receive_mcu read select failure:ret=%d , %s", ret, strerror(errno));
		if (ret == -1) {
            close(context->mcu_fd);
			context->running = false;
		}
		return -1;
	}
	bytes_read = read(context->mcu_fd, readbuffer, sizeof(readbuffer));
	if (bytes_read < 0) {
		if(EAGAIN == errno)
			return 0;
		DERR("read: %s", strerror(errno));
		close(context->mcu_fd);
		context->mcu_fd = -1;
		frame_reset(&context->frame);
		return -1;
		//abort();
	}

	if (0 == bytes_read) {
		DTRACE("port closed");
		return -1;
	}

	offset = 0;
	// NOTE: bytes_read and offset are signed types
	// bytes_read and offset must be positive
	while (bytes_read - offset > 0) {
		offset += frame_process_buffer(&context->frame, readbuffer + offset, bytes_read - offset);
		if(offset <= 0) {
			DTRACE("offset is <= 0");
			abort();
		}
		if (frame_data_ready(&context->frame)) {
			int status;
			//process data
			status = control_frame_process(context, context->frame.data, context->frame.data_len);
			if(status)
				DTRACE("status != 0");
			frame_reset(&context->frame);
		}
	}

	//DTRACE("TODO: Read message from MCU");
	//DTRACE("TODO: Handle messages");
	//DTRACE("TODO: Check for more messages");

	return 0;
}

static int send_sock_data(struct control_thread_context * context, struct sockaddr_un * addr, uint8_t * data, size_t len)
{
	socklen_t sock_len;
	int r;

	sock_len = sizeof(struct sockaddr_un);

	if(len >= SOCK_MAX_MSG)
	{
		 DERR("msg len(%u) >= SOCK_MAX_MSG", (unsigned int)len);
		 return -1;
	}

	r = sendto(context->sock_fd, data, len, 0, (struct sockaddr*)addr, sock_len);
	DTRACE("r = %d", r);
	if(-1 == r)
	{
		DERR("sendto: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static int send_sock_string_message(struct control_thread_context * context, struct sockaddr_un * addr, char * msg)
{
	char buf[SOCK_MAX_MSG];
	socklen_t sock_len;
	size_t len;
	int r;

	sock_len = sizeof(struct sockaddr_un);

	len = strlen(msg);
	if(0 == len)
	{
		DERR("msg len = 0");
		return -1;
	}
	if(len >= SOCK_MAX_MSG)
	{
		 DERR("msg len(%u) >= SOCK_MAX_MSG", (unsigned int)len);
		 return -1;
	}
	buf[0] = '\0';
	memcpy(buf+1, msg, len);

	r = sendto(context->sock_fd, buf, (1+len), 0, (struct sockaddr*)addr, sock_len);
	DTRACE("r = %d", r);
	if(-1 == r)
	{
		DERR("sendto: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static int hex_value(uint8_t hex)
{
	//Probably a better portable way of doing this
	// if(a >= '0' && a <= '9') return '0'-a;
	// return 'a' - tolower(a);
	switch(hex)
	{
		default:
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'a':
		case 'A': return 0xa;
		case 'b':
		case 'B': return 0xb;
		case 'c':
		case 'C': return 0xc;
		case 'd':
		case 'D': return 0xd;
		case 'e':
		case 'E': return 0xe;
		case 'f':
		case 'F': return 0xf;
	}
}

static int control_handle_sock_raw(struct control_thread_context * context, struct sockaddr_un * addr, uint8_t * hex_data, size_t len)
{
	uint8_t data[1024];
	uint32_t i;
	int32_t r;


	if(len%2)
	{
		DERR("Len not even, len=%u", (unsigned int)len);
		log_hex(hex_data, len);
		return -1;
	}

	for(i = 0; i*2 < len; i++)
	{
		data[i] = hex_value(hex_data[i*2]) << 4;
		data[i] |= hex_value(hex_data[i*2+1]);
	}
	data[0] = control_get_seq(context);

	r = control_send_mcu(context, data, i);
	if(r)
		return send_sock_string_message(context, addr, "ERROR");
	else
		return send_sock_string_message(context, addr, "OK");

}

static int control_handle_sock_command(struct control_thread_context * context, struct sockaddr_un * addr, uint8_t * data, size_t len)
{
	int r = -1;
	char buf[64] = {0};
	int wdg_max_time = 0, count = 0;

	if (len <= 0)
	{
		return -2;
	}

	if(0 == memcmp(data, "status", strlen("status")+1))
	{
		char statusbuf[SOCK_MAX_MSG-2];
		if(0 == control_get_status_message(statusbuf, sizeof(statusbuf)))
		{
			if(send_sock_string_message(context, addr, statusbuf))
				r = -1;
			else
				r = 0;
		}
	}
	else if(0 == memcmp(data, "check", strlen("check")+1))
	{
		if(send_sock_string_message(context, addr, "Running"))
			r = -1;
		else
			r = 0;
	}
	else if (0 == memcmp(data, "app_ping", strlen("app_ping")+1))
	{
		clock_gettime(CLOCK_MONOTONIC_RAW, &(context->last_app_ping_time));
		DINFO("app_ping %d\n", (int)(context->last_app_ping_time.tv_sec));
		if(send_sock_string_message(context, addr, "app_pong"))
			r = -1;
		else
			r = 0;
	}
	else if (0 == memcmp(data, "get_app_watchdog_time", strlen("get_app_watchdog_time")+1))
	{
		wdg_max_time = get_app_watchdog_expire_time();
		sprintf(buf, "get_app_watchdog_time=%d sec\r", wdg_max_time);
		DINFO("get_app_watchdog_time %d sec\n",wdg_max_time);
		if(send_sock_string_message(context, addr, buf))
			r = -1;
		else
			r = 0;
	}
	else if (0 == memcmp(data, "set_app_watchdog_time=", strlen("set_app_watchdog_time=")))
	{
		sscanf((const char *)data, "set_app_watchdog_time=%d\r", &wdg_max_time);

		/* make sure it's valid */
		if ((wdg_max_time >= 60 && wdg_max_time < 5000) || (wdg_max_time == 0) )
		{
			if (set_app_watchdog_expire_time(wdg_max_time))
			{
				DINFO("set_app_watchdog_time = %d sec\n",wdg_max_time);
				sprintf(buf, "set_app_watchdog_time=%d sec\r", wdg_max_time);
				context->max_app_watchdog_ping_time = wdg_max_time;
				clock_gettime(CLOCK_MONOTONIC_RAW, &(context->last_app_ping_time));
			}
			else
			{
				sprintf(buf, "ERROR:file write failure\r");
			}
		}
		else
		{
			DERR("Invalid app watchdog time: %d\n", wdg_max_time);
			sprintf(buf, "ERROR:invalid app_watchdog_time");
		}

		if(send_sock_string_message(context, addr, buf))
			r = -1;
		else
			r = 0;
	}
	else if (0 == memcmp(data, "get_app_watchdog_count", strlen("get_app_watchdog_count")+1))
	{
		if(get_app_watchdog_count(&count))
		{
			sprintf(buf, "get_app_watchdog_count=%d\r", count);
			DINFO("get_app_watchdog_count %d \n",count);
		}
		else
		{
			DERR("Could not get app watchdog count\n");
			sprintf(buf, "ERROR:could not get app watchdog count\r");
		}

		if(send_sock_string_message(context, addr, buf))
			r = -1;
		else
			r = 0;
	}
	else if (0 == memcmp(data, "clear_app_watchdog_count", strlen("clear_app_watchdog_count")+1))
	{
		if(set_app_watchdog_count(0))
		{
			sprintf(buf, "set_app_watchdog_count=%d\r", count);
			DINFO("get_app_watchdog_count %d \n",count);
		}
		else
		{
			DERR("Could not get app watchdog count\n");
			sprintf(buf, "ERROR:could not set app watchdog count\r");
		}

		if(send_sock_string_message(context, addr, buf))
			r = -1;
		else
			r = 0;
	}

	if(0 > r)
		return send_sock_string_message(context, addr, "ERROR");
	if(1 == r)
	   return send_sock_string_message(context, addr, "OK");
	if(0 == r)
		return 0;
	return -1;
}

static int control_handle_api_command(struct control_thread_context * context, struct sockaddr_un * addr, uint8_t * data, size_t len)
{
	int r = -1;

	uint8_t mdata[MAX_COMMAND_PACKET_SIZE];

	/* write req */
	if (data[0] == MAPI_WRITE_RQ)
	{
		mdata[1] = COMM_WRITE_REQ;
	}
	/* read req */
	else if (data[0] == MAPI_READ_RQ)
	{
		context->sock_resp_addr = addr; /* the response will be sent back on this address */
		mdata[1] = COMM_READ_REQ;
	}
	memcpy(&mdata[2], &data[1], len - 1);
	r = control_frame_process(context, mdata, len + 1);
	return r;
}

static int control_receive_gpio(struct control_thread_context * context)
{
	int r;
	int err = 0;
	const uint8_t errsize = 4;
	uint8_t data[6]; //Barak - as for now 31/3/2019, 6 is the maximal possible message size
	uint8_t size;
	const int GPIO_INT_STATUS_REQ_SIZE = 4;
	const int COMM_READ_REQ_SIZE = 5;
	const int COMM_WRITE_REQ_SIZE = 6;

	size = read(context->gpio_fd, data, sizeof(data));

	//DERR("size: %u",size);

	if (-1 == size)
	{
		DERR("read: %s", strerror(errno));
		return -1;
	}
	//DERR("read DATA1: %u %u %u %u %u enum %u	\n", data[0], data[1], data[2], data[3], data[4], (uint8_t)MAPI_GET_MCU_GPIO_STATE_DBG);

	data[0] = control_get_seq(context);

	if(GPIO_INT_STATUS_REQ_SIZE == size)
	{
		data[1] = (uint8_t)GPIO_INT_STATUS;
	}
	else if(COMM_READ_REQ_SIZE == size)
	{
		data[1] = (uint8_t)COMM_READ_REQ;
		data[2] = MAPI_GET_MCU_GPIO_STATE_DBG;
	}
	else if(COMM_WRITE_REQ_SIZE == size)
	{
		data[1] = (uint8_t)COMM_WRITE_REQ;
		data[2] = MAPI_SET_MCU_GPIO_STATE_DBG;
	}
	//DERR("read message1: %u %u %u %u %u enum %u	\n", data[0], data[1], data[2], data[3], data[4], (uint8_t)MAPI_GET_MCU_GPIO_STATE_DBG);

	err = control_send_mcu(context, data, size);

	//in case there was an error with the MCU inform the driver
	//by returning -1 
	if(0 != err)
	{
		data[0] = 0;
		data[1] = 0;
		data[2] = -1;

		write(context->gpio_fd, &data[0], errsize);
	}
	 
	return err;
}

#define LED_DAT_LEN 5
static int control_leds(struct control_thread_context * context)
{
    int err, i;
    uint8_t leds_data[16];
    uint8_t msg[8];

    err = read(context->vled_fd, leds_data, sizeof(leds_data));

    if (-1 == err) {
        DERR("failure to read[/dev/vleds] - %s", strerror(errno));
        return -1;
    }

    if ((uint8_t)-1 == leds_data[15]) {
        // nothing chenged
        return 0;
    }

    if (leds_data[15] > 3 || leds_data[15] < 1) {
        DERR("invalid led [%d]", leds_data[15]);
        return -1;
    }

    msg[0] = control_get_seq(context);
    msg[1] = (uint8_t)COMM_WRITE_REQ;
    msg[2] = (uint8_t)MAPI_SET_LED_STATUS;

    if (leds_data[15] & 1) {
        i = 0; 

        memcpy(&msg[3], &leds_data[i], LED_DAT_LEN);
        DINFO("set led[%d:%d] req[%d:%d:%d:%d]", msg[3], i, msg[4], msg[5], msg[6], msg[7]);
    #if defined (IO_CONTROL_RECOVERY_DEBUG)
        printf("%s: set led[%d:%d] req[%d:%d:%d:%d]\n", __func__, msg[3], i, msg[4], msg[5], msg[6], msg[7]);
    #endif
        err = control_send_mcu(context, msg, sizeof(msg));
        if (-1 == err) {
            DERR("failure to send command - %s", strerror(errno));
            return -1;
        }
    }

    if (leds_data[15] & 2) {
        i = LED_DAT_LEN; 

        memcpy(&msg[3], &leds_data[i], LED_DAT_LEN);
        DINFO("set led[%d:%d] req[%d:%d:%d:%d]", msg[3], i, msg[4], msg[5], msg[6], msg[7]);
    #if defined (IO_CONTROL_RECOVERY_DEBUG)
        printf("%s: set led[%d:%d] req[%d:%d:%d:%d]\n", __func__, msg[3], i, msg[4], msg[5], msg[6], msg[7]);
    #endif
        err = control_send_mcu(context, msg, sizeof(msg));
        if (-1 == err) {
            DERR("failure to send command - %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/* converts RTC bcd array format to string */
void rtc_bcdconvert_and_set_systime(uint8_t * dt_bcd, char * dt_str, bool print_time)
{
	uint8_t hundreth_sec_int = (dt_bcd[0]>>4) + (dt_bcd[0]&0x0F);
	uint8_t seconds = (((dt_bcd[1]>>4)&0x7) * 10) + (dt_bcd[1]&0x0F);
	uint8_t minutes = (((dt_bcd[2]>>4)&0x7) * 10) + (dt_bcd[2]&0x0F);
	uint8_t hours = (((dt_bcd[3]>>4)&0x3) * 10) + (dt_bcd[3]&0x0F);
	uint8_t century = (dt_bcd[3]>>6);
	//uint8_t day_of_week = dt[4]&0x7;
	uint8_t day_of_month = (((dt_bcd[5]>>4)&0x3) * 10) + (dt_bcd[5]&0x0F);
	uint8_t month = (((dt_bcd[6]>>4)&0x1) * 10) + (dt_bcd[6]&0x0F);
	uint16_t year = ((dt_bcd[7]>>4) * 10) + (dt_bcd[7]&0x0F);
	int ret;

	year = 2000 + (century * 100) + year;

	time_t my_time = time(0);
	struct tm* tm_ptr = gmtime(&my_time);
	tm_ptr->tm_year = year - 1900;
	tm_ptr->tm_mon = month-1;
	tm_ptr->tm_mday = day_of_month;

	tm_ptr->tm_hour = hours;
	tm_ptr->tm_min = minutes;
	tm_ptr->tm_sec = seconds;

	ret = setenv("TZ","UTC",1);

	if(ret<0)
	{
	  DERR("setenv() returned an error %d.\n", ret);
	}

	const struct timeval tv = {mktime(tm_ptr), hundreth_sec_int*1000};
	ret = settimeofday(&tv,0);
	if (ret < 0)
	{
		DERR("settimeofday returned %d errno: %s", ret, strerror(errno));
	}

	snprintf(dt_str, 23 , "%04d-%02d-%02d %02d:%02d:%02d.%02d ",
			year, month, day_of_month, hours, minutes, seconds, hundreth_sec_int);
	if (print_time)
	{
		DINFO("init rtc date_time: %04d-%02d-%02d %02d:%02d:%02d.%02d\n",
				year, month, day_of_month, hours, minutes, seconds, hundreth_sec_int);
	}
}

void update_system_time_with_rtc(struct control_thread_context * context)
{
	uint8_t req[] = {MAPI_READ_RQ, MAPI_GET_RTC_DATE_TIME };
	char dt_str[23];
	int ret = -1;
	//struct sock_addr_un addr = RTC_SOCK_ADDR;

	context->rtc_req = true;
	control_handle_api_command(context, NULL, req, sizeof(req));
	DINFO("update_system_time_with_rtc: about to request RTC time\n");

	while (context->rtc_req == true)
	{
		ret = control_receive_mcu(context);
		if(ret < 0)
		{
			DERR("update_system_time_with_rtc, control_receive_mcu returned %d", ret);
			context->rtc_req = false;
			return;
		}
		DTRACE("update_system_time_with_rtc, After recv");
	}

	rtc_bcdconvert_and_set_systime(context->rtc_init_val, dt_str, true);
	context->rtc_req = false;
}

// Returns major number of version
//
int32_t set_fw_vers_files(struct control_thread_context * context)
{
	uint8_t req[2];
	char ver[16] = {0};
	int32_t ret = -1, ver_maj_n = 0;
	int32_t fdw = -1;
	uint32_t cnt;
	char* mcu_file = "/proc/mcu_version";
	char* fpga_file = "/proc/fpga_version";
	char* fn;
    const char* prop_name = 0;


	for (cnt = 0; cnt < 2; ++cnt) {
	    req[0] = MAPI_READ_RQ;
	    if (0 == cnt) {
	    	fn = mcu_file;
	    	req[1] = MAPI_GET_MCU_FW_VERSION;
            prop_name = mcu_ver_prop;
	    } else {
	    	fn = fpga_file;
	    	req[1] = MAPI_GET_FPGA_VERSION;
            prop_name = fpga_ver_prop;
	    }

	    fdw = open(fn, O_WRONLY, 0666);
    	if (0 > fdw) {
    		DERR("set_fw_vers_files cannot open /proc/mcu_version\n");
    		continue;
    	}

		control_handle_api_command(context, NULL, req, sizeof(req));

		context->dont_send = true;
		strcpy(ver, prop_unknown);

		while (context->dont_send) {
			ret = control_receive_mcu(context);
			if (ret < 0) {
				DERR("set_fw_vers_files failed - %d\n", ret);
				context->dont_send = false;

				write(fdw, ver, strlen(ver));
				close(fdw);
				property_set(prop_name, ver);
                if (ret == EBADF) {
                    context->running = false;
                }
			//	return;
			}
		}

        if (ret >= 0) {
			if (0 == cnt) {
				sprintf(ver, "%X.%d.%d.%d", (uint8_t)context->frame.data[3], (uint8_t)context->frame.data[4], (uint8_t)context->frame.data[5], (uint8_t)context->frame.data[6]);
                ver_maj_n = context->frame.data[4];
            } else {
                sprintf(ver, "%X", *((uint32_t*)&context->frame.data[3]));
            }

			write(fdw, ver, strlen(ver));
			close(fdw);
            property_set(prop_name, ver);
        }
	}

    return ver_maj_n;
}

#if 0
int32_t get_in_volts(struct control_thread_context * context, int32_t ver_maj_n)
{
	uint8_t req[2];
	char ver[16] = {0};
	int32_t ret = -1;
	int32_t fdw = -1;
	uint32_t cnt;
	char* mcu_file = "/proc/mcu_version";
	char* fn;
    const char* prop_name = 0;


	for (cnt = 0; cnt < 2; ++cnt) {
	    req[0] = MAPI_READ_RQ;
	    if (0 == cnt) {
	    	fn = mcu_file;
	    	req[1] = MAPI_GET_MCU_FW_VERSION;
            prop_name = mcu_ver_prop;
	    } else {
	    	fn = fpga_file;
	    	req[1] = MAPI_GET_FPGA_VERSION;
            prop_name = fpga_ver_prop;
	    }

	    fdw = open(fn, O_WRONLY, 0666);
    	if (0 > fdw) {
    		DERR("set_fw_vers_files cannot open /proc/mcu_version\n");
    		continue;
    	}

		control_handle_api_command(context, NULL, req, sizeof(req));

		context->dont_send = true;
		strcpy(ver, prop_unknown);

		while (context->dont_send) {
			ret = control_receive_mcu(context);
			if (ret < 0) {
				DERR("set_fw_vers_files failed - %d\n", ret);
				context->dont_send = false;

				write(fdw, ver, strlen(ver));
				close(fdw);
				property_set(prop_name, ver);
                if (ret == EBADF) {
                    context->running = false;
                }
			//	return;
			}
		}

        if (ret >= 0) {
			if (0 == cnt) {
				sprintf(ver, "%X.%d.%d.%d", (uint8_t)context->frame.data[3], (uint8_t)context->frame.data[4], (uint8_t)context->frame.data[5], (uint8_t)context->frame.data[6]);
                ver_maj_n = context->frame.data[4];
            } else {
                sprintf(ver, "%X", *((uint32_t*)&context->frame.data[3]));
            }

			write(fdw, ver, strlen(ver));
			close(fdw);
            property_set(prop_name, ver);
        }
	}

    return ver_maj_n;
}
#endif

/* Request for all the GPInput values, in case they were missed on bootup */
int update_all_GP_inputs(struct control_thread_context * context)
{
	uint8_t req[] = { 0, COMM_WRITE_REQ, MAPI_SET_GPI_UPDATE_ALL_VALUES };
	return control_frame_process(context, req, sizeof(req));
}

/* Send a request to the MCU to issue a watchdog reset */
int send_app_watchdog(struct control_thread_context * context)
{
	uint8_t req[] = { 0, COMM_WRITE_REQ, MAPI_SET_APP_WATCHDOG_REQ };
	return control_frame_process(context, req, sizeof(req));
}

static int control_receive_sock(struct control_thread_context * context)
{
	struct sockaddr_un c_addr = {0};
	uint8_t buf[SOCK_MAX_MSG] = {0};
	int r = -1;

	socklen_t sock_len;
	ssize_t num_bytes;

	sock_len = sizeof(struct sockaddr_un);
	num_bytes = recvfrom(context->sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&c_addr, &sock_len);
	if(-1 == num_bytes)
	{
		DERR("recvfrom: %s", strerror(errno));
		exit(-1);
	}


	DTRACE("server recv %ld bytes from '%s'", (long)num_bytes,
			(c_addr.sun_path[0] ? 0 : 1) + c_addr.sun_path); // if first byte is null add one to address (sorry bad style here) assumes last byte in sub_path is null as it should be

	// TODO: remove for production.
	log_hex(buf, num_bytes);
	if(num_bytes < 1)
	{
		DERR("Empty message");
		return -1;
	}

	switch(buf[0])
	{
		// 0 String command
		case MCTRL_MSTR:
			r = control_handle_sock_command(context, &c_addr, buf+1, num_bytes-1);
			break;

		// 1 RAW command
		case MCTRL_MRAW:
			r = control_handle_sock_raw(context, &c_addr, buf+1, num_bytes-1);
			break;

		// 2 API Command
		case MCTRL_MAPI:
			r = control_handle_api_command(context, &c_addr, buf+1, num_bytes-1);
			break;

		default:
			DERR("Error, unknown command type %d", buf[0]);
			log_hex(buf, num_bytes);

			r = send_sock_string_message(context, &c_addr, "ERROR");
	}

	return r;
}


static void check_devices(struct control_thread_context * context)
{
	if( -1 == context->mcu_fd) {
		DTRACE("check for device '%s'", context->name);

		// TODO: This should search for the correct device name by
		// using /sys/bus/usb to find the current device name incase 
		// other devices enumerate, or the order changes, etc.
		if(file_exists(context->name)) {
			if(1)/*!file_exists("/data/disable_control_tty"))*/ {
				context->mcu_fd = open_serial(context->name);
				DINFO("opened %s fd = %d", context->name, context->mcu_fd);
			}
		} else {
			DTRACE("%s does not exist", context->name);
		}
	} else {
        char tty_s[32];
        int rc;
        struct stat tty_i;

        rc = readlink(context->name, tty_s, sizeof(tty_s));
        tty_s[sizeof(tty_s) - sizeof(tty_s[0])] = 0;
        if (rc > 0) {
            rc = stat(tty_s, &tty_i);
            if (0 != rc) {
                DINFO("%s: %s ponts to not existing %s\n", __func__, context->name, tty_s);
            } else {
                return;
            }
        }
        DINFO("%s: restart iodriver\n", __func__);
        close(context->mcu_fd);
        context->mcu_fd = -1;
        context->running = 0;
    }

	// TODO: add vgpio, and sockets here if needed
}

/* get_app_watchdog_expire_time(): checks device for micronet_app_watchdog_time.cfg and
 * if present, reads the app watchdog time and returns the value (in seconds).
 * if not present returns zero as the app watchdog time */
static int get_app_watchdog_expire_time(void)
{
	char app_wdg_file_name[] = "/sdcard/micronet_app_watchdog_time.cfg";
	int fd;
	ssize_t bytes_read; // NOTE signed type
	char readbuffer[8];
	int app_watchdog_max_time = 0;

	fd = open(app_wdg_file_name, O_RDONLY, O_NDELAY);
	if(0 > fd)
	{
		DINFO("Cannot open /sdcard/micronet_app_watchdog_time.cfg\n");
		app_watchdog_max_time = 0;
		close(fd);
		return app_watchdog_max_time;
	}
	bytes_read = read(fd, readbuffer, sizeof(readbuffer));
	if(bytes_read > 0)
	{
		app_watchdog_max_time = atoi(readbuffer);
	}
	else
	{
		DINFO("read: %s", strerror(errno));
		app_watchdog_max_time = 0;
	}

	/* make sure it's valid */
	if (app_watchdog_max_time < 60 || app_watchdog_max_time > 5000)
	{
		app_watchdog_max_time = 0;
	}
	close(fd);
	DINFO("app_watchdog_max_time = %d", app_watchdog_max_time);
	return app_watchdog_max_time;
}

/* set_app_watchdog_expire_time(): replaces the value of app_watchdog_max_time with
 * the value provided in max_time in micronet_app_watchdog_time.cfg.
 * Returns true on success  */
static bool set_app_watchdog_expire_time(int max_time)
{
	char app_wdg_file_name[] = "/sdcard/micronet_app_watchdog_time.cfg";
	int fd, bytes_to_write;
	ssize_t bytes_written; // NOTE signed type
	char writebuf[8] = {0};
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	fd = open(app_wdg_file_name, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if(0 > fd)
	{
		DERR("Cannot open /sdcard/micronet_app_watchdog_time.cfg\n");
		close(fd);
		return false;
	}
	bytes_to_write = sprintf(writebuf, "%d", max_time);
	bytes_written = write(fd, writebuf, bytes_to_write);
	if((int)(bytes_written) == bytes_to_write)
	{
		DINFO("wrote %s to micronet_app_watchdog_time.cfg\n", writebuf);
	}
	else
	{
		DINFO("error writing: %s", strerror(errno));
		close(fd);
		return false;
	}

	close(fd);
	return true;
}

static bool get_app_watchdog_count(int * count)
{
	char app_wdg_file_name[] = "/sdcard/micronet_app_watchdog_count.cfg";
	int fd;
	ssize_t bytes_read; // NOTE signed type
	char readbuffer[8];

	fd = open(app_wdg_file_name, O_RDONLY, O_NDELAY);
	if(0 > fd)
	{
		DINFO("Cannot open /sdcard/micronet_app_watchdog_time.cfg\n");
		close(fd);
		*count = 0; /* If app_watchdog doesn't exist, it means no watchdog has happened */
		return true;
	}
	bytes_read = read(fd, readbuffer, sizeof(readbuffer));
	if(bytes_read > 0)
	{
		*count = atoi(readbuffer);
	}
	else
	{
		DINFO("read: %s", strerror(errno));
		close(fd);
		return false;
	}

	close(fd);
	DINFO("app watchdog count = %d", *count);
	return true;
}

/* set app watchdog count to the value provided in count */
static bool set_app_watchdog_count(int count)
{
	char app_wdg_file_name[] = "/sdcard/micronet_app_watchdog_count.cfg";
	int fd, bytes_to_write;
	ssize_t bytes_written; // NOTE signed type
	char buf[8] = {0};

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	fd = open(app_wdg_file_name, O_RDWR | O_CREAT | O_TRUNC, mode);
	if(0 > fd)
	{
		DERR("Cannot open /sdcard/micronet_app_watchdog_count.cfg\n");
		close(fd);
		return false;
	}

	bytes_to_write = sprintf(buf, "%d", count);
	bytes_written = write(fd, buf, bytes_to_write);
	if((int)(bytes_written) == bytes_to_write)
	{
		DINFO("wrote %s to micronet_app_watchdog_count.cfg\n", buf);
	}
	else
	{
		DINFO("error writing: %s", strerror(errno));
		close(fd);
		return false;
	}

	close(fd);
	return true;
}

bool device_has_rtc(){
	bool ret = false;
    char model_str[PROPERTY_VALUE_MAX] = "0";
	
	property_get("ro.product.model", model_str, "0");	
	if (strcmp((const char *)model_str, "TREQr_5")==0){
     	ret = true;
	}
	DINFO("%s: ro.product.model=%s, ret=%d", __func__, model_str, ret);

	return ret;
}

void * control_proc(void * cntx)
{
	struct control_thread_context * context = cntx;
	int status;
	uint8_t databuffer[1024]; // >= 10 * (8*2)
	struct timespec time_last_sent_ping;
	clock_gettime(CLOCK_MONOTONIC_RAW, &time_last_sent_ping);
	struct timespec curr_time;
	time_t time_diff = 0;
	bool on_init = true;
	context->max_app_watchdog_ping_time = get_app_watchdog_expire_time();
	int app_watchdog_count = 0;
    int dev_type = 0;

#if defined (IO_CONTROL_RECOVERY_DEBUG)
    redirect_stdio(IO_CONTROL_LOG);
#endif
	frame_setbuffer(&context->frame, databuffer, sizeof(databuffer));

	context->running = true;

	context->gpio_fd = -1;
	context->mcu_fd = -1;
	context->sock_fd = -1;
    context->vled_fd = -1;
    context->one_wire_fd = -1;

    clock_gettime(CLOCK_MONOTONIC_RAW, &(context->last_app_ping_time));
    context->dont_send = false;

	// TODO: maby move to check_devies()
	if(file_exists("/dev/vgpio"))
		context->gpio_fd = open("/dev/vgpio", O_RDWR, O_NDELAY);
    else
        DINFO("%s /dev/vgpio does not exist", __func__);

    if(file_exists("/dev/vleds"))
        context->vled_fd = open("/dev/vleds", O_RDONLY, O_NDELAY);
    else
    	DINFO("%s /dev/vleds does not exist", __func__);

    if(file_exists("/dev/one_wire"))
    	context->one_wire_fd = open("/dev/one_wire", O_WRONLY, O_NDELAY);
    else
    	DINFO("%s /dev/one_wire does not exist", __func__);

    // TODO: maby move to check_devies()
	context->sock_fd = control_open_socket(context);

	do
	{
		//DTRACE("Main loop");

		// Check for devices that need to be opened/reopened
		check_devices(context);
        if (!context->running) {
            break;
        }
		/* Only done once and does not depend on data being received */
		if (on_init && (context->mcu_fd > -1 ) && !FD_ISSET(context->mcu_fd, &context->fds)) {
			on_init = false;
			if (device_has_rtc()) {
				update_system_time_with_rtc(context);
			}
			clock_gettime(CLOCK_MONOTONIC_RAW, &time_last_sent_ping);
			clock_gettime(CLOCK_MONOTONIC_RAW, &(context->last_app_ping_time));
			update_all_GP_inputs(context);
			dev_type = set_fw_vers_files(context);
            DINFO("slave board %d\n", dev_type);
		}
#if 1
        if (dev_type < 7) {
            if ((context->mcu_fd > -1) && !FD_ISSET(context->mcu_fd, &context->fds)) {
                /* MCU to periodic A8 pings */
                clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
                time_diff = curr_time.tv_sec - time_last_sent_ping.tv_sec;
                if ((time_diff) > TIME_BETWEEN_MCU_PINGS) {
                    if (context->ping_sent != context->pong_recv) {
                        DERR("ping sent %d, ping rx %d", context->ping_sent, context->pong_recv);
                        context->running = false;
                        break;
                    }
                    uint8_t msg[2];

                    msg[0] = control_get_seq(context);
                    msg[1] = (uint8_t)PING_REQ;
                    control_send_mcu(context, msg, sizeof(msg));
                    clock_gettime(CLOCK_MONOTONIC_RAW, &time_last_sent_ping);
                    context->ping_sent++;

                    /* Check if we are still getting app pings */
                    clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
                    time_diff = curr_time.tv_sec  - context->last_app_ping_time.tv_sec;
                    DTRACE("Time since App ping: %d sec, maxtime: %d sec\n",(int)time_diff, context->max_app_watchdog_ping_time);
                    if ((context->max_app_watchdog_ping_time != 0) && (time_diff > context->max_app_watchdog_ping_time)) {
                        get_app_watchdog_count(&app_watchdog_count);
                        set_app_watchdog_count(++app_watchdog_count);
                        DERR("APP ping time expired, causing a watchdog reset, app_watchdog_count %d!!\n", app_watchdog_count);
                        send_app_watchdog(context);
                        /* wait for the watchdog to occur */
                        sleep(60);
                        context->running = false;
                    }
                }
            }
        }

#endif

        // TODO: Waiting for events
		status = control_thread_wait(context);

		if ((context->mcu_fd > -1) && FD_ISSET(context->mcu_fd, &context->fds)) {
			status = control_receive_mcu(context);
			if (status < 0) {
				DERR("control_receive_mcu returned %d", status);
				context->running = false;
				break;
			}
			DTRACE("After mcu recv");
		}

		if ((context->gpio_fd > -1) && FD_ISSET(context->gpio_fd, &context->fds)) {
			status = control_receive_gpio(context);
			if (status < 0) {
				DERR("control_receive_gpio returned %d\n", status);
			}
			DTRACE("After gpio receive");
		}

        if ((context->vled_fd > -1) && FD_ISSET(context->vled_fd, &context->fds)) {
            status = control_leds(context);
            if (status < 0) {
                DERR("failure to set led %d\n", status);
            }
            DTRACE("After vled receive");
        }

		if ((context->sock_fd > -1) && FD_ISSET(context->sock_fd, &context->fds)) {
			status = control_receive_sock(context);
			if (status < 0) {
				DERR("control_receive_sock returned %d\n", status);
			}
			DTRACE("After sock receive");
		}
	} while(context->running);
    if (context->mcu_fd > -1)
        close(context->mcu_fd);
    set_fw_vers_files(context);

#if defined (IO_CONTROL_RECOVERY_DEBUG)
    redirect_stdio("/dev/tty");
#endif

    close(context->sock_fd);
	close(context->gpio_fd);
	close(context->vled_fd);
	close(context->one_wire_fd);
	DERR("control thread exiting");
	return NULL;
}
