#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <scheduled_time.h>

static void
print_set(const guint8 *set, guint len)
{
  guint i;
  for (i = 0; i < len; i++) {
    fputc((set[i/8] & 1<<(i%8))?'1':'0',stdout);
    if (i % 10 == 9) fputc(' ', stdout);
  }
}

void
print_st(ScheduledTime *st)
{
  fputs("Minutes: ", stdout);
  print_set(st->minute, 60);
  fputs("\nHours: ", stdout);
  print_set(st->hour, 24);
  fputs("\nDay of month: ", stdout);
  print_set(st->dayofmonth, 32);
  fputs("\nMonth: ", stdout);
  print_set(st->month, 13);
  fputs("\nDay of week: ", stdout);
  print_set(st->dayofweek, 8);
  fputs("\n", stdout);
}

int
main(int argc, char *argv[])
{
  gchar *next_str;
  GDateTime *next_dt;
  gint64 now;
  gint64 next;
  GError *err = NULL;
  ScheduledTime st;
  scheduled_time_init(&st);
  if (!scheduled_time_parse(&st, argv[1], &err)) {
    g_printerr("Failed to parse time: %s\n", err->message);
    return EXIT_FAILURE;
  }
  print_st(&st);

  now = g_get_real_time();
  while(TRUE) {
    next = scheduled_time_next_match(&st, now);
    next_dt = g_date_time_new_from_unix_local(next/G_TIME_SPAN_SECOND);
    next_str = g_date_time_format(next_dt, "%Y-%m-%d %H:%M");
    g_date_time_unref(next_dt);
    printf("%lld: %s\n", next, next_str);
    g_free(next_str);
    now = next +  G_TIME_SPAN_MINUTE *3 /2;
  }
  return EXIT_SUCCESS;
}
