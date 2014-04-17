#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"
#include <apr.h>
#include <util_script.h>
#include <util_varbuf.h>
#include <apr_strings.h>
#include <apr_dbd.h>
#include <mod_dbd.h>
#include <http_log.h>

static void register_hooks(apr_pool_t *pool);
static int logreader_handler(request_rec *r);


typedef struct 
{
  char *database;
  char *log_table;
} logreader_config;

#if 0
static void *
create_svr_conf(apr_pool_t *p, server_rec *s)
{
  logreader_config *config = apr_palloc(p, sizeof(logreader_config));
  if (config) {
    config->database = NULL;
    config->log_table = NULL;
  }
  return config;
}
#endif

static void *create_dir_conf(apr_pool_t *p, char *dir)
{
  logreader_config *config = apr_palloc(p, sizeof(logreader_config));
  if (config) {
    config->database = NULL;
    config->log_table = NULL;
  }
  return config;
}

static void *
merge_conf(apr_pool_t *p, void *base_conf, void *new_conf)
{
  logreader_config *lc = apr_palloc(p, sizeof(logreader_config));
  if (lc) {
    logreader_config *nlc = new_conf;
    logreader_config *blc = base_conf;
    lc->database = nlc->database ? nlc->database : blc->database;
    lc->log_table = nlc->log_table ? nlc->log_table : blc->log_table;
  }
  return lc;
}

static const char *set_database(cmd_parms *cmd, void *cfg, const char *arg)
{
  logreader_config *lc = (logreader_config*)cfg;
  lc->database = apr_pstrdup(cmd->pool, arg);
  return NULL;
}

static const char *set_table(cmd_parms *cmd, void *cfg, const char *arg)
{
  logreader_config *lc = (logreader_config*)cfg;
  lc->log_table = apr_pstrdup(cmd->pool, arg);
  return NULL;
}

static const command_rec logreader_directives[] =
{
    AP_INIT_TAKE1("LogreaderDatabase", set_database, NULL,
		  RSRC_CONF | ACCESS_CONF,
		  "Database to connect to, if not set connect to "
		  "the default database"),
    AP_INIT_TAKE1("LogreaderLogTable", set_table, NULL,
		  RSRC_CONF | ACCESS_CONF,
		  "Database table for logged signals"),
    { NULL }
};

module AP_MODULE_DECLARE_DATA   logreader_module =
{ 
  STANDARD20_MODULE_STUFF,

  create_dir_conf, /* Per-directory configuration handler */
  merge_conf,  /* Merge handler for per-directory configurations */
#if 0
  create_svr_conf, /* Per-server configuration handler */
  merge_conf,  /* Merge handler for per-server configurations */
#else
  NULL,
  NULL,
#endif
  logreader_directives,      /* Any directives we may have for httpd */
  register_hooks   /* Our hook registering function */
};

static void register_hooks(apr_pool_t *pool) 
{
  ap_hook_handler(logreader_handler, NULL, NULL, APR_HOOK_LAST);

}

static int json_error(request_rec *r, const char *format, ...)
{
  va_list ap;
  ap_set_content_type(r, "application/json;charset=UTF-8");
  ap_rputs("{\"error\":\"",r);
  va_start(ap, format);
  ap_vrprintf(r, format, ap);
  va_end(ap);
  ap_rputs("\"}\n",r);
  return OK;
}
#define COL_ID 0
#define COL_LABEL 1
#define COL_SRC 2
#define COL_DEST 3
#define COL_BIT_OFFSET 4
#define COL_BIT_WIDTH 5
#define COL_TYPE 6

#define COL_START 1
#define COL_END 2
#define COL_VALUE 3


static int
check_symbol(const char *s)
{
  while(*s != '\0') {
    if ((*s < 'a' || *s > 'z') && (*s < 'A' || *s > 'Z') &&
	(*s < '0' || *s > '9') && *s != '_') {
      return 0;
    }
    s++;
  }
  return 1;
}

#define NS_TO_MS(x) ((x)/1000000)
#define MS_TO_NS(x) ((x)*1000000)

static void 
append_int64(struct ap_varbuf *buf, apr_int64_t v)
{
  char str[25];
  apr_snprintf(str, sizeof(str), "%" APR_INT64_T_FMT, v);
  ap_varbuf_strcat(buf, str);
}

static apr_dbd_prepared_t *
build_values_stmt(request_rec *r, ap_dbd_t*db, const char *table,
		  apr_int64_t start, apr_int64_t end)
{
  char *query;
  apr_dbd_prepared_t *stmt = NULL;
  struct ap_varbuf querybuf;
  int res;

  ap_varbuf_init(r->pool, &querybuf, 128);
  ap_varbuf_strcat(&querybuf, "SELECT id, start, end, value FROM ");
  ap_varbuf_strcat(&querybuf, table);
  ap_varbuf_strcat(&querybuf, "_values WHERE");
  ap_varbuf_strcat(&querybuf, " id = %s");
  if (start >= 0) {
    ap_varbuf_strcat(&querybuf, " AND end >= ");
    append_int64(&querybuf, start);
  }
  if (end >= 0) {
    ap_varbuf_strcat(&querybuf, " AND start < ");
    append_int64(&querybuf, end);
  }
  query = ap_varbuf_pdup(r->pool, &querybuf, NULL, 0,
			 " ORDER BY start;", 16, NULL);
  ap_varbuf_free(&querybuf);
  res = apr_dbd_prepare(db->driver, r->pool, db->handle, query, NULL, &stmt);
  if (res) {
    ap_log_rerror(APLOG_MARK,  APLOG_ERR , res, r, "Failed to prepare statement: %s", query);
    return NULL;
  }
  return stmt;
}

static apr_dbd_prepared_t *
build_signals_stmt(request_rec *r, ap_dbd_t*db, const char *table,
		   apr_array_header_t *signals)
{
  char *query;
  struct ap_varbuf querybuf;
  apr_dbd_prepared_t *stmt = NULL;
  int res;
  ap_varbuf_init(r->pool, &querybuf, 128);
  ap_varbuf_strcat(&querybuf,
		   "SELECT id, label, src,dest,bit_offset, bit_width, type "
		   "FROM ");
  ap_varbuf_strcat(&querybuf, table);
  ap_varbuf_strcat(&querybuf, "_signals");
  if (!apr_is_empty_array(signals)) {
    unsigned int i;
    char *s = APR_ARRAY_IDX(signals, 0, char*);    
    ap_varbuf_strcat(&querybuf, " WHERE id = \"");
    ap_varbuf_strcat(&querybuf, s);
    for (i = 1; i < signals->nelts; i++) {
       ap_varbuf_strcat(&querybuf, "\" OR id = \"");
       s = APR_ARRAY_IDX(signals, i, char*);    
       ap_varbuf_strcat(&querybuf, s);
    }
    ap_varbuf_strcat(&querybuf, "\"");
  }
  ap_varbuf_strcat(&querybuf, ";");
  query = ap_varbuf_pdup(r->pool, &querybuf, NULL, 0, "", 0, NULL);
  ap_varbuf_free(&querybuf);
  res = apr_dbd_prepare(db->driver, r->pool, db->handle, query, NULL, &stmt);
  if (res) {
    ap_log_rerror(APLOG_MARK,  APLOG_ERR , res, r, "Failed to prepare statement: %s", query);
    return NULL;
  }
  return stmt;
}


static int
do_select(request_rec *r, const char *table, apr_array_header_t *signals,
	  apr_int64_t start, apr_int64_t end,
	  apr_int64_t min_duration)
{
  apr_dbd_prepared_t *values_stmt = NULL;
  apr_dbd_prepared_t *signals_stmt = NULL;
  apr_dbd_results_t *result = NULL;
  apr_dbd_results_t *values_result = NULL;
  apr_dbd_row_t *row = NULL;
  ap_dbd_t*db;

  logreader_config *lc = (logreader_config*)
    ap_get_module_config(r->per_dir_config, &logreader_module);

  db = ap_dbd_acquire(r);
  if (!db) {
    return json_error(r,"Failed to acquire database connection");
  }
  if (lc->database) {
    int e = apr_dbd_set_dbname(db->driver, r->pool,  db->handle, lc->database);
    if (e != 0) {
      return json_error(r,"Failed to select database '%s'",lc->database);
    }
  }
  values_stmt = build_values_stmt(r, db, table, start, end);
  if (!values_stmt) return json_error(r,"Failed to prepare values select");
  signals_stmt = build_signals_stmt(r, db, table,signals);
  if (!signals_stmt) return json_error(r,"Failed to prepare signal select");
  if (apr_dbd_pselect(db->driver, r->pool, db->handle, &result, signals_stmt,0,0, NULL)){
     return json_error(r,"Failed to select signals");
  }

  ap_set_content_type(r, "application/json;charset=UTF-8");
  ap_rprintf(r,"{\"start\":%lld,\"end\":%lld",start,end);

  while(apr_dbd_get_row(db->driver, r->pool,  result, &row, -1) == 0) {
    apr_int64_t prev_end = -1;
    int short_duration = 0;  // The previous span was too short
    const char *id = apr_dbd_get_entry(db->driver, row, COL_ID);
    const char *label = apr_dbd_get_entry(db->driver, row, COL_LABEL);
    const char *src = apr_dbd_get_entry(db->driver, row, COL_SRC);
    const char *dest = apr_dbd_get_entry(db->driver, row, COL_DEST);
    const char *bit_offset = apr_dbd_get_entry(db->driver, row, COL_BIT_OFFSET);
    const char *bit_width = apr_dbd_get_entry(db->driver, row, COL_BIT_WIDTH);
    const char *type = apr_dbd_get_entry(db->driver, row, COL_TYPE);
    ap_rprintf(r,",\"%s\":{\"label\":\"%s\"",id,label);
    ap_rprintf(r,",\"src\":%s,\"dest\":%s",src,dest);
    ap_rprintf(r,",\"bit_offset\":%s,\"bit_width\":%s",bit_offset,bit_width);
    ap_rprintf(r,",\"type\":\"%s\", \"values\":[",type);
#if 1
    if (apr_dbd_pselect(db->driver, r->pool, db->handle, &values_result, values_stmt,0,1, &id)) {
      return json_error(r,"Failed to select values");
    }
    while(apr_dbd_get_row(db->driver, r->pool,  values_result, &row, -1) == 0) {
      const char *start_str;
      const char *end_str;
      const char *value_str;
      apr_int64_t start;
      apr_int64_t end;

      start_str = apr_dbd_get_entry(db->driver, row, COL_START);
      start = apr_strtoi64(start_str, NULL, 0);
      end_str = apr_dbd_get_entry(db->driver, row, COL_END);
      end = apr_strtoi64(end_str, NULL, 0);
      value_str = apr_dbd_get_entry(db->driver, row, COL_VALUE);
      if (start != prev_end && prev_end != -1) {
	short_duration = 0;
	ap_rprintf(r,"%lld,null,",prev_end);
      }
      if (end - start < min_duration) {
	if (!short_duration) {
	  ap_rprintf(r,"%lld,\"-\",",start);
	  short_duration = 1;
	}
      } else {
	short_duration = 0;
	ap_rprintf(r,"%lld,%s,",start, value_str);
      }
      prev_end = end;
    }
    if (prev_end != -1) {
      ap_rprintf(r,"%lld,null",prev_end);
    }
    ap_rputs("]}",r);
#endif
  }
  ap_rputs("}\n",r);
 
  return OK;
}


static int
logreader_handler(request_rec *r)
{
  const char *start_str;
  const char *end_str;
  const char *signal_str;
  const char *min_duration_str;
  apr_int64_t start = 0;
  apr_int64_t end = -1;
  apr_int64_t min_duration = -1;
  apr_table_t *get_args;
  const char  *path;
  const char *table;
  apr_array_header_t *signals;
  logreader_config *lc = (logreader_config*)
    ap_get_module_config(r->per_dir_config, &logreader_module);
  if (!r->handler || strcmp(r->handler, "logreader-handler")) return DECLINED;
  if (*r->path_info == '\0') {
    ap_set_content_type(r, "text/plain");
    ap_rprintf(r,"URI: %s\n",r->uri);
    ap_rprintf(r,"args %s\n",r->args);
    ap_rprintf(r,"Database %s\n",lc->database);
    ap_rprintf(r,"Table %s\n",lc->log_table);
  }
  table = lc->log_table;
  path = r->path_info;
  if (path && (path[0] != '\0' && !(path[0] == '/' && path[1] == '\0'))) {
    path++;
    if (!check_symbol(path)) {
      return json_error(r,"Illegal database table '%s'", path);
    }
    table = path;
  }
  if (!table) return json_error(r,"No table");

  ap_args_to_table(r, &get_args);
  start_str = apr_table_get(get_args, "start");
  if (start_str) {
    char *end;
    start = apr_strtoi64(start_str, &end,10);
    if (start_str == end || *end != '\0') {
      return json_error(r,"Invalid start time '%s'", start_str);
    }
   
  }
  end_str = apr_table_get(get_args, "end");
  if (end_str) {
    char *endp;
    end = apr_strtoi64(end_str, &endp,10);
    if (end_str == endp || *endp != '\0') {
      return json_error(r,"Invalid end time '%s'", end_str);
    }
  
  }
  if (start >= end) {
    return json_error(r,"Start time must be less than end time");
  }
  min_duration_str = apr_table_get(get_args, "min_duration");
  if (min_duration_str) {
    char *endp;
    min_duration = apr_strtoi64(min_duration_str, &endp,10);
    if (end_str == endp || *endp != '\0' || min_duration < 0) {
      return json_error(r,"Invalid minimum duration time '%s'", min_duration_str);
    }
  }
  signals = apr_array_make(r->pool, 0, sizeof(char*));
  signal_str = apr_table_get(get_args, "signals");
  if (!signal_str) {
    return json_error(r,"No signals");
  } if (signal_str[0] == '*' && signal_str[1] == '\0') {
  } else {
    char *last;
    char *signal = apr_strtok(apr_pstrdup(r->pool,signal_str), ",", &last);
    if (!signal) return json_error(r,"No signals");
    while (signal) {
      if (!check_symbol(signal))  return json_error(r,"Illegal signal id");
      APR_ARRAY_PUSH(signals, char*) = signal;

      signal = apr_strtok(NULL, ",", &last);
    }
  }
  
  return do_select(r, table, signals, start, end, min_duration);
}
