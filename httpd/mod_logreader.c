#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"

static void register_hooks(apr_pool_t *pool);
static int logreader_handler(request_rec *r);


module AP_MODULE_DECLARE_DATA   logreader_module =
{ 
  STANDARD20_MODULE_STUFF,
#if 0
  create_dir_conf, /* Per-directory configuration handler */
  merge_dir_conf,  /* Merge handler for per-directory configurations */
#else
  NULL,
  NULL,
#endif
#if 0
  create_svr_conf, /* Per-server configuration handler */
  merge_svr_conf,  /* Merge handler for per-server configurations */
  directives,      /* Any directives we may have for httpd */
#else
  NULL,
  NULL,
  NULL,
#endif
  register_hooks   /* Our hook registering function */
};

static void register_hooks(apr_pool_t *pool) 
{
  ap_hook_handler(logreader_handler, NULL, NULL, APR_HOOK_LAST);

}


static int logreader_handler(request_rec *r)
{
  if (!r->handler || strcmp(r->handler, "logreader-handler")) return DECLINED;
  ap_set_content_type(r, "text/plain");
  ap_rprintf(r,"URI: %s\n",r->uri);
  ap_rprintf(r,"path_info: %s\n",r->path_info);
  ap_rprintf(r,"args %s\n",r->args);
  return OK;
}
