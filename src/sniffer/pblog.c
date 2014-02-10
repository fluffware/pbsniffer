#include <serial.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <pbframer.h>
#include <scheduled_time.h>
#include <pcapng.h>
#include <pcap.h>
#include <string.h>

typedef struct _LogFile LogFile;

struct _LogFile
{
  gint64 start;
  gint64 end;
  GFile *file;
  GOutputStream *stream;
};
  
typedef struct AppContext AppContext;
struct AppContext
{
  GInputStream *capture_stream;
  gchar *device;
  guint speed;
  PBFramer *framer;
  GAsyncQueue *captured_queue;
  gchar *file_prefix;

  gchar *swap_schedule_str;
  ScheduledTime swap_schedule;
  GThread *output_thread;
  GCancellable *cancel;
  GMutex log_file_lock;
  GCond log_file_change;
  LogFile log_files[3];
  
  /* Indexes log_files. Modulo 3. Only changed by the writing thread. */
  guint write_file; /* Only advances if next file is valid */
  guint next_file; /* Always equal to or one ahead of write_file */  
};

static void
log_file_init(LogFile *log)
{
  log->file = NULL;
  log->stream = NULL;
}

static void
log_file_clear(LogFile *log)
{
  g_clear_object(&log->file);
  g_clear_object(&log->stream);
}


static void
app_init(AppContext *app)
{
  gint i;
  app->capture_stream = NULL;
  app->device = "/dev/ttyS0";
  app->speed = 500000;
  app->framer = NULL;
  app->captured_queue = NULL;
  app->file_prefix = "/tmp/pblog";
  app->swap_schedule_str = "0 1 * * *"; /* Swap every hour */

  scheduled_time_init(&app->swap_schedule);
  for (i = 0; i < 3; i++) {
    log_file_init(&app->log_files[i]);
  }
  app->write_file = 0;
  app->next_file = 1;
  app->output_thread = NULL;
  app->cancel = g_cancellable_new();
  g_mutex_init(&app->log_file_lock);
  g_cond_init(&app->log_file_change);
}

static void
app_cleanup(AppContext* app)
{
  gint i;
  g_clear_object(&app->capture_stream);
  g_clear_object(&app->framer);

  g_cancellable_cancel(app->cancel);
  g_cond_signal(&app->log_file_change);

  if (app->output_thread) {
    g_thread_join(app->output_thread);
  }

  g_clear_object(&app->cancel);

  if (app->captured_queue) {
    g_async_queue_unref(app->captured_queue);
    app->captured_queue = NULL;
  }
  
  for (i = 0; i < 3; i++) {
    log_file_clear(&app->log_files[i]);
  }
  
  g_mutex_clear(&app->log_file_lock);
  g_cond_clear(&app->log_file_change);
}

#define FILE_SUFFIX ".pcapng"

/* Opens and closes log files */
static gpointer
create_output_thread(gpointer data)
{
  GError *err = NULL;
 
  AppContext *app = data;
  guint next_file = 0;
  guint write_file = 0;
  gboolean write_changed;
  gboolean next_changed;
  gint64 next_swap = g_get_real_time();
  guint64 if_speed;
  /* Wait for a change */
  while(TRUE) {
    g_mutex_lock(&app->log_file_lock);
    while(app->next_file == next_file && app->write_file == write_file
	  && !g_cancellable_is_cancelled(app->cancel)) {
      g_cond_wait(&app->log_file_change, &app->log_file_lock);
    }
    next_changed = app->next_file != next_file;
    write_changed = app->write_file != write_file;
    g_mutex_unlock(&app->log_file_lock);
    if (g_cancellable_is_cancelled(app->cancel)) return NULL;
  
    if (next_changed) {
      ByteChain *options = NULL;
      LogFile *log_file;
      gchar start_buffer[20];
      gchar *file_name;
      GFile *file = NULL;
      GFileOutputStream *stream;
      next_file = (next_file + 1) % 3;
      log_file = &app->log_files[next_file];
      g_assert(log_file->file == NULL);
      log_file->start = next_swap;
      next_swap = scheduled_time_next_match(&app->swap_schedule,
					    next_swap + G_TIME_SPAN_MINUTE);
      log_file->end = next_swap;
      g_snprintf(start_buffer, sizeof(start_buffer), "%lld", log_file->start);
      file_name = g_strconcat(app->file_prefix, "_", start_buffer, FILE_SUFFIX,
			      NULL);
      file = g_file_new_for_path (file_name);
      stream = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE,
			      app->cancel, &err);
      if (!stream) {
	g_critical("Failed to open log file '%s': %s",file_name, err->message);
	g_object_unref(file);
	g_free(file_name);
	g_clear_error(&err);
	return NULL;
      }
      g_free(file_name);
      log_file->stream = g_buffered_output_stream_new(G_OUTPUT_STREAM(stream));
      g_filter_output_stream_set_close_base_stream(G_FILTER_OUTPUT_STREAM(log_file->stream), TRUE);
      g_object_unref(stream);
      pcapng_add_string_option(&options, PCAPNG_OPTION_SHB_USERAPPL, "pblog"); 
      if (!pcapng_write_section_header(log_file->stream, -1, options, &err)) {
	g_critical("Failed to write section header: %s", err->message);
	g_object_unref(file);
	g_clear_object(&log_file->stream);
	g_clear_error(&err);
	return NULL;
      }
      pcapng_clear_options(&options);
      pcapng_add_string_option(&options, PCAPNG_OPTION_IF_NAME, app->device);
      if_speed = app->speed;
      pcapng_add_option(&options, PCAPNG_OPTION_IF_SPEED, 8,
			(guint8*)&if_speed); 
      if (!pcapng_write_interface_description(log_file->stream, DLT_USER0,
					      256, options, &err)) {
	g_critical("Failed to write interface description: %s\n", err->message);
	g_object_unref(file);
	g_object_unref(stream);
	g_clear_error(&err);
	return NULL;
      }
      pcapng_clear_options(&options);
      g_output_stream_flush(log_file->stream, NULL, NULL);
      g_mutex_lock(&app->log_file_lock);
      log_file->file = file;
      g_mutex_unlock(&app->log_file_lock);
      g_cond_signal(&app->log_file_change);
    }
    if (write_changed) {
      LogFile *log_file;
      log_file = &app->log_files[write_file];
      if (log_file->file) { /* No file to close when starting */
	gchar *prefix;
	gchar *prefix_end;
	gchar time_buffer[40];
	gchar *file_name;
	GFile *renamed;
	if (!g_output_stream_close(log_file->stream, NULL, &err)) {
	  g_warning("Closing file failed: %s", err->message);
	  g_clear_error(&err);
	}
	g_clear_object(&log_file->stream);
	prefix = g_file_get_basename(log_file->file);
	prefix_end = rindex(prefix, '_');
	g_assert(prefix_end);
	*prefix_end = '\0';
	g_snprintf(time_buffer, sizeof(time_buffer), "%lld-%lld",
		   log_file->start, log_file->end);
	file_name = g_strconcat(prefix, "_", time_buffer, FILE_SUFFIX,
				NULL);
	g_free(prefix);
	renamed = g_file_set_display_name(log_file->file, file_name, NULL, &err);
	g_free(file_name);
	if (!renamed) {
	  g_warning("Renaming file failed: %s", err->message);
	  g_clear_error(&err);
	}
	g_object_unref(renamed);
	g_clear_object(&log_file->file);
      }
      write_file = (write_file + 1) % 3;
    }
    g_debug("Thread: %d %d Write %d %d", write_file, next_file, app->write_file, app->next_file);
  }
  return NULL;
}

static LogFile *
swap_file(AppContext *app)
{
  LogFile *log_file = &app->log_files[app->write_file];
  LogFile *next_log_file;
  g_mutex_lock(&app->log_file_lock);
  if (app->next_file == app->write_file) {
    app->next_file = (app->next_file + 1) % 3;
    g_cond_signal(&app->log_file_change);
  }
  next_log_file = &app->log_files[app->next_file];
  while(next_log_file->file == NULL) {
    g_cond_wait(&app->log_file_change, &app->log_file_lock);
  }
  g_debug("Swapping %d -> %d", app->write_file, app->next_file);
  app->write_file = app->next_file;
  app->next_file = (app->next_file + 1) % 3;
  g_cond_signal(&app->log_file_change);
  log_file = &app->log_files[app->write_file];
  g_mutex_unlock(&app->log_file_lock);
  return log_file;
}

static LogFile *
get_log_file(AppContext *app)
{
  LogFile *log_file = &app->log_files[app->write_file];
  if (log_file->file == NULL) {
    log_file = swap_file(app);
  }
  return log_file;
}

static void
packets_received(PBFramer *framer, GAsyncQueue *queue, AppContext *app)
{
  GError *err = NULL;
  PBFramerPacket *packet;
  LogFile *log_file = get_log_file(app);
  while ((packet = g_async_queue_try_pop(queue)) != NULL) {
    if (log_file->end <= packet->timestamp) {
      log_file = swap_file(app);
    }
    if (!pcapng_write_enhanced_packet(log_file->stream, 0, packet->timestamp,
				      packet->length, packet->length,
				      packet->data, NULL, &err)){
      g_warning("Failed to write packet to log: %s", err->message);
      g_clear_error(&err);
    }
#if 0
    {
      guint i;
      GString *str = g_string_new("");
      for (i = 0; i < packet->length; i++) {
	g_string_append_printf(str, " %02x", packet->data[i]);
      }
      g_debug("Received %lld: %s", packet->timestamp, str->str);
      g_string_free(str, TRUE);
    }
#endif
    pb_framer_packet_free(packet);
  }
}

static gboolean
sigint_handler(gpointer user_data)
{
  g_main_loop_quit(user_data);
  return TRUE;
}

AppContext app;

const GOptionEntry app_options[] = {
  {"device", 'd', 0, G_OPTION_ARG_STRING,
   &app.device, "Serial device", "DEV"},
  {"speed", 's', 0, G_OPTION_ARG_INT,
   &app.speed, "Serial speed (bps)", "SPEED"},
  {"file-prefix",'f', 0, G_OPTION_ARG_STRING,
   &app.file_prefix, "Capture file prefix", "PREFIX"},
  {"swap-file-schedule", '\0', 0, G_OPTION_ARG_STRING,
   &app.swap_schedule_str,
   "Crontab style scheduling for swaping to a new log file.", "PATTERN"},
  {NULL}
};

int
main(int argc, char *argv[])
{
  GMainLoop *loop;
  int ser_fd;
  GError *err = NULL;
  GOptionContext *opt_ctxt;
#if MEM_DEBUG
  g_mem_set_vtable(glib_mem_profiler_table);
#endif  
  g_type_init();
  app_init(&app);
  opt_ctxt = g_option_context_new (" - log Profibus traffic");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  g_option_context_free(opt_ctxt);

  if (!scheduled_time_parse(&app.swap_schedule, app.swap_schedule_str, &err)) {
    g_printerr("Failed to parse swap schedule: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  ser_fd = profibus_serial_open(app.device, app.speed, &err);
  if (ser_fd < 0) {
    g_printerr("Failed open serial port: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  app.output_thread = g_thread_new("Log file", create_output_thread, &app);
  app.capture_stream = g_unix_input_stream_new(ser_fd, TRUE);
  app.captured_queue =
    g_async_queue_new_full((GDestroyNotify)pb_framer_packet_free);
  app.framer = pb_framer_new(app.capture_stream, app.captured_queue);
  g_signal_connect(app.framer, "packets-queued",
		   (GCallback)packets_received, &app);
  loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add(SIGINT, sigint_handler, loop);
  g_unix_signal_add(SIGTERM, sigint_handler, loop);
  g_debug("Starting");
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
  g_message("Exiting"); 
  app_cleanup(&app);
#if MEM_DEBUG
  g_mem_profile();
#endif
  return EXIT_SUCCESS;
}
