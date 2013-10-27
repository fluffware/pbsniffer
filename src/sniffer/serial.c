#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <asm/termios.h>
#include <asm/ioctls.h>
#include <string.h>
#include "serial.h"

#ifndef BOTHER
#define    BOTHER CBAUDEX
#endif
extern int ioctl(int d, int request, ...);


int 
profibus_serial_open(const char *device, unsigned int rate, GError **err)
{
  struct termios2 settings;
  int fd = open(device, O_RDONLY);
  if (fd < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"open failed: %s", strerror(errno));
    return -1;
  }
  if (ioctl(fd, TCGETS2, &settings) < 0) {
      g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		  "ioctl TCGETS2 failed: %s", strerror(errno));
    close(fd);
    return -1;
  }
  settings.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | INPCK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON | PARMRK);
  settings.c_iflag |= IGNBRK | IGNPAR;
  settings.c_oflag &= ~OPOST;
  settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  settings.c_cflag &= ~(CSIZE | PARODD | CBAUD);
  settings.c_cflag |= CS8 | BOTHER | CREAD | PARENB;
  settings.c_ispeed = rate;
  settings.c_ospeed = rate;
  if (ioctl(fd, TCSETS2, &settings) < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"ioctl TCSETS2 failed: %s", strerror(errno));
    close(fd);
    return -1;
  }
  
  return fd;
}
