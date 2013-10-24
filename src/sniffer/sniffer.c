#include <serial.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

typedef struct AppContext AppContext;
struct AppContext
{
  int ser_fd;
};

static void
app_init(AppContext *app)
{
  app->ser_fd = -1;
}

static void
app_cleanup(AppContext* app)
{
  if (app->ser_fd >= 0) {
    close(app->ser_fd);
    app->ser_fd = -1;
  }
}

int
main(int argc, const char *argv[])
{
  AppContext app;
  GError *err = NULL;
  app_init(&app);
  app.ser_fd = profibus_serial_open(argv[1], atoi(argv[2]), &err);
  if (app.ser_fd < 0) {
    g_printerr("Failed open serial port: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  while(1) {
    guint8 buffer[100];
    size_t i;
    ssize_t r = read(app.ser_fd, buffer, sizeof(buffer));
    if (r <= 0) { 
      g_printerr("Failed to read data");
      break;
    }
    for (i = 0; i < r; i++) {
      fprintf(stdout, " %02x", buffer[i]);
    }
  }
  app_cleanup(&app);
  return EXIT_SUCCESS;
}
