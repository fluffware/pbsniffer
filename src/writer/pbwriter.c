#include <stdlib.h>
#include <wiretap/wtap.h>
#include <stdio.h>

#include <fcntl.h>
#include <errno.h>
#include <asm/termios.h>
#include <asm/ioctls.h>
#include <string.h>
#include <unistd.h>


#ifndef BOTHER
#define    BOTHER CBAUDEX
#endif
extern int ioctl(int d, int request, ...);


int 
profibus_serial_open(const char *device, unsigned int rate, GError **err)
{
  struct termios2 settings;
  int fd = open(device, O_RDWR);
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

static char *device = "/dev/ttyS0";
static gint speed = 500000;
static gint pseudo = FALSE;

const GOptionEntry app_options[] = {
  {"device", 'd', 0, G_OPTION_ARG_STRING,
   &device, "Serial device", "DEV"},
  {"speed", 's', 0, G_OPTION_ARG_INT,
   &speed, "Serial speed (bps)", "SPEED"},
  {"pseudo-terminal", 'p', 0, G_OPTION_ARG_NONE,
   &pseudo, "Open pseudo terminal", NULL},
  {NULL}
};

int
main(int argc, char *argv[])
{
  int ser_fd;
  GError *err = NULL;
  int werr;
  gchar *err_info;
  gint64 data_offset;
  wtap *w;
  GOptionContext *opt_ctxt;
  opt_ctxt = g_option_context_new ("FILE");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    return EXIT_FAILURE;
  }
  g_option_context_free(opt_ctxt);
  
  w = wtap_open_offline(argv[1], &werr, &err_info, FALSE);
  if (!w) {
    g_printerr("Failed to open packet file: %d\n", werr);
    return EXIT_FAILURE;
  }

  ser_fd = profibus_serial_open(device, speed, &err);
  if (ser_fd < 0) {
    g_printerr("Failed open serial port: %s\n", err->message);
    return EXIT_FAILURE;
  }
  
  while(TRUE) {
    guint8 *data;
    struct wtap_pkthdr *hdr;
    if (!wtap_read(w, &werr, &err_info, &data_offset)) {
      g_printerr("Failed to read packet file: %d\n", werr);
      return EXIT_FAILURE;
    }
    hdr = wtap_phdr(w);
    data = wtap_buf_ptr(w);
    write(ser_fd, data, hdr->len);
  }
  wtap_close(w);
  return EXIT_SUCCESS;
}
