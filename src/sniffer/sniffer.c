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

GQuark
pb_parser_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("pb-parser-error-quark");
  return error_quark;
}

guint8
checksum(const guint8 *bytes, gsize len)
{
  guint8 sum = 0;
  while(len-- > 0) {
    sum += *bytes++;
  }
  return sum;
}

static void
decode_address(FILE *out, guint8 *bytes, guint8 len)
{
  guint8 src;
  guint8 dest;
  if (len < 2) return;
  src = bytes[1];
  dest = bytes[0];
  if (src & 0x80 && len > 4) {
    fprintf(out, "%d(%d) -> ", src & 0x7f, bytes[4]);
  } else {
    fprintf(out, "%d -> ", src & 0x7f);
  }
  if (dest & 0x80 && len > 3) {
    fprintf(out, "%d(%d)", dest & 0x7f, bytes[3]);
  } else {
    fprintf(out, "%d", dest & 0x7f);
  }
}

static void
decode_function_code(FILE *out, guint8 fc)
{
  if (fc & 0x40) {
    fputs("Request, ", out);
    fputs("FCV=", out);
    fputc((fc & 0x10) ? '1':'0', out);
    fputs(", FCB=", out);
    fputc((fc & 0x20) ? '1':'0', out);
    fputs(", ", out);
    switch(fc & 0x8f) {
    case 0x80:
      fputs("CV", out);
      break;
    case 0x00:
      fputs("TE", out);
      break;
    case 0x03:
      fputs("SDA_LOW", out);
      break;
    case 0x04:
      fputs("SDN_LOW", out);
      break;
    case 0x05:
      fputs("SDA_HIGH", out);
      break;
    case 0x06:
      fputs("SDN_HIGH", out);
      break;
    case 0x07:
      fputs("MSRD", out);
      break;
    case 0x09:
      fputs("Request FDL Status", out);
      break;
    case 0x0c:
      fputs("SRD low", out);
      break;
    case 0x0d:
      fputs("SRD high", out);
      break;
    case 0x0e:
      fputs("Request Ident with reply", out);
      break;
    case 0x0f:
      fputs("Request LSAP with reply", out);
      break;
    default:
       fputs("Invalid (reserved)", out);
       break;
    }
  } else {
    fputs("Response, ", out);
    switch(fc & 0x30) {
    case 0x00:
      fputs("Slave", out);
      break;
    case 0x10:
      fputs("Master not ready", out);
      break;
    case 0x20:
      fputs("Master ready, without token", out);
      break;
    case 0x30:
      fputs("Master ready, in token ring", out);
      break;
    }
    fputs(", ", out);
    switch(fc & 0x0f) {
    case 0x00:
      fputs("OK", out);
      break;
    case 0x01:
      fputs("UE", out);
      break;
    case 0x02:
      fputs("RR", out);
      break;
    case 0x03:
      fputs("RS", out);
      break;
    case 0x08:
      fputs("DL", out);
      break;
    case 0x10:
      fputs("DH", out);
      break;
    case 0x12:
      fputs("RDL", out);
      break;
    case 0x13:
      fputs("RDH", out);
      break;
    default:
      fputs("Invalid (reserved)", out);
      break;
    }
  }
}

static gchar *diag_flags[3][8] =
  {
    {"Station_Non_Existent",
     "Station_Not_Ready",
     "Cfg_Fault",
     "Ext_Diag",
     "Not_Supported",
     "Invalid_Slave_Response",
     "Prm_Fault",
     "Master_lock"},
    {"Prm_Req",
     "Stat_Diag",
     NULL,
     "WD_On",
     "Freeze_Mode",
     "Sync_Mode",
     NULL,
     "Deactivated"},
    {NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     NULL,
     "Ext_Diag_Overflow"}
  };

static void
decode_diagnostics(FILE *out, guint8 *bytes, gsize len)
{
  guint byte;
  guint bit;
  if (len < 6) return;
  for(byte = 0; byte < 3; byte++) {
    for(bit = 0; bit < 8; bit++) {
      if (bytes[byte] & (1<<bit)) {
	if (diag_flags[byte][bit] != NULL) {
	  fputs(diag_flags[byte][bit], out);
	  fputs(", ", out);
	}
      }
    }
  }
  fprintf(out, "Master_Addr=%d, ", bytes[3]);
  fprintf(out, "Ident=0x%04x", (bytes[4]<<8) | bytes[5]);
}

static void
decode_header(FILE *out, guint8 *bytes, guint8 len)
{
  decode_address(out, bytes, len);
  if (len >= 3) {
    gsize i = 3;
    fprintf(stdout, ", FC=0x%02x", bytes[2]);
    if (bytes[0] & 0x80) i++;
    if (bytes[1] & 0x80) i++;
    if (i < len) {
      fputs(", PDU=", out);
      for (; i < len; i++) {
	fprintf(out, " %02x", bytes[i]);
      }
    }
    fputc('\n', out);
    decode_function_code(out, bytes[2]);
  }
  fputc('\n', out);
  if (((bytes[2] & 0x70) == 0x00) && len > 4
      && (bytes[1] & 0x80) && bytes[4] == 60) {
    fputs("Diagnostics: ",out);
    decode_diagnostics(out, bytes + 5, len - 5);
    fputc('\n', out);
  }
}


#define PB_PARSER_ERROR (pb_parser_error_quark())

enum {
  PB_PARSER_ERROR_EXTRA_DATA,
  PB_PARSER_ERROR_TOO_LONG,
  PB_PARSER_ERROR_TOO_SHORT,
  PB_PARSER_ERROR_WRONG_END,
  PB_PARSER_ERROR_LENGTH_MISMATCH
};

static gboolean
check_checksum(const guint8 *bytes, gsize len, GError **err)
{
  guint8 cs = checksum(bytes, len);
  if (cs != bytes[len]) {
    g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_LENGTH_MISMATCH,
		"Checksum error. Calculated 0x%02x, got 0x%02x",
		cs, bytes[len]);
    return FALSE;
  }
  return TRUE;
}

static gboolean
parse_byte(AppContext *app, guint8 in, GError **err)
{
  if (app->packet_len >= MAX_PACKET_LEN) {
    g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_TOO_LONG,
		"Telegram too long");
    return FALSE;
  }
  
  app->packet_buffer[app->packet_len++] = in;
  if (app->packet_len == 1) {
    switch(app->packet_buffer[0]) {
    case SC:
      fprintf(stdout, "\nShort confirmation\n");
      return FALSE;
    case SD1:
    case SD2:
    case SD3:
    case SD4:
      g_debug("Packet start");
      break;
    default:
      g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_EXTRA_DATA,
		  "Extra data outside telegram");
      return FALSE;
    }
    return TRUE;
  } else if(app->packet_len == 3) {
    switch(app->packet_buffer[0]) {
    case SD4:
      fputs("\nToken ",stdout);
      decode_header(stdout, app->packet_buffer + 1, 2);
      return FALSE;
    case SD2:
      if (app->packet_buffer[1] == app->packet_buffer[2]) {
	app->telegram_len = app->packet_buffer[1];
      } else {
	g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_LENGTH_MISMATCH,
		    "Variable telegram length error");
	return FALSE;
      }
      if (app->telegram_len < 4) {
	g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_TOO_SHORT,
		    "Variable telegram length too short");
	return FALSE;
      }
      if (app->telegram_len > 249) {
	g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_TOO_LONG,
		    "Variable telegram length too long");
	return FALSE;
      }
      g_debug("Telegram length %d", app->telegram_len);
      break;
    }
    return TRUE;
  } else if (app->packet_len == 6) {
    if (app->packet_buffer[0] == SD1) {
      if (app->packet_buffer[5] != ED) {
	g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_WRONG_END,
		    "Telegram doesn't end with ED");
	return FALSE;
      }
      if (!check_checksum(app->packet_buffer +1, 3, err)) return FALSE;
      fputs("\nTelegram ", stdout);
      decode_header(stdout, app->packet_buffer+1, 3);
      return FALSE;
    }
  } else {
    switch(app->packet_buffer[0]) {
    case SD2:
      if (app->telegram_len + 6  == app->packet_len) {
	if (app->packet_buffer[app->packet_len - 1] != ED) {
	  g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_WRONG_END,
		    "Telegram doesn't end with ED");
	  return FALSE;
	}
	if (!check_checksum(app->packet_buffer +4, app->telegram_len, err))
	  return FALSE;
	fputs("\nTelegram ", stdout);
	decode_header(stdout, app->packet_buffer + 4, app->telegram_len);
	return FALSE;
      }
      break;
    case SD3:
      if (app->packet_len == 14) {
	if (app->packet_buffer[13] != ED) {
	  g_set_error(err, PB_PARSER_ERROR, PB_PARSER_ERROR_WRONG_END,
		    "Telegram doesn't end with ED");
	  return FALSE;
	}
	if (!check_checksum(app->packet_buffer +1, 11, err))
	  return FALSE;
	fputs("\nTelegram ", stdout);
	decode_header(stdout, app->packet_buffer + 1, 11);
	return FALSE;
      }
      break;
    }
  }
  return TRUE;
}

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
    if (!parse_byte(app, app->read_buffer[i], &err)) {
      if (err) {
	fflush(stdout);
	g_warning("%s", err->message);
	g_clear_error(&err);
      }
      app->packet_len = 0;
      app->telegram_len = -1;
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
