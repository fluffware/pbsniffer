#include <stdlib.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <glib.h>
#include <signal_extractor.h>
#include <wiretap/wtap.h>
#include <string.h>

typedef struct AppContext AppContext;
struct AppContext
{
  GError *err;
  MYSQL mysql;
  MYSQL_STMT *insert_stmt;

  gchar *dbhost;
  gchar *dbuser;
  gchar *dbpasswd;
  gchar *database;
  guint dbport;
  gchar *dbtable;
  GFile *signal_config;

  SignalExtractor *signal_extractor;
  wtap *wtap_handle;
  GTree *packet_files;
  gchar *packet_file_prefix;
  gint64 prev_file_end_ns;
  unsigned int duplicate_count;
};

#define DATABASE_ERROR (database_error_quark())
GQuark
database_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("database-error-quark");
  return error_quark;
}

#define WTAP_ERROR (wtap_error_quark())
GQuark
wtap_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("wtap-error-quark");
  return error_quark;
}



enum {
  DATABASE_ERROR_OK = 0,
  DATABASE_ERROR_CONNECTION_FAILED
};

static void 
app_cleanup(AppContext *app)
{
  if (app->insert_stmt) {
    mysql_stmt_close(app->insert_stmt);
  }
  mysql_close(&app->mysql);
  mysql_thread_end();
  if (app->wtap_handle) {
    wtap_close(app->wtap_handle);
  }
  if (app->signal_extractor) {
    signal_extractor_destroy(app->signal_extractor);
  }
  g_tree_unref(app->packet_files);
  g_clear_object(&app->signal_config);
  g_free(app->packet_file_prefix);
  g_clear_error(&app->err);
}

typedef struct TSRange TSRange;
struct TSRange
{
  gint64 start_us;
  gint64 end_us;
};

gint
range_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
  return (*(gint*)a < *(gint*)b) ? -1 : 1;
}

static void
app_init(AppContext *app)
{
  mysql_init(&app->mysql);
  mysql_thread_init();
  app->insert_stmt = NULL;
  app->signal_extractor = NULL;
  app->wtap_handle = NULL;
  app->signal_config = NULL;
  app->packet_files = g_tree_new_full(range_cmp, NULL, g_free, g_object_unref);
  app->packet_file_prefix = NULL;
  app->err = NULL;
  app->prev_file_end_ns = -1;
}

static gboolean
setup_database(AppContext *app, GError **err)
{
  char *insert_stmt_str;
  if (!mysql_real_connect(&app->mysql,app->dbhost,app->dbuser, app->dbpasswd,
			  app->database, app->dbport, NULL, 0)) {
    g_set_error(err, DATABASE_ERROR, DATABASE_ERROR_CONNECTION_FAILED,
		"Connection failed: %s", mysql_error(&app->mysql));
    return FALSE;
  }
  app->insert_stmt = mysql_stmt_init(&app->mysql);
  insert_stmt_str =
    g_strdup_printf("INSERT INTO %s (id, label, start, end, value) VALUES (?, ?, ?, ?, ?);", app->dbtable);
  if (mysql_stmt_prepare(app->insert_stmt,
			 insert_stmt_str,strlen(insert_stmt_str))) {
    g_set_error(err, DATABASE_ERROR, DATABASE_ERROR_CONNECTION_FAILED,
		"Preparing statement \"%s\" failed: %s",
		insert_stmt_str, mysql_error(&app->mysql));
    g_free(insert_stmt_str);
    return FALSE;
  }

  g_free(insert_stmt_str);
  return TRUE;
}

static gboolean
open_packet_file(AppContext *app, GFile *file, GError **err)
{
  int werr;
  gchar *err_info = NULL;
  gchar *path = g_file_get_path(file);
  app->wtap_handle = wtap_open_offline(path, &werr, &err_info, FALSE);
  g_free(path);
  if (!app->wtap_handle) {
    if (err_info) {
      g_set_error(err, WTAP_ERROR, werr,
		  "Failed to open packet file: %d %s\n", werr, err_info);
    } else {
      g_set_error(err, WTAP_ERROR, werr,
		  "Failed to open packet file: %d\n", werr);
    }
    return FALSE;
  }
  return TRUE;
}

static void
extract_callback(gpointer user_data, const gchar *id, 
		 const gchar *label,
		 gint64 start, gint64 end,
		 gint value)
{
  MYSQL_BIND bind[5];
  AppContext *app = user_data;
  unsigned long long s = start;
  unsigned long long e = end;
  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type = MYSQL_TYPE_STRING;
  bind[0].buffer = (char*)id;
  bind[0].buffer_length = strlen(id);
  bind[0].length = &bind[0].buffer_length;

  bind[1].buffer_type = MYSQL_TYPE_STRING;
  bind[1].buffer = (char*)label;
  bind[1].buffer_length = strlen(label);
  bind[1].length = &bind[1].buffer_length;

  bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
  bind[2].buffer = &s;
  bind[2].buffer_length = sizeof(s);
  bind[2].is_unsigned = TRUE;
  
  bind[3].buffer_type = MYSQL_TYPE_LONGLONG;
  bind[3].buffer = &e;
  bind[3].buffer_length = sizeof(e);
  bind[3].is_unsigned = TRUE;
  
  bind[4].buffer_type = MYSQL_TYPE_LONG;
  bind[4].buffer = &value;
  bind[4].buffer_length = sizeof(value);

  if (mysql_stmt_bind_param(app->insert_stmt, bind)) {
    g_critical("Failed to bind parameters for insert statement: %s",
	       mysql_error(&app->mysql));
    return;
  }

  if (mysql_stmt_execute(app->insert_stmt)) {
    unsigned int e = mysql_errno(&app->mysql);
    if (e == ER_DUP_ENTRY || e == ER_DUP_ENTRY_WITH_KEY_NAME) {
      app->duplicate_count++;
    } else {
      g_critical("Failed to execute insert statement: %d %s",
		 e, mysql_error(&app->mysql));
      return;
    }
  }
}

#define SD1 0x10
#define SD2 0x68
#define SD3 0xa2
#define SD4 0xdc
#define ED 0x16
#define SC 0xe5

#define SDA_LOW 0x43
#define SDN_LOW 0x44
#define SDA_HIGH 0x45
#define SDN_HIGH 0x46
#define MSRD 0x47
#define SRD_LOW 0x4c
#define SRD_HIGH 0x4d

#define DL 0x08
#define DH 0x0a
#define RDL 0x0c
#define RDH 0x0d

static gboolean
read_packets(AppContext *app, GError **err)
{
  int werr;
  gchar *err_info = NULL;
  struct wtap_pkthdr *hdr;
  gint64 ts_ns;
  gint64 data_offset;
  app->duplicate_count = 0;

  if (mysql_query(&app->mysql, "START TRANSACTION;")) {
    unsigned int e = mysql_errno(&app->mysql);
    g_critical("Failed to start transaction: %d %s",
	       e, mysql_error(&app->mysql));
  }

  while(TRUE) {
    guint8 *data;
    guint len;
    guint8 src;
    guint8 dst;
    if (!wtap_read(app->wtap_handle, &werr, &err_info, &data_offset)) {
      if (werr == 0) break;
      if (err_info) {
	g_set_error(err, WTAP_ERROR, werr,
		    "Failed to open packet file: %d %s\n", werr, err_info);
      } else {
	g_set_error(err, WTAP_ERROR, werr,
		    "Failed to open packet file: %d\n", werr);
      }
      return FALSE;
    }
    hdr = wtap_phdr(app->wtap_handle);
    data = wtap_buf_ptr(app->wtap_handle);
    
    if (hdr->len < 10) continue;
    if (data[0] == SD2) {
      if (data[1] != data[2] || data[1] + 6 != hdr->len) continue;
      len = data[1];
      data += 4;
    } else if (data[0] == SD3) {
      if (hdr->len != 14) continue;
      len = 11;
      data++;
    }
    src = data[1];
    dst = data[0];
    if ((src & 0x80) || (dst & 0x80)) continue;
    switch(data[2] & 0xcf) {
      /* Function codes that send data */
    case SDA_LOW:
    case SDN_LOW:
    case SDA_HIGH:
    case SDN_HIGH:
    case MSRD:
    case SRD_LOW:
    case SRD_HIGH:

    case DL:
    case DH:
    case RDL:
    case RDH:
      break;
    default:
      continue;
    }
    data += 3;
    len -= 3;
    ts_ns =  hdr->ts.secs * 1000000000LL +  hdr->ts.nsecs;

    signal_extractor_extract(app->signal_extractor, src, dst,
			     ts_ns,
			     data, len,
			     extract_callback, app);
  }
  if (mysql_query(&app->mysql, "COMMIT;")) {
    unsigned int e = mysql_errno(&app->mysql);
    g_critical("Failed to commit: %d %s",
	       e, mysql_error(&app->mysql));
  }
  ts_ns =  hdr->ts.secs * 1000000000LL +  hdr->ts.nsecs;
  if (ts_ns > app->prev_file_end_ns) {
    app->prev_file_end_ns = ts_ns;
  }
  if (app->duplicate_count > 0) {
    g_warning("%d inserts failed due to duplicate keys", app->duplicate_count);
    app->duplicate_count = 0;
  }
  return TRUE;
}

gboolean
select_packet_files(AppContext *app, const char *arg, GError **err)
{
  GFile *base_file = g_file_new_for_commandline_arg(arg);
  if (g_file_query_exists(base_file, NULL)) {
    gchar *path;
    gchar *end;
    TSRange *r = g_new(TSRange, 1);
    r->start_us = 0;
    r->end_us = 0;
    g_tree_insert(app->packet_files, r, base_file);
    path = g_file_get_path(base_file);
    end = rindex(path, '.');
    if (end) {
      *end = '\0';
      app->packet_file_prefix = path;
    } else {
      g_free(path);
    }
  } else {
    /* Assume it's a prefix */
    char *prefix;
    guint prefix_len;
    GFile *dir;
    GFileEnumerator *files;
    GFileInfo *info;
    dir = g_file_get_parent(base_file);
    if (!dir) {
      g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
		  "Failed to get directory name");
      g_object_unref(base_file);
      return FALSE;
    }
    if (!g_file_query_exists(dir, NULL)) {
      g_set_error(err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
		  "Directory %s not found", g_file_get_path(dir));
      g_object_unref(base_file);
      g_object_unref(dir);
      return FALSE;
    }
    prefix = g_file_get_basename(base_file);
    g_object_unref(base_file);
    prefix_len = strlen(prefix);
    g_assert(prefix);
    files = g_file_enumerate_children(dir,
				      G_FILE_ATTRIBUTE_STANDARD_NAME ","
				      G_FILE_ATTRIBUTE_STANDARD_TYPE,
				      G_FILE_QUERY_INFO_NONE, NULL, err);
    if (!files) {
      g_free(prefix);
      g_object_unref(dir);
      return FALSE;
    }

    while((info = g_file_enumerator_next_file(files, NULL, err))) {
      const char *name;
      guint32 type;
      const char *p;
      char *endnum;
      gint64 start_us;
      gint64 end_us;
      TSRange *range;
      GFile *child;
      type = g_file_info_get_attribute_uint32 (info,
					       G_FILE_ATTRIBUTE_STANDARD_TYPE);
      if (type != G_FILE_TYPE_REGULAR) continue;
      name =
	g_file_info_get_attribute_byte_string(info,
					      G_FILE_ATTRIBUTE_STANDARD_NAME);
      if (!g_str_has_prefix(name, prefix)) continue;
      /* Parse start and end times from filename */
      p = name + prefix_len;
      if (*p != '_') continue;
      p++;
      start_us = g_ascii_strtoull(p, &endnum, 10);
      if (p == endnum) continue;
      p = endnum;
      if (*p != '-') continue;
      p++;
      end_us = g_ascii_strtoull(p, &endnum, 10);
      if (p == endnum) continue;
      p = endnum;
      g_debug("File: %s %lld %lld", name, start_us, end_us);
      range = g_new(TSRange, 1);
      range->start_us = start_us;
      range->end_us = end_us;
      child = g_file_resolve_relative_path(dir, name);
      g_tree_insert(app->packet_files, range, child);
    }
    g_file_enumerator_close(files, NULL, NULL);
    g_object_unref(files);
    g_free(prefix);
    g_object_unref(dir);
    if (*err) {
      return FALSE;
    }
    app->packet_file_prefix = g_strdup(arg);
  }
  return TRUE;
}

static gboolean
read_packet_file(gpointer key,
		 gpointer value,
		 gpointer data)
{
  TSRange *range = key;
  GFile *file = value;
  AppContext *app = data;
  if (range->start_us * 1000 != app->prev_file_end_ns) {
    signal_extractor_finish(app->signal_extractor, app->prev_file_end_ns,
			    extract_callback, app);
  }
  app->prev_file_end_ns = range->end_us * 1000;

  g_debug("Range %lld - %lld", range->start_us, range->end_us);
  if (!open_packet_file(app, file,  &app->err)){
    return TRUE;
  }
  if (!read_packets(app,  &app->err)){
    return TRUE;
  }
  wtap_close(app->wtap_handle);
  app->wtap_handle = NULL;
  return FALSE;
}

static gchar *dbhost = NULL;
static gchar *dbuser = NULL;
static gchar *dbpasswd = NULL;
static gchar *database = NULL;
static guint dbport = 0;
static gchar *dbtable = NULL;
static gchar *signal_config = NULL;
static gboolean dump_filter = FALSE;

const GOptionEntry app_options[] = {
  {"db-host", 0, 0, G_OPTION_ARG_STRING,
   &dbhost, "Database host to connect to", "HOST"},
  {"db-user", 0, 0, G_OPTION_ARG_STRING,
   &dbuser, "Database user", "USER"},
  {"database", 0, 0, G_OPTION_ARG_STRING,
   &database, "Database to connect to", "DATABASE"},
  {"port", 0, 0, G_OPTION_ARG_INT,
   &database, "Database port to connect to", "PORT"},
  {"db-table", 0, 0, G_OPTION_ARG_STRING,
   &dbtable, "Database table to read signals from", "TABLE"},
  {"signal-config", 0, 0, G_OPTION_ARG_STRING,
   &signal_config, "Configuration file for signal extraction", "FILE"},
  {"dump-filter",  0, 0, G_OPTION_ARG_NONE,
   &dump_filter, "Print signal filter", NULL},
  {NULL}
};

int
main(int argc, char *argv[])
{
  GError *err = NULL;
  AppContext app;
  GOptionContext *opt_ctxt;
  opt_ctxt = g_option_context_new ("FILE");
  g_option_context_add_main_entries(opt_ctxt, app_options, NULL);
  if (!g_option_context_parse(opt_ctxt, &argc, &argv, &err)) {
    g_printerr("Failed to parse options: %s\n", err->message);
    g_error_free(err);
    return EXIT_FAILURE;
  }
  g_option_context_free(opt_ctxt);
  app.dbuser = dbuser;
  app.dbhost = dbhost;
  app.dbpasswd = dbpasswd;
  app.database = database;
  app.dbport = dbport;
  app.dbtable = dbtable;


  app_init(&app);
  if (argc < 2) {
    g_printerr("No input file\n");
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  
  if (!setup_database(&app, &err)) {
    g_printerr("Database setup failed: %s\n", err->message);
    g_error_free(err);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  if (!select_packet_files(&app, argv[1], &err)) {
    g_printerr("Failed to decide which packet files to read: %s\n",
	       err->message);
    g_error_free(err);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
#if 0
  {
    gboolean
      dump_file(gpointer key,
		gpointer value,
		gpointer data)
    {
      TSRange *range = key;
      GFile *file = value;
      g_debug("%lld - %lld %s", range->start, range->end, g_file_get_path(file));
      return FALSE;
    }
    g_tree_foreach(app.packet_files, dump_file, &app);
  }
#endif
  if (signal_config) {
    app.signal_config = g_file_new_for_commandline_arg(signal_config);
  } else if (app.packet_file_prefix) {
    gchar *path = g_strconcat(app.packet_file_prefix, "_config.xml", NULL);
    app.signal_config = g_file_new_for_path(path);
    g_free(path);
  }
  if (!app.signal_config) {
    g_printerr("No signal configuration\n");
    app_cleanup(&app);
    return EXIT_FAILURE;
  }

  app.signal_extractor = signal_extractor_new();
  if (!signal_extractor_config(app.signal_extractor, app.signal_config, &err)){
    gchar *path = g_file_get_path(app.signal_config);
    g_printerr("Failed to read signal configuration from '%s': %s\n",
	       path , err->message);
    g_free(path);
    g_error_free(err);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }
  if (dump_filter) {
    gchar *dump = signal_extractor_dump_filter(app.signal_extractor);
    g_printerr("%s", dump);
    g_free(dump);
  }
  g_tree_foreach(app.packet_files, read_packet_file, &app);
  if (app.err) {
    g_printerr("Failed to read packet file: %s\n", app.err->message);
    app_cleanup(&app);
    return EXIT_FAILURE;
  }

  /* Finish any active traces */
  signal_extractor_finish(app.signal_extractor, app.prev_file_end_ns,
			  extract_callback, &app);

  if (app.duplicate_count > 0) {
    g_warning("%d inserts failed due to duplicate keys", app.duplicate_count);
  }

  app_cleanup(&app);
  return EXIT_SUCCESS;
}
