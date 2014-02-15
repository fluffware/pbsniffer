#include "pbfilter.h"
#include <string.h>


GQuark
pb_filter_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("pb-filter-error-quark");
  return error_quark;
}

enum
{
  PROP_0 = 0,
  N_PROPERTIES
};

#define NO_SAP 0xff
#define NO_KEY 0xffffffff
#define KEY(src, ssap, dst, dsap) \
  (((src) << 24) | ((dst) << 16) | ((ssap) << 8) | (dsap))

#define KEY_ADDRESS_EQ(key1, key2) ((((key1) ^ (key2)) & 0xffff0000) == 0)
#define KEY_NO_SAP(key) (((key) &0x0000ffff) == 0x0000ffff)
typedef struct _Transaction Transaction;
struct _Transaction
{
  guint8 *request;
  gsize request_len;
  gint64 request_ts;
  guint8 *response;
  gsize response_len;
  gint64 response_ts;
};
  
struct _PBFilter
{
  GObject parent_instance;
  GTree *transactions;
  PBFilterOutput output;
  gpointer *output_user_data;
  guint32 last_req_key; /* Key of previous request */
  gboolean last_req_output; /* The last request  was written to output */
};

struct _PBFilterClass
{
  GObjectClass parent_class;
  /* Signals */
};

G_DEFINE_TYPE (PBFilter, pb_filter, G_TYPE_OBJECT)

static void
transaction_free(Transaction *trans)
{
  g_free(trans->request);
  g_free(trans->response);
  g_free(trans);
}

static void
finalize(GObject *object)
{
  PBFilter *filter = PB_FILTER(object);
  if (filter->transactions) {
    g_tree_unref(filter->transactions);
    filter->transactions = NULL;
  }
  G_OBJECT_CLASS(pb_filter_parent_class)->finalize(object);
}
static void
dispose(GObject *object)
{
  /* PBFilter *filter = PB_FILTER(object); */
  G_OBJECT_CLASS(pb_filter_parent_class)->dispose(object);
}

static void
set_property (GObject *object, guint property_id,
	      const GValue *value, GParamSpec *pspec)
{
  /* PBFilter *filter = PB_FILTER(object); */
  switch (property_id)
    {
   default:
       /* We don't have any other property... */
       G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
get_property (GObject *object, guint property_id,
	      GValue *value, GParamSpec *pspec)
{
  /* PBFilter *filter = PB_FILTER(object); */
  switch (property_id) {
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
pb_filter_class_init (PBFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = finalize;
  gobject_class->dispose = dispose;
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

}

static gint
key_cmp(gconstpointer a, gconstpointer b, gpointer ud)
{
  return GPOINTER_TO_SIZE(a) - GPOINTER_TO_SIZE(b);
}

static void
pb_filter_init (PBFilter *filter)
{
  filter->transactions = g_tree_new_full(key_cmp, NULL, NULL,
					 (GDestroyNotify)transaction_free);
  filter->output = NULL;
  filter->last_req_key = NO_KEY;
  filter->last_req_output = FALSE;
}

#define SD1 0x10
#define SD2 0x68
#define SD3 0xa2
#define SD4 0xdc
#define ED 0x16
#define SC 0xe5



PBFilter *
pb_filter_new(void)
{
  PBFilter *filter;
  filter = g_object_new(PB_FILTER_TYPE, NULL);
  return filter;
}

static guint32
get_key(const guint8 *data, gsize length)
{
  guint8 fc;
  guint8 sa;
  guint8 da;
  guint8 ssap = NO_SAP;
  guint8 dsap = NO_SAP;
  const guint8 *end = data + length;
  if (length < 3) return NO_KEY;
  sa = data[2];
  da = data[1];
  fc = data[3];
  data += 4;
  /* Get DSAP */
  if (da & 0x80) {
    if (data >= end) return NO_KEY;
    dsap = *data++;
    if ((dsap & 0xc0) == 0xc0) {
      if (data >= end) return NO_KEY;
      dsap = *data++;
    }
    dsap &= 0x3f;
  }
  /* Get SSAP */
  if (sa & 0x80) {
    if (data >= end) return NO_KEY;
    ssap = *data++;
    if ((ssap & 0xc0) == 0xc0) {
      if (data >= end) return NO_KEY;
      ssap = *data++;
    }
    ssap &= 0x3f;
  }
  if (fc & 0x40) {
    if (ssap != NO_SAP || dsap != NO_SAP) {
      return KEY(sa & 0x7f, ssap, da & 0x7f, dsap);
    } else {
      return KEY(sa & 0x7f, (0x80 | (fc & 0x0f)), da & 0x7f,NO_SAP);
    }
  } else {
    /* Revers fields for response so the key matches the key of the request */
    return KEY(da & 0x7f, dsap, sa & 0x7f, ssap);
  }
}

/* Assumes a SD1,SD2(truncated) or SD3 packet */
static gboolean
packets_equal(gsize a_len, const guint8 *a_data,
	      gsize b_len,const guint8 *b_data)
{
  if (a_len != b_len) return FALSE;
  /* Only use a_len from now on */
  
  /* Check type */
  if (a_data[0] != b_data[0]) return FALSE;
  if (a_data[0] == SD2) {
    a_data += 3;
    b_data += 3;
    a_len -= 3;
  }
  /* Check that addresses are the same */
  if (a_data[1] != b_data[1] || a_data[2] != b_data[2]) return FALSE;
  /* Check function code */
  if (a_data[3] & 0x40) {
    /* Ignore alternating bit in request */
    if (((a_data[3] ^ b_data[3]) & 0xcf) != 0) return FALSE;
  } else {
    if (a_data[3] != b_data[3]) return FALSE;
  }
  /* Check PDU */
  if (memcmp(&a_data[4], &b_data[4], a_len - 6) != 0) return FALSE;
  return TRUE;
}

static Transaction *
create_transaction(gsize length, const guint8 *data,gint64 ts)
{
  Transaction *trans = g_new(Transaction,1);
  trans->request_len = length;
  trans->request = g_new(guint8, length);
  memcpy(trans->request, data, length);
  trans->request_ts = ts;
  trans->response = NULL;
  trans->response_len = 0;
  return trans;
}

/* Returns TRUE if the request differed from previous */
static gboolean
set_request(Transaction *trans, gsize length, const guint8 *data,gint64 ts)
{
  if (!packets_equal(length, data, trans->request_len, trans->request)) {
    if (length != trans->request_len) {
      g_free(trans->request);
      trans->request = g_new(guint8, length);
      trans->request_len = length;
    }
    memcpy(trans->request, data, length);
    trans->request_ts = ts;
    return TRUE;
  } else {
    /* In case the alternating bit switched */
    guint8 offset = (data[0] == SD2) ? 6: 3;
    trans->request[offset] = data[offset];
    trans->request[length - 2] = data[length - 2];
    trans->request_ts = ts;
    return FALSE;
  }
}


/* Returns TRUE if the response differed from previous */
static gboolean
set_response(Transaction *trans, gsize length, const guint8 *data,gint64 ts)
{
  if (!packets_equal(length, data, trans->response_len, trans->response)) {
    if (length != trans->response_len) {
      g_free(trans->response);
      trans->response = g_new(guint8, length);
      trans->response_len = length;
    }
    memcpy(trans->response, data, length);
    trans->response_ts = ts;
    return TRUE;
  } else {
    trans->response_ts = ts;
    return FALSE;
  }
}

static void
output_request(PBFilter *filter, Transaction *trans)
{
  if (filter->output) {
    filter->output(trans->request_len, trans->request, trans->request_ts,
		   filter->output_user_data);
  }
  filter->last_req_output = TRUE;
}
static void
output_response(PBFilter *filter, Transaction *trans)
{
  if (filter->output) {
    filter->output(trans->response_len, trans->response, trans->response_ts,
		   filter->output_user_data);
  }
  filter->last_req_output = FALSE;
}




void
pb_filter_input(PBFilter *filter, gsize length, const guint8 *data, gint64 ts)
{
  guint offset = 0;
  switch(data[0]) {
  case SD4:
    /* Ignore tokens */
    return;
  case SC:
    if (filter->last_req_key != NO_KEY) {
      Transaction *trans;
      trans = g_tree_lookup(filter->transactions,
			    GSIZE_TO_POINTER(filter->last_req_key));
      if (trans) {
	trans->response_ts = ts;
	if (!trans->response || trans->response[0] != SC) {
	  g_free(trans->response);
	  trans->response = g_new(guint8, 1);
	  trans->response[0] = SC;
	  trans->response_len = 1;
	  if (!filter->last_req_output) {
	    output_request(filter, trans);
	  }
	  output_response(filter, trans);
	} else {
	  if (filter->last_req_output) {
	    output_response(filter, trans);
	  }
      }
    }
    filter->last_req_output = FALSE;
    filter->last_req_key = NO_KEY;
    return;
  case SD2:
    /* Skip the first 3 bytes since we keep track of the length elsewhere.
       This way SD1,SD2 and SD3 has the same structure */
    offset = 3;
  case SD1:
  case SD3:
    {
      guint32 key = get_key(data + offset, length - offset);
      Transaction *trans = NULL;

      if (key == NO_KEY) return;
      /* If a response without SAP matches the previous requests addresses
	 then use assume that the request matches the response */
      if (KEY_NO_SAP(key) && KEY_ADDRESS_EQ(key, filter->last_req_key)) {
	key = filter->last_req_key;
      }
      trans = g_tree_lookup(filter->transactions, GSIZE_TO_POINTER(key));
      
      if (data[3 + offset] & 0x40) {
	/* Request */
	filter->last_req_output = FALSE;
	if (!trans) {
	  trans = create_transaction(length, data, ts);
	  g_tree_insert(filter->transactions, GSIZE_TO_POINTER(key), trans);
	  output_request(filter,trans);
	} else {
	  if ((set_request(trans, length, data, ts))) {
	    output_request(filter,trans); 
	  }
	}
	filter->last_req_key = key;
      } else {
	/* Response */

	/* Make sure the response matches the request */
	if (key != filter->last_req_key) {
	  filter->last_req_key = NO_KEY;
	  return;
	}
	filter->last_req_key = NO_KEY;
	if (!trans) return;
	if ((set_response(trans, length, data, ts))) {
	  if (!filter->last_req_output) {
	    output_request(filter, trans);
	  }
	  output_response(filter, trans);
	} else {
	  if (filter->last_req_output) {
	    output_response(filter, trans);
	  }
	}
      }
    }
    break;
    }
  }
    
}

void
pb_filter_set_output(PBFilter *filter, PBFilterOutput cb, gpointer user_data)
{
  filter ->output = cb;
  filter->output_user_data = user_data;
}

void
pb_filter_output_state(PBFilter *filter)
{
  g_warning("pb_filter_output_state: Not implemented");
}
