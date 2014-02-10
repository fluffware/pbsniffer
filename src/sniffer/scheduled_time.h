#include <glib.h>

#define SCHEDULED_TIME_ERROR (scheduled_time_error_quark())

enum {
  SCHEDULED_TIME_ERROR_OK = 0,
  SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
  SCHEDULED_TIME_ERROR_RANGE
};

#define BITTOBYTE(x) (((x)+7)/7)

typedef struct _ScheduledTime ScheduledTime;

struct _ScheduledTime
{
  guint8 minute[BITTOBYTE(60)];
  guint8 hour[BITTOBYTE(24)];
  guint8 dayofmonth[BITTOBYTE(1+31)];
  guint8 month[BITTOBYTE(1+12)];
  guint8 dayofweek[BITTOBYTE(1+7)];
};

void
scheduled_time_init(ScheduledTime *st);

gint64
scheduled_time_next_match(const ScheduledTime *st, gint64 from);

gboolean
scheduled_time_parse(ScheduledTime *st, const gchar *pattern, GError **err);
