#include "scheduled_time.h"
#include <string.h>
#include <stdlib.h>

GQuark
scheduled_time_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("scheduled-time-error-quark");
  return error_quark;
}

void
scheduled_time_init(ScheduledTime *st)
{
  memset(st, 0, sizeof(ScheduledTime));
}

#define CHECK_BIT(s,b) (s[(b) / 8] & (1<<(b%8)))


static gboolean
find_minute(const ScheduledTime *st, gint *minute)
{
  while(TRUE) {
    if (CHECK_BIT(st->minute, *minute)) {
       return TRUE;
    }
    if (*minute == 59) return FALSE;
    (*minute)++;
  }
}

static gboolean
find_hour(const ScheduledTime *st, gint *hour, gint *minute)
{
  while(TRUE) {
    if (CHECK_BIT(st->hour, *hour)) {
      if (find_minute(st, minute)) return TRUE;
    }
    if (*hour == 23) return FALSE;
    (*hour)++;
    *minute = 0;
  }
}

static gboolean
find_day(const ScheduledTime *st, GDate *date, gint *hour, gint *minute)
{
  while(TRUE) {
    gint day = g_date_get_day(date);
    if (CHECK_BIT(st->dayofmonth, day)) {
      gint dow = g_date_get_weekday(date);
      if (CHECK_BIT(st->dayofweek, dow)) {
	if (find_hour(st, hour, minute)) return TRUE;
      }
    }
    day++;
    if (day > 28 /* Just to speed things up in most cases */
	&& day > g_date_get_days_in_month(g_date_get_month(date),
					  g_date_get_year(date))) {
      return FALSE;
    }
    g_date_set_day(date, day);
    *hour = 0;
    *minute = 0;
  }
}

static gboolean
find_month(const ScheduledTime *st, GDate *date, gint *hour, gint *minute)
{
  while(TRUE) {
    gint month = g_date_get_month(date);
    if (CHECK_BIT(st->month, month)) {
      if (find_day(st, date, hour, minute)) return TRUE;
    }
    g_date_set_day(date, 1);
    *hour = 0;
    *minute = 0;
    g_date_add_months(date, 1);
  }
  return FALSE;
}
gint64
scheduled_time_next_match(const ScheduledTime *st, gint64 from)
{
  gint hour;
  gint minute;
  gint64 next;
  GDateTime *dt;
  GDateTime *next_dt;
  GDate date;
  g_date_clear(&date,1);
  dt = g_date_time_new_from_unix_local (from/G_TIME_SPAN_SECOND);
  g_date_set_dmy(&date, g_date_time_get_day_of_month(dt),
		 g_date_time_get_month(dt), g_date_time_get_year(dt));
  hour = g_date_time_get_hour(dt);
  minute = g_date_time_get_minute(dt);
  g_date_time_unref(dt);
  find_month(st, &date, &hour, &minute);
  next_dt = g_date_time_new_local(g_date_get_year(&date),
				  g_date_get_month(&date),
				  g_date_get_day(&date),
				  hour, minute, 0.0);
  next = g_date_time_to_unix(next_dt);
  g_date_time_unref(next_dt);
  return next * G_TIME_SPAN_SECOND;
}

static gboolean
parse_set(const gchar **pp, guint8 *set, guint min_bit, guint max_bit,
	  GError **err)
{
  char *end;
  gint int_start;
  gint int_end;
  gint every;
  guint i;
  const gchar *p = *pp;
  while(g_ascii_isspace(*p)) p++;
  if (*p == '\0') {
    g_set_error(err, SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		"Missing field");
    return FALSE;
  }
  while(*p != '\0' && !g_ascii_isspace(*p)) {
    every = 1;
    if (*p == '*') {
      p++;
      int_start = min_bit;
      int_end = max_bit;
    } else {
      int_start = strtoul(p, &end, 10);
      if (p == end) {
	g_set_error(err, SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		    "Expected a number (interval start), got '%c'", *p);
	return FALSE;
      }
      int_end = int_start;
      p = end;
      if (*p == '-') {
	p++;
	if (*p == '\0' || g_ascii_isspace(*p)) {
	  g_set_error(err,
		      SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		      "Unexpected end of field");
	  return FALSE;
	}
	int_end = strtoul(p, &end, 10);
	if (p == end) {
	  g_set_error(err,
		      SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		      "Expected a number (interval end), got '%c'", *p);
	  return FALSE;
	}
	p = end;
      }
    }
    if (*p == '/') {
      p++;
      if (*p == '\0' || g_ascii_isspace(*p)) {
	g_set_error(err,
		    SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		    "Unexpected end of field");
	return FALSE;
      }
      every = strtoul(p, &end, 10);
      if (p == end) {
	g_set_error(err,
		    SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		    "Expected a number (every), got '%c'", *p);
	    return FALSE;
      }
      p = end;
    }
    if (*p != ',' && *p != '\0' && !g_ascii_isspace(*p)) {
      if (*p == '\0') {
	g_set_error(err,
		    SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_SYNTAX_ERROR,
		    "Expected comma or end of field");
	return FALSE;
      }
    }
    if (*p == ',') p++;
    if (int_end < int_start) {
      g_set_error(err,
		  SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_RANGE,
		  "Invalid range %d > %d", int_start, int_end);
      return FALSE;
    }
    if (int_end >max_bit || int_start < min_bit) {
        g_set_error(err,
		    SCHEDULED_TIME_ERROR, SCHEDULED_TIME_ERROR_RANGE,
		    "Out of range");
	return FALSE;
    }
    for (i = int_start; i <= int_end; i++) {
      if (i % every == 0) {
	set[i/8] |= 1<<(i%8);
      }
    }
  }
  *pp = p;
  return TRUE;
}

gboolean
scheduled_time_parse(ScheduledTime *st, const gchar *p, GError **err)
{
  if (!parse_set(&p, st->minute, 0, 59,err)) return FALSE;
  if (!parse_set(&p, st->hour, 0, 23,err)) return FALSE;
  if (!parse_set(&p, st->dayofmonth, 1, 31,err)) return FALSE;
  if (!parse_set(&p, st->month, 1, 12,err)) return FALSE;
  if (!parse_set(&p, st->dayofweek, 0, 7,err)) return FALSE;
  return TRUE;
}
