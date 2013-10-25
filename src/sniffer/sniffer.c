#include <serial.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gunixinputstream.h>

#define MAX_PACKET_LEN 256

typedef struct AppContext AppContext;
struct AppContext
{
  GInputStream *capture_stream;
  guint8 *read_buffer;
  gsize read_buffer_len;
  guint8 packet_buffer[MAX_PACKET_LEN];
  gsize packet_len;
  gsize telegram_len;
};

static void
app_init(AppContext *app)
{
  app->capture_stream = NULL;
  app->read_buffer_len = 256;
  app->read_buffer = g_new(guint8, app->read_buffer_len);
  app->packet_len = 0;
  app->telegram_len = 255;
}

static void
app_cleanup(AppContext* app)
{
  g_clear_object(&app->capture_stream);
  g_free(app->read_buffer);
  app->read_buffer_len = 0;
}

static gboolean
sigint_handler(gpointer user_data)
{
  g_main_loop_quit(user_data);
  return TRUE;
}
static void
start_read(GInputStream *stream, AppContext *app);

#define SD1 0x10
#define SD2 0x68
#define SD3 0xa2
#define SD4 0xdc
#define ED 0x16
#define SC 0xe5

static void
read_callback(GObject *source_object,
	      GAsyncResult *res,
	      gpointer user_data)
{
  GError *err = NULL;
  gssize i;
  gssize r;
  AppContext *app = (AppContext*)user_data;
  r = g_input_stream_read_finish(G_INPUT_STREAM(source_object), res, &err);
  if (r <= 0) { 
      g_warning("Failed to read data");
      return;
  }
  for (i = 0; i < r; i++) {
    fprintf(stdout, " %02x", app->read_buffer[i]);
    if (app->packet_len < MAX_PACKET_LEN) {
      app->packet_buffer[app->packet_len++] = app->read_buffer[i];
      if (app->packet_len == 1) {
	switch(app->packet_buffer[0]) {
	case SC:
	  fprintf(stdout, "Short confirmation");
	  app->packet_len = 0;
	  break;
	case SD1:
	case SD2:
	case SD3:
	case SD4:
	  g_debug("Packet start");
	  break;
	default:
	  app->packet_len = 0;
	  break;
	}
      } else if(app->packet_len == 3) {
	switch(app->packet_buffer[0]) {
	case SD4:
	  fprintf(stdout, "Token %d -> %d\n",
		  app->packet_buffer[2], app->packet_buffer[1]);
	  break;
	case SD2:
	  if (app->packet_buffer[1] == app->packet_buffer[2]) {
	    app->telegram_len = app->packet_buffer[1];
	  } else {
	    g_warning("Variable telegram length error");
	    app->packet_len = 0;
	  }
	}
      }
    } else {
      g_warning("Packet too long");
      app->packet_len = 0;
    }
  }

  start_read(G_INPUT_STREAM(source_object), (AppContext*)user_data);
  fflush(stdout);
}

static void
start_read(GInputStream *stream, AppContext *app)
{
  g_input_stream_read_async(stream,
			    app->read_buffer, sizeof(app->read_buffer_len),
			    G_PRIORITY_DEFAULT, NULL, read_callback, app);
}

int
main(int argc, const char *argv[])
{
  GMainLoop *loop;
  AppContext app;
  int ser_fd;
  GError *err = NULL;
  app_init(&app);
  g_type_init();
  ser_fd = profibus_serial_open(argv[1], atoi(argv[2]), &err);
  if (ser_fd < 0) {
    g_printerr("Failed open serial port: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  app.capture_stream = g_unix_input_stream_new(ser_fd, TRUE);
  start_read(app.capture_stream, &app);

  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add(SIGINT, sigint_handler, loop);
  g_unix_signal_add(SIGTERM, sigint_handler, loop);
  g_debug("Starting");
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  g_message("Exiting"); 
  app_cleanup(&app);
  return EXIT_SUCCESS;
}
