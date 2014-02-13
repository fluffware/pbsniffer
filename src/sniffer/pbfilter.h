#ifndef __PBFILTER_H__ZC8FVVOZZV__
#define __PBFILTER_H__ZC8FVVOZZV__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define PB_FILTER_ERROR (pb_filter_error_quark())

enum {
  PB_FILTER_ERROR_OK = 0,
};

#define PB_FILTER_TYPE                  (pb_filter_get_type ())
#define PB_FILTER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PB_FILTER_TYPE, PBFilter))
#define PB_FILTER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PB_FILTER_TYPE, PBFilterClass))
#define IS_PB_FILTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PB_FILTER_TYPE))
#define IS_PB_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PB_FILTER_TYPE))
#define PB_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PB_FILTER_TYPE, PBFilterClass))

typedef struct _PBFilter PBFilter;
typedef struct _PBFilterClass PBFilterClass;

PBFilter *
pb_filter_new();

typedef void (*PBFilterOutput)(gsize length, const guint8 *data, gint64 ts,
			       gpointer user_data);

void
pb_filter_set_output(PBFilter *filter, PBFilterOutput cb, gpointer user_data);

void
pb_filter_input(PBFilter *filter, gsize length, const guint8 *data, gint64 ts);

void
pb_filter_output_state(PBFilter *filter);

#endif /* __PBFILTER_H__ZC8FVVOZZV__ */
