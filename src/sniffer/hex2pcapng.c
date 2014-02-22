#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <pcapng.h>
#include <string.h>
#include <gio/gunixoutputstream.h>

#define PACKET_SIZE 2048

#define INPUT_SIZE (PACKET_SIZE * 2)

typedef struct AppContext AppContext;
struct AppContext
{
  GMainLoop *loop;
  GCancellable *cancel;
  gchar *input_file;
  gchar *output_file;
  GOutputStream *output;
  GInputStream *input;
  guint8 *input_buffer;
  gsize input_buffer_size; /* Maximum capacity */
  gsize input_buffer_len; /* Current length */
  guint8 *packet_buffer;
  gboolean parse_ts;
  guint64 current_ts;
  guint64 ts_period;
};

static void
app_cleanup(AppContext* app)
{
  g_clear_object(&app->cancel);
  g_clear_object(&app->output);
  g_clear_object(&app->input);
  g_free(app->input_buffer);
  g_free(app->packet_buffer);
}

static void
app_init(AppContext *app)
{
  app->loop = NULL;
  app->input_file = NULL;
  app->output_file = NULL;
  app->output = NULL;
  app->input_buffer_size = INPUT_SIZE;
  app->input_buffer_len = 0;
  app->input_buffer = g_new(guint8, app->input_buffer_size);
  app->packet_buffer = g_new(guint8, PACKET_SIZE);
  app->cancel = g_cancellable_new();
}

AppContext app;

static guint start_ts_ms = 0;
static guint ts_period_ms = 1;

const GOptionEntry app_options[] = {
  {"output", 0, 0, G_OPTION_ARG_FILENAME, &app.output_file, 
   "Write output to this file. If not given write to stdout", "FILE"},
  {"input", 0, 0, G_OPTION_ARG_FILENAME, &app.input_file, 
   "Read input from this file. If not given read from stdin", "FILE"},
  {"parse-ts", 't', 0, G_OPTION_ARG_NONE,
   &app.parse_ts,
   "Interpret first number on line as time since previus packet in ms",
   NULL},
  {"ts-period", 0, 0, G_OPTION_ARG_INT,
   &ts_period_ms,
   "Period between packets in ms, if none given in file",
   NULL},
  {"ts-start", 0, 0, G_OPTION_ARG_INT,
   &start_ts_ms,
   "Timestamp of first packet in ms, if none given in file",
   NULL},
  
  {NULL}
};

static gboolean
open_output(AppContext *app, GError **err)
{
  ByteChain *options = NULL;
  if (app->output_file) {
    GFile *file;
    file =  g_file_new_for_path (app->output_file);
    app->output =
      G_OUTPUT_STREAM(g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, err));
    g_object_unref(file);
    if (!app->output) return FALSE;
  } else {
    app->output = G_OUTPUT_STREAM(g_unix_output_stream_new(1, FALSE));
  }
  pcapng_add_string_option(&options, PCAPNG_OPTION_SHB_USERAPPL, "hex2pcapng"); 
  if (!pcapng_write_section_header(app->output, -1, options, err)) {
    pcapng_clear_options(&options);
    return FALSE;
  }
  pcapng_clear_options(&options);
  if (!pcapng_write_interface_description(app->output, DLT_PROFIBUS_DL,
					  256, options, err)) {
    pcapng_clear_options(&options);
    return FALSE;
  }
  pcapng_clear_options(&options);
  return TRUE;
}

static gboolean
open_input(AppContext *app, GError **err)
{
  if (app->input_file) {
    GFile *file;
    file =  g_file_new_for_path (app->input_file);
    app->input = G_INPUT_STREAM(g_file_read(file, NULL, err));
    g_object_unref(file);
    if (!app->input) return FALSE;
  } else {
    app->input = G_INPUT_STREAM(g_unix_input_stream_new(0, FALSE));
  }
  return TRUE;
}

static void
skip_white(const gchar **p, const gchar *end)
{
  while(*p < end && g_ascii_isspace(**p)) (*p)++;
}

static gboolean
parse_hex(const gchar **pp, const gchar *end, guint *vp)
{
  const gchar *p = *pp;
  guint v = 0;
  while(p < end && g_ascii_isxdigit(*p)) {
    v = v *16 + g_ascii_xdigit_value(*p);
    p++;
  }
  if (*pp == p) return FALSE;
  *vp = v;
  *pp = p;
  return TRUE;
}

static gboolean
parse_dec(const gchar **pp, const gchar *end, guint *vp)
{
  const gchar *p = *pp;
  guint v = 0;
  while(p < end && g_ascii_isdigit(*p)) {
    v = v *10 + g_ascii_digit_value(*p);
    p++;
  }
  if (*pp == p) return FALSE;
  *vp = v;
  *pp = p;
  return TRUE;
}

static void
parse_line(AppContext *app, const gchar *p, const gchar *lend)
{
  gsize len;
  GError *err = NULL;
  guint8 *b = app->packet_buffer;
  g_debug("'%.*s'", lend-p, p);
  skip_white(&p, lend);
  if (p == lend || *p == '#') {
    /* Ignore line */
    return;
  }
  if (app->parse_ts) {
    guint ts;
    if (!parse_dec(&p, lend, &ts)) {
      g_warning("Invalid timestamp");
      return;
    }
    app->current_ts = ts*(guint64)1000;
    skip_white(&p, lend);
  }
  while(p < lend) {
    guint v;
    if (*p == '#') {
      /* Skip rest of line */
      break;
    }
    if (!parse_hex(&p, lend, &v) || v > 0xff) {
      g_warning("Invalid hex byte");
      return;
    }
    *b++ = v;
    skip_white(&p, lend);
  }
  len = b - app->packet_buffer;
  if (len == 0) return;
  if (!pcapng_write_enhanced_packet(app->output, 0, app->current_ts, len, len,
				    app->packet_buffer, NULL, &err)) {
    g_warning("Failed to write packet to output: %s", err->message);
    g_clear_error(&err);
  }
  app->current_ts += app->ts_period;
}

static inline gboolean
is_eol(gchar c)
{
  return c == '\n' || c == '\r';
}

static void
read_callback(GObject *source_object,
	      GAsyncResult *res,
	      gpointer user_data)
{
  GError *err = NULL;
  AppContext *app = user_data;
  gssize len;
  const gchar *p;
  const gchar *end;
  len = g_input_stream_read_finish(G_INPUT_STREAM(source_object), res, &err);
  if (len < 0) {
    if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_printerr("Failed to read from input: %s\n", err->message);
    }
    g_main_loop_quit(app->loop);
    return;
  }
  if (len == 0) {
    g_main_loop_quit(app->loop);
    return;
  }
  app->input_buffer_len += len;
  
  end = (gchar*)app->input_buffer + app->input_buffer_len;
  p = (gchar*)app->input_buffer;
  while(TRUE) {
    const gchar *lend = p;
    while(lend < end && !is_eol(*lend)) lend++;
    if (lend == end) {
      /* Move all unhandled data to the beginning of the buffer */
      memmove(app->input_buffer, p, end - p);
      app->input_buffer_len = end - p;
      break;
    }
    parse_line(app, p, lend);
    p = lend + 1;
  }
  if (app->input_buffer_len  >= app->input_buffer_size) {
    app->input_buffer_len = 0;
    g_warning("Input line too long");
  }
  g_input_stream_read_async(app->input,
			    app->input_buffer + app->input_buffer_len,
			    app->input_buffer_size - app->input_buffer_len,
			    G_PRIORITY_DEFAULT, app->cancel,
			    read_callback, app);
}

static gboolean
sigint_handler(gpointer user_data)
{
  g_cancellable_cancel(G_CANCELLABLE(user_data));
  return TRUE;
}

int
main(int argc, char *argv[])
{
  GError *err = NULL;
  GOptionContext *opt_ctxt;
  g_type_init();
  app_init(&app);

  opt_ctxt = g_option_context_new (" - convert hex encoded packets to PCAPNG");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }

  g_option_context_free(opt_ctxt);

  app.current_ts = start_ts_ms * (guint64)1000;
  app.ts_period = ts_period_ms * (guint64)1000;
  
  if (!open_output(&app, &err)) {
    g_printerr("Failed to open output: %s\n", err->message);
    g_clear_error(&err);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  
  if (!open_input(&app, &err)) {
    g_printerr("Failed to open input: %s\n", err->message);
    g_clear_error(&err);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }

  g_input_stream_read_async(app.input,
			    app.input_buffer, app.input_buffer_size,
			    G_PRIORITY_DEFAULT, app.cancel,
			    read_callback, &app);
  app.loop = g_main_loop_new(NULL, FALSE);
  g_unix_signal_add(SIGINT, sigint_handler, app.cancel);
  g_unix_signal_add(SIGTERM, sigint_handler, app.cancel);
  g_debug("Starting");
  g_main_loop_run(app.loop);
  g_main_loop_unref(app.loop);
  g_debug("Exiting"); 

  app_cleanup(&app);
  return EXIT_SUCCESS;
}
