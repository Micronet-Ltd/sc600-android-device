// FIXME: This should be the POSIX compatability flag, and the function that depends on this should be commented
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

// TODO: handle restore, and multiple devices...
struct termios saved_termios = {0};
int have_saved_termios = 0;

void setup_tty(int fd)
{
	struct termios tio = {0};

	if(tcgetattr(fd, &saved_termios))
	{
		DERR("tcgetattr: %s", strerror(errno));
	}

	have_saved_termios = 1;

	tio = saved_termios;

	tio.c_iflag |= IGNBRK | IGNPAR;
	tio.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	tio.c_oflag = 0;
	//tio.c_oflag &= ~(OPOST);
	tio.c_lflag = 0;
	tio.c_cc[VERASE] = 0;
	tio.c_cc[VKILL] = 0;
	//tio.c_cc[VMIN] = 0;
	//tio.c_cc[VTIME] = 0;
	//tio.c_cflag |= (CS8);
	//tio.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	if(tcsetattr(fd, TCSANOW, &tio))
	{
		DERR("tcsetattr: %s", strerror(errno));
	}
}

int open_serial(const char * name)
{
	int fd;
	struct termios tio = {0};

	if( 0 > (fd = open(name, O_RDWR | O_NOCTTY)))
	{
		DERR("open: %s", strerror(errno));
		close(fd);
		return -1;
	}

	//fcntl(fd, F_SETFL, O_NDELAY);

	ioctl(fd, TIOCSCTTY, (void*)1);
	fcntl(fd, F_SETFL, 0);
	tcgetattr(fd, &tio);
	// TODO: ACM does not have speed
	cfsetispeed(&tio, B57600);
	cfsetospeed(&tio, B57600);
	tio.c_cflag |= (CLOCAL | CREAD);
	tio.c_cflag &= ~PARENB;
	tio.c_cflag &= ~CSTOPB;
	tio.c_cflag &= ~CSIZE;
	tio.c_cflag |= CS8;

	tio.c_cflag &= ~CRTSCTS;
	tio.c_iflag &= ~(IXON | IXOFF | IXANY);

	tio.c_lflag &= ~(ECHO | ECHOE);
	tio.c_oflag &= ~OPOST;
	tcsetattr(fd, TCSANOW, &tio);

	setup_tty(fd);

	return fd;
}
