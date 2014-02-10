#include "pbframer.h"
#include <string.h>

#define BUFFER_CAPACITY 1024
#define MAX_PACKET_LEN 256


GQuark
pb_framer_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("pb-framer-error-quark");
  return error_quark;
}
enum {
  PACKETS_QUEUED,
  LAST_SIGNAL
};

static guint pb_framer_signals[LAST_SIGNAL] = {0 };

struct _PBFramer
{
  GObject parent_instance;
  GInputStream *input;
  GAsyncQueue *queue;
  GThread *thread;
  GCancellable *cancel;
  guint8 *input_buffer;
  gssize input_buffer_capacity;
  guint8 *packet_start;
  gsize packet_len;
  /* Time stamp corresponing to the beginning of input_buffer */
  gint64 input_buffer_ts;
  /* Number of microsecconds corresponding to one byte */
  glong byte_period;
  gulong lost_packets;
  gint max_queue; /* Max queue length before starting to drop packets */
  gint min_queue; /* Don't signal the user until the queue has got this many
		     entries or it hasn't been checked for a second. */
  
  GMainContext *creator_context;
  GSource *idle_signal;
};

struct _PBFramerClass
{
  GObjectClass parent_class;
  /* Signals */
  void (*packets_queued)(PBFramer *framer, GAsyncQueue *queue);
};

G_DEFINE_TYPE (PBFramer, pb_framer, G_TYPE_OBJECT)

static void
finalize(GObject *object)
{
  PBFramer *framer = PB_FRAMER(object);
  if (framer->cancel) {
    g_cancellable_cancel(framer->cancel);
  }
  if (framer->thread) {
    g_thread_join(framer->thread);
    framer->thread = NULL;
  }
  if (framer->idle_signal) {
    g_source_destroy(framer->idle_signal);
    g_source_unref(framer->idle_signal);
    framer->idle_signal = NULL;
  }
  if (framer->creator_context) {
    g_main_context_unref(framer->creator_context);
    framer->creator_context = NULL;
  }
  g_clear_object(&framer->cancel);
  g_clear_object(&framer->input);
  if (framer->queue) {
    g_async_queue_unref(framer->queue);
    framer->queue = NULL;
  }
  g_free(framer->input_buffer);
  framer->input_buffer = NULL;
  G_OBJECT_CLASS(pb_framer_parent_class)->finalize(object);
}
static void
dispose(GObject *object)
{
  /* PBFramer *framer = PB_FRAMER(object); */
  G_OBJECT_CLASS(pb_framer_parent_class)->dispose(object);
}

static void
pb_framer_class_init (PBFramerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = finalize;
  gobject_class->dispose = dispose;

  /*Signals */
  pb_framer_signals[PACKETS_QUEUED] =
    g_signal_new("packets-queued",
		 G_OBJECT_CLASS_TYPE (gobject_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(PBFramerClass, packets_queued),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
pb_framer_init (PBFramer *framer)
{
  framer->input = NULL;
  framer->queue = NULL;
  framer->thread = NULL;
  framer->cancel = g_cancellable_new();
  framer->input_buffer_capacity = 0;
  framer->input_buffer = NULL;
  framer->packet_len = 0;
  framer->byte_period = 0;
  framer->lost_packets = 0;
  framer->max_queue = 1000;
  framer->min_queue = 10;
  framer->creator_context = NULL;
  framer->idle_signal = NULL;
}

#define SD1 0x10
#define SD2 0x68
#define SD3 0xa2
#define SD4 0xdc
#define ED 0x16
#define SC 0xe5


typedef struct _QueueSource QueueSource;
struct _QueueSource
{
  GSource source;
  PBFramer *framer;
};
  
static gboolean
queue_prepare(GSource    *source, gint       *timeout_)
{
  QueueSource *qs = (QueueSource*)source;
  *timeout_ = 1000; /* Check the queue once a second in case
		       it's shorter than min_queue */
  return g_async_queue_length(qs->framer->queue) > 0;
}

static gboolean
queue_check(GSource    *source)
{
  QueueSource *qs = (QueueSource*)source;
  return g_async_queue_length(qs->framer->queue) > 0;
}

static gboolean
queue_dispatch(GSource    *source,
	       GSourceFunc callback,
	       gpointer    user_data)
{
  QueueSource *qs = (QueueSource*)source;
  PBFramer *framer = qs->framer;
  g_signal_emit(framer, pb_framer_signals[PACKETS_QUEUED],
		0, framer->queue);
  return TRUE;
}

static void
queue_finalize(GSource    *source)
{
}

GSourceFuncs queue_funcs =
  {
    queue_prepare,
    queue_check,
    queue_dispatch,
    queue_finalize
  };

static void
packet_done(PBFramer *framer)
{
  if (g_async_queue_length(framer->queue) <= framer->max_queue) {
    PBFramerPacket *packet;
    gint64 ts = (framer->input_buffer_ts
	       + framer->byte_period * (framer->packet_start
					- framer->input_buffer));
    packet = g_new(PBFramerPacket, 1);
    packet->data = g_new(guint8, framer->packet_len);
    memcpy(packet->data, framer->packet_start, framer->packet_len);
    packet->timestamp = ts;
    packet->length =  framer->packet_len;
    g_async_queue_push(framer->queue, packet);
    if (framer->lost_packets > 0) {
      g_warning("Lost %ld packets", framer->lost_packets);
      framer->lost_packets = 0;
    }
   
  } else {
    if (framer->lost_packets == 0) {
      g_warning("Queue overflow");
    }
    framer->lost_packets++;
  }
  if (g_async_queue_length(framer->queue) >= framer->min_queue) {
    g_main_context_wakeup (framer->creator_context);
  }
  framer->packet_start += framer->packet_len;
  framer->packet_len = 0;
}

static guint8
checksum(const guint8 *bytes, gsize len)
{
  guint8 sum = 0;
  while(len-- > 0) {
    sum += *bytes++;
  }
  return sum;
}
/* Returns true if more data is needed */
static inline gboolean
grow_packet(gsize *packet_len, gsize *data_left, gsize new_length)
{
  gsize needed = new_length - *packet_len;
  if (needed > *data_left) {
    *packet_len += *data_left;
    return TRUE;
  }
  *data_left -= needed;
  *packet_len = new_length;
  return FALSE;
}

gboolean
parse_data(PBFramer *framer, gsize len, GError **err)
{
  guint8 ptype;
  while(TRUE) {
    guint8 cs_calc;
    guint8 cs_packet;
    guint8 *pstart = framer->packet_start;
    if (framer->packet_len == 0) {
      if (grow_packet(&framer->packet_len, &len, 1)) return TRUE;
      switch(framer->packet_start[0]) {
      case SC:
      case SD1:
      case SD2:
      case SD3:
      case SD4:
	break;
      default:
	g_set_error(err, PB_FRAMER_ERROR, PB_FRAMER_ERROR_INVALID_START,
		    "Invalid start symbol: 0x%02x",
		    framer->packet_start[0]);
	return FALSE;
      }
    }
    ptype = pstart[0];
    if (ptype == SC) {
      packet_done(framer);
      continue;
    }
    if (framer->packet_len < 3) {
      if (grow_packet(&framer->packet_len, &len, 3)) return TRUE;
    }
    if (ptype == SD4) {
      packet_done(framer);
      continue;
    }
    if (ptype == SD2 &&  pstart[1] != pstart[2]) {
      g_set_error(err, PB_FRAMER_ERROR, PB_FRAMER_ERROR_MISMATCH_LENGTH,
		  "Length bytes doesn't match: 0x%02x != 0x%02x",
		  pstart[1], pstart[2]);
      return FALSE;
    }
    if (framer->packet_len < 6) {
      if (grow_packet(&framer->packet_len, &len, 6)) return TRUE;
    }
    
    if (ptype == SD2) {
      guint8 dlen;
      if (pstart[3] != SD2) {
	g_set_error(err, PB_FRAMER_ERROR, PB_FRAMER_ERROR_SECOND_SD2,
		    "Fourth byte of SD2 packet is not SD2, but 0x%02x",
		    pstart[3]);
	framer->packet_len = 3;
	return FALSE;
      }
      dlen =  pstart[1];
      if (framer->packet_len < dlen + 6) {
	if (grow_packet(&framer->packet_len, &len, dlen + 6)) return TRUE;
      }
    } else if (ptype == SD3) {
      if (framer->packet_len < 14) {
	if (grow_packet(&framer->packet_len, &len, 14)) return TRUE;
      }
    }
    if (pstart[framer->packet_len-1] != ED) {
      g_set_error(err, PB_FRAMER_ERROR, PB_FRAMER_ERROR_NO_ED,
		  "Packet doesn't end with ED, but 0x%02x",
		  pstart[framer->packet_len-1]);
      return FALSE;
    }

    switch(ptype) {
    case SD1:
      cs_calc = checksum(&pstart[1], 3);
      cs_packet = pstart[4];
      break;
    case SD2:
      cs_calc = checksum(&pstart[4], pstart[1]);
      cs_packet =  pstart[pstart[1]+4];
      break;
    case SD3:
      cs_calc = checksum(&pstart[1], 11);
      cs_packet =  pstart[12];
      break;
    }
    if (cs_calc != cs_packet) {
      g_set_error(err, PB_FRAMER_ERROR, PB_FRAMER_ERROR_CHECKSUM,
		  "Checksum error, got 0x%02x, expected 0x%02x",
		  cs_packet, cs_calc);
      return FALSE;
    }
    packet_done(framer);
  }
}

static gpointer
framer_thread(gpointer data)
{
  GError *err = NULL;
  PBFramer *framer = PB_FRAMER(data);
  while(TRUE) {
    gssize r;
    guint8 *read_start;
    gssize read_len;
    framer->packet_start = framer->input_buffer;
    read_start = framer->packet_start + framer->packet_len;
    read_len = framer->input_buffer_capacity - framer->packet_len;
    r = g_input_stream_read(framer->input, read_start, read_len,
			    framer->cancel, &err);
    framer->input_buffer_ts = g_get_real_time();
    
    if (r == 0) {
      g_warning("Reached EOF");
      break;
    }
    if (r < 0) {
      if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
	break;
      }
      g_warning("Failed to read raw PROFIBUS data: %s", err->message);
      g_clear_error(&err);
    } else {
      guint8 *read_end;
      GError *err = NULL;
      gsize len = r;
      /* Adjust the time stamp to the beginning of the input buffer */
      framer->input_buffer_ts -=
	framer->byte_period * (framer->packet_len + r);
      read_end = r + read_start;
      while (!parse_data(framer, len, &err)) {
	g_warning("Failed to parse packet: %s", err->message);
	g_clear_error(&err);
	/* Skip invalid bytes */
	framer->packet_start += framer->packet_len;
	framer->packet_len = 0;
	len = read_end - framer->packet_start;
      }
      if (framer->packet_len > 0) {
	/* Move partial packet to beginning of buffer */
	memmove(framer->input_buffer, framer->packet_start, framer->packet_len);
      }
    }
  }
  return NULL;
}

PBFramer *
pb_framer_new(GInputStream *input, GAsyncQueue *queue)
{
  PBFramer *framer;
  g_type_init();
  framer = g_object_new(PB_FRAMER_TYPE, NULL);
  framer->input = input;
  g_object_ref(input);
  framer->queue = queue;
  g_async_queue_ref(queue);
  framer->creator_context = g_main_context_ref_thread_default();
  
  framer->idle_signal = g_source_new(&queue_funcs, sizeof(QueueSource));
  ((QueueSource*)framer->idle_signal)->framer = framer;
  g_source_attach(framer->idle_signal, framer->creator_context);
  
  framer->input_buffer_capacity = BUFFER_CAPACITY;
  framer->input_buffer = g_new(guint8, framer->input_buffer_capacity);
  framer->thread = g_thread_new("PROFIBUS framer", framer_thread, framer);
  return framer;
}

void
pb_framer_packet_free(PBFramerPacket *packet)
{
  g_free(packet->data);
  g_free(packet);
}
