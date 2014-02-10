#ifndef __PBFRAMER_H__BFK34KHU4U__
#define __PBFRAMER_H__BFK34KHU4U__
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#define PB_FRAMER_ERROR (pb_framer_error_quark())

enum {
  PB_FRAMER_ERROR_OK = 0,
  PB_FRAMER_ERROR_INVALID_START,
  PB_FRAMER_ERROR_MISMATCH_LENGTH,
  PB_FRAMER_ERROR_SECOND_SD2,
  PB_FRAMER_ERROR_NO_ED,
  PB_FRAMER_ERROR_CHECKSUM
};

#define PB_FRAMER_TYPE                  (pb_framer_get_type ())
#define PB_FRAMER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PB_FRAMER_TYPE, PBFramer))
#define PB_FRAMER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PB_FRAMER_TYPE, PBFramerClass))
#define IS_PB_FRAMER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PB_FRAMER_TYPE))
#define IS_PB_FRAMER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PB_FRAMER_TYPE))
#define PB_FRAMER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PB_FRAMER_TYPE, PBFramerClass))

typedef struct _PBFramer PBFramer;
typedef struct _PBFramerClass PBFramerClass;

typedef struct _PBFramerPacket PBFramerPacket;
struct _PBFramerPacket
{
  gint64 timestamp; /* Microseconds since January 1, 1970 UTC */
  gsize length; /* In bytes */
  guint8 *data;
};
  

PBFramer *
pb_framer_new(GInputStream *input, GAsyncQueue *queue);

void
pb_framer_packet_free(PBFramerPacket *packet);

#endif /* __PBFRAMER_H__BFK34KHU4U__ */
