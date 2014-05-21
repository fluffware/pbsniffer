#include <stdlib.h>
#include <wiretap/wtap.h>
#include <stdio.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/termios.h>
#include <string.h>
#include <unistd.h>


extern int ioctl(int d, int request, ...);

gboolean
set_termios(int fd, unsigned int rate, GError **err)
{
  struct termios2 settings;
  if (ioctl(fd, TCGETS2, &settings) < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		  "ioctl TCGETS2 failed: %s", strerror(errno));
    return FALSE;
  }
  settings.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | INPCK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON |IXOFF | PARMRK);
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
    return FALSE;
  }
  return TRUE;
}
int 
profibus_serial_open(const char *device,  unsigned int rate, GError **err)
{
  int fd = open(device, O_RDWR);
  if (fd < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"open failed: %s", strerror(errno));
    return -1;
  }
  if (!set_termios(fd, rate, err)) {
    close(fd);
    return -1;
  }
  return fd;
}

int
open_pseudo_terminal(GError **err)
{
  int fd = posix_openpt(O_RDWR);
  if (fd < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"posix_openpt failed: %s", strerror(errno));
    return -1;
  }
  if (!set_termios(fd, 115200, err)) {
    close(fd);
    return -1;
  }
  if (unlockpt(fd) < 0) {
    g_set_error(err, G_FILE_ERROR, g_file_error_from_errno(errno),
		"unlockpt failed: %s", strerror(errno));
    return -1;
  }
  g_message("Use %s as tty", ptsname(fd));
  return fd;
}


static char *device = "/dev/ttyS0";
static gint speed = 500000;
static gint pseudo = FALSE;
static gint timing = FALSE;

const GOptionEntry app_options[] = {
  {"device", 'd', 0, G_OPTION_ARG_STRING,
   &device, "Serial device", "DEV"},
  {"speed", 's', 0, G_OPTION_ARG_INT,
   &speed, "Serial speed (bps)", "SPEED"},
  {"pseudo-terminal", 'p', 0, G_OPTION_ARG_NONE,
   &pseudo, "Open pseudo terminal", NULL},
  {"timing", 't', 0, G_OPTION_ARG_NONE,
   &timing, "Try to time packets the same way as the packet file",
   NULL},
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
  gint64 ts_start_us = 0;
  gint64 time_start_us = 0;
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

  if (pseudo) {
    ser_fd = open_pseudo_terminal(&err);
    if (ser_fd < 0) {
      g_printerr("Failed open pseudo terminal: %s\n", err->message);
      return EXIT_FAILURE;
    }
  } else {
    ser_fd = profibus_serial_open(device, speed, &err);
    if (ser_fd < 0) {
      g_printerr("Failed open serial port: %s\n", err->message);
      return EXIT_FAILURE;
    }
  }
  
  while(TRUE) {
    guint8 *data;
    struct wtap_pkthdr *hdr;
    if (!wtap_read(w, &werr, &err_info, &data_offset)) {
      if (err == 0) break;
      g_printerr("Failed to read packet file: %d\n", werr);
      return EXIT_FAILURE;
    }
    hdr = wtap_phdr(w);
    data = wtap_buf_ptr(w);
    if (timing) {
      if (time_start_us == 0) {
	ts_start_us = hdr->ts.secs * 1000000LL +  hdr->ts.nsecs / 1000;
	time_start_us = g_get_monotonic_time();
      } else {
	gint64 ts = hdr->ts.secs * 1000000LL +  hdr->ts.nsecs / 1000;
	gint64 next = time_start_us + (ts - ts_start_us);
	gint64 now = g_get_monotonic_time();
	gint64 delay = next - now;
	if (delay > 0) {
	  poll(NULL, 0, delay / 1000);
	}
      }
    }
    if (write(ser_fd, data, hdr->len) != hdr->len) {
      g_printerr("Failed to write to serial device: %s\n", strerror(errno));
      break;
    }
    /* g_debug("Write %d", hdr->len); */
  }
  wtap_close(w);
  g_message("EOF");
  close(ser_fd);
  return EXIT_SUCCESS;
}
