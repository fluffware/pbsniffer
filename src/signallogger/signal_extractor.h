#include <glib.h>
#include <gio/gio.h>

#define SIGNAL_EXTRACTOR_ERROR (signal_extractor_error_quark())

#define SIGNAL_TYPE_UNKNOWN 0 
#define SIGNAL_TYPE_UNSIGNED_INT 1
#define SIGNAL_TYPE_SIGNED_INT 2

enum {
  SIGNAL_EXTRACTOR_ERROR_OK = 0,
  SIGNAL_EXTRACTOR_ERROR_UNKOWN_ELEMENT,
  SIGNAL_EXTRACTOR_ERROR_MISSING_PARAMETER,
  SIGNAL_EXTRACTOR_ERROR_INVALID_VALUE,
  SIGNAL_EXTRACTOR_ERROR_MULTIPLE_ELEMENTS

};

typedef struct SignalFilter SignalFilter;
struct SignalFilter
{
  SignalFilter *next;
  gchar *id;
  gchar *label;
  gint bit_offset;
  gint bit_width;
  gint type;
  gint64 last_timestamp_ns;
  gint last_value;
};

typedef struct PacketTrace PacketTrace;
struct PacketTrace
{
  guint src;
  guint dst;
  SignalFilter *signals;
};

typedef struct SignalExtractor SignalExtractor;
struct SignalExtractor
{
  GTree *traces;
};

SignalExtractor *
signal_extractor_new(void);

void
signal_extractor_destroy(SignalExtractor *se);

gboolean
signal_extractor_config(SignalExtractor *se, GFile *config_file, GError **err);

typedef void (*SignalExtractorCallback)(gpointer user_data, const gchar *id, 
					const gchar *label,
					gint64 start, gint64 end,
					gint value);
void
signal_extractor_extract(SignalExtractor *se, guint src, guint dst,
			 gint64 timestamp_ns,
			 const guint8 *block, gsize block_size,
			 SignalExtractorCallback callback, gpointer user_data);

void
signal_extractor_finish(SignalExtractor *se, 
			gint64 timestamp_ns,
			SignalExtractorCallback callback, gpointer user_data);


gchar *
signal_extractor_dump_filter(SignalExtractor *se);
