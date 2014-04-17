#include "signal_extractor.h"
#include <string.h>
#include <stdlib.h>

GQuark
signal_extractor_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("signal-extractor-error-quark");
  return error_quark;
}

#define MAKE_KEY(src,dst) GSIZE_TO_POINTER(((src)<<16) | (dst))

static gint
trace_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
  return GPOINTER_TO_SIZE(a) - GPOINTER_TO_SIZE(b);
}

static void 
destroy_signal(SignalFilter *signal)
{
  g_free(signal->id);
  g_free(signal->label);
  g_free(signal);
}

static void 
destroy_trace(PacketTrace *trace)
{
  SignalFilter *s = trace->signals;
  while(s) {
    SignalFilter *next = s->next;
    destroy_signal(s);
    s = next;
  }
  g_free(trace);
}

void
signal_extractor_destroy(SignalExtractor *se)
{
  g_tree_unref(se->traces);
  g_free(se);
}

static struct {
  GQuark signal_list;
  GQuark trace;
  GQuark signal;
  GQuark label;
  GQuark mask;
  GQuark signed_;
  GQuark unsigned_;
} names;

static void
init_names(void)
{
  names.signal_list = g_quark_from_static_string("signal-list");
  names.signal = g_quark_from_static_string("signal");
  names.trace = g_quark_from_static_string("trace");
  names.label = g_quark_from_static_string("label");
  names.mask = g_quark_from_static_string("mask");
  names.signed_ = g_quark_from_static_string("signed");
  names.unsigned_ = g_quark_from_static_string("unsigned");
}

SignalExtractor *
signal_extractor_new(void)
{
  SignalExtractor *se = g_new(SignalExtractor,1);
  se->traces = g_tree_new_full(trace_cmp, NULL, NULL,
			       (GDestroyNotify)destroy_trace);
  init_names();
  return se;
}

typedef struct ParseState ParseState;
struct ParseState
{
  SignalExtractor *signal_extractor;
  GString *text;
  SignalFilter *signal;
  PacketTrace *trace;
};



static void
signal_start(GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     gpointer             user_data,
	     GError             **error)
{
  ParseState *state = user_data;
  GQuark elemq = g_quark_try_string(element_name);
  if (elemq == names.label) {
    if (state->signal->label) {
      g_set_error(error, SIGNAL_EXTRACTOR_ERROR,
		  SIGNAL_EXTRACTOR_ERROR_MULTIPLE_ELEMENTS,
		  "Multiple label ellements for one signal");
      return;
    }
    state->text = g_string_new("");
  } else if (elemq == names.mask) {
    const gchar *offsetstr;
    const gchar *widthstr;
    SignalFilter *signal = state->signal;
    char *end;
    if (!g_markup_collect_attributes(element_name,
				     attribute_names, attribute_values,
				     error,
				     G_MARKUP_COLLECT_STRING, "offset",
				     &offsetstr,
				     G_MARKUP_COLLECT_STRING, "width",
				     &widthstr,
				     NULL)) {
      return;
    }
    signal->bit_offset = strtoul(offsetstr, &end, 0);
    if (*end != '\0') {
      g_set_error(error, SIGNAL_EXTRACTOR_ERROR,
		  SIGNAL_EXTRACTOR_ERROR_INVALID_VALUE,
		  "Invalid source address");
      return;
    }
    signal->bit_width = strtoul(widthstr, &end, 0);
    if (*end != '\0') {
      g_set_error(error, SIGNAL_EXTRACTOR_ERROR,
		  SIGNAL_EXTRACTOR_ERROR_INVALID_VALUE,
		  "Invalid destination address");
      return;
    }
  } else if (elemq == names.signed_) {
    state->signal->type = SIGNAL_TYPE_SIGNED_INT;
  } else if (elemq == names.unsigned_) {
    state->signal->type = SIGNAL_TYPE_UNSIGNED_INT;
  } else {
    g_set_error(error,  G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		"Got element %s, expected label, mask or unsigned",
		element_name);
    return;
  }
}

static void
signal_end(GMarkupParseContext *context,
		const gchar         *element_name,
		gpointer             user_data,
		GError             **error)
{
  ParseState *state = user_data;
  GQuark elemq = g_quark_try_string(element_name);
  if (elemq == names.label) {
    
    state->signal->label = g_string_free(state->text, FALSE);
    state->text = NULL;
  }
}

static const GMarkupParser signal_parser;

static void
trace_start(GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     gpointer             user_data,
	     GError             **error)
{
  ParseState *state = user_data;
  GQuark elemq = g_quark_try_string(element_name);
  if (elemq == names.signal) {
    gchar *id;
    if (!g_markup_collect_attributes(element_name,
				     attribute_names, attribute_values,
				     error,
				     G_MARKUP_COLLECT_STRDUP, "id", &id,
				     NULL)) {
      return;
    }
    g_markup_parse_context_push(context, &signal_parser, state);
    g_assert(state->signal == NULL);
    state->signal = g_new(SignalFilter, 1);
    state->signal->id = id;
    state->signal->label = NULL;
    state->signal->bit_offset = -1;
    state->signal->bit_width = -1;
    state->signal->type = SIGNAL_TYPE_UNSIGNED_INT;
    state->signal->last_timestamp_ns = -1;
    g_assert(state->trace != NULL);
    state->signal->next = state->trace->signals;
    state->trace->signals = state->signal;
  } else {
    g_set_error(error,  G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		"Got element %s, expected trace", element_name);
    return;
  }

}

static void
trace_end(GMarkupParseContext *context,
		const gchar         *element_name,
		gpointer             user_data,
		GError             **error)
{
  ParseState *state = user_data;
  SignalFilter *signal = state->signal;
  g_markup_parse_context_pop(context);
  if (!signal->label) {
    signal->label = g_strdup(signal->id);
  }
  if (signal->bit_offset < 0) {
    g_set_error(error,  SIGNAL_EXTRACTOR_ERROR,
		SIGNAL_EXTRACTOR_ERROR_MISSING_PARAMETER,
		"Missing mask element");
    return;
  }
  state->signal = NULL;
}

static const GMarkupParser trace_parser;

static void
signal_list_start(GMarkupParseContext *context,
		  const gchar         *element_name,
		  const gchar        **attribute_names,
		  const gchar        **attribute_values,
		  gpointer             user_data,
		  GError             **error)
{
  ParseState *state = user_data;
  GQuark elemq = g_quark_try_string(element_name);
  if (elemq == names.trace) {
    const gchar *srcstr;
    const gchar *dststr;
    char *end;
    gint src;
    gint dst;
    if (!g_markup_collect_attributes(element_name,
				     attribute_names, attribute_values,
				     error,
				     G_MARKUP_COLLECT_STRING, "src", &srcstr,
				     G_MARKUP_COLLECT_STRING, "dst", &dststr,
				     NULL)) {
      return;
    }
    src = strtoul(srcstr, &end, 0);
    if (*end != '\0' || src >= 128) {
      g_set_error(error, SIGNAL_EXTRACTOR_ERROR,
		  SIGNAL_EXTRACTOR_ERROR_INVALID_VALUE,
		  "Invalid source address");
      return;
    }
    dst = strtoul(dststr, &end, 0);
    if (*end != '\0' || dst >= 128) {
      g_set_error(error, SIGNAL_EXTRACTOR_ERROR,
		  SIGNAL_EXTRACTOR_ERROR_INVALID_VALUE,
		  "Invalid destination address");
      return;
    }
    g_markup_parse_context_push(context, &trace_parser, state);
    g_assert(state->trace == NULL);
    state->trace = g_new(PacketTrace, 1);
    state->trace->src = src;
    state->trace->dst = dst;
    state->trace->signals = NULL;
    g_tree_insert(state->signal_extractor->traces,
		  MAKE_KEY(src,dst), state->trace);
  } else {
    g_set_error(error,  G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		"Got element %s, expected trace", element_name);
    return;
  }
}
static void
signal_list_end(GMarkupParseContext *context,
		const gchar         *element_name,
		gpointer             user_data,
		GError             **error)
{
  ParseState *state = user_data;
  state->trace = NULL;
  g_markup_parse_context_pop(context);
}

static const GMarkupParser signal_list_parser;

static void
document_start(GMarkupParseContext *context,
		  const gchar         *element_name,
		  const gchar        **attribute_names,
		  const gchar        **attribute_values,
		  gpointer             user_data,
		  GError             **error)
{
  ParseState *state = user_data;
  GQuark elemq = g_quark_try_string(element_name);
  if (elemq == names.signal_list) {
    g_markup_parse_context_push(context, &signal_list_parser, state);
  } else {
    g_set_error(error,  G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		"Got element %s, expected signal-list", element_name);
    return;
  }
}

static void
document_end(GMarkupParseContext *context,
	    const gchar         *element_name,
	    gpointer             user_data,
	    GError             **error)
{
  g_markup_parse_context_pop(context);
}

void parse_text(GMarkupParseContext *context,
		const gchar         *text,
		gsize                text_len,
		gpointer             user_data,
		GError             **error)
{
  ParseState *state = user_data;
  if (state->text) {
    g_string_append_len(state->text, text, text_len);
  }
}

static void
parse_error(GMarkupParseContext *context,
	    GError              *error,
	    gpointer             user_data)
{
}

static const GMarkupParser signal_parser =
  {
    signal_start,
    signal_end,
    parse_text, 
    NULL,
    parse_error
  };

static const GMarkupParser trace_parser =
  {
    trace_start,
    trace_end,
    NULL, 
    NULL,
    parse_error
  };

static const GMarkupParser signal_list_parser =
  {
    signal_list_start,
    signal_list_end,
    NULL, 
    NULL,
    parse_error
  };

gboolean
signal_extractor_config(SignalExtractor *se,
			GFile *file, GError **err)
{
  const GMarkupParser parser =
    {
      document_start,
      document_end,
      NULL,
      NULL,
      parse_error
    };
  gchar buffer[100];
  ParseState state;
  GMarkupParseContext *parse_ctxt;
  GInputStream *stream;
  gssize r;
  stream = (GInputStream*)g_file_read(file, NULL, err);
  if (!stream) {
    return FALSE;
  }
  state.signal_extractor = se;
  state.text = NULL;
  state.signal = NULL;
  state.trace = NULL;

  parse_ctxt =
    g_markup_parse_context_new(&parser,G_MARKUP_PREFIX_ERROR_POSITION, &state, NULL);
  while ((r = g_input_stream_read(stream, buffer, sizeof(buffer),
				  NULL, err)) > 0) {
    if (!g_markup_parse_context_parse(parse_ctxt, buffer, r, err)) {
      r= -1;
      break;
    }
  }
  g_input_stream_close(stream, NULL, NULL);
  g_markup_parse_context_free(parse_ctxt);
  if (r < 0 && state.text) {
    /* In case there's an error while collecting text */
    g_string_free(state.text, TRUE);
    state.text = NULL;
  }
  g_assert(state.text == NULL);
  if (r < 0) {
    return FALSE;
  }
  return TRUE;
}

void
signal_extractor_extract(SignalExtractor *se, guint src, guint dst,
			 gint64 timestamp,
			 const guint8 *block, gsize block_size,
			 SignalExtractorCallback callback, gpointer user_data)
{
  SignalFilter *s;
  PacketTrace *trace = g_tree_lookup(se->traces, MAKE_KEY(src,dst));
  if (!trace) return;
  
  for(s = trace->signals; s; s = s->next) {
    guint8 mask_start;
    guint byte_start;
    guint8 mask_last;
    guint8 byte_last;
    guint value;
    guint shift;
    gint bit_end = s->bit_offset + s->bit_width;
    if (bit_end > block_size * 8) {
      g_warning("Mask outside block");
    }
    byte_start = s->bit_offset / 8;
    shift = s->bit_offset % 8;
    mask_start = 0xff << shift;
    byte_last = (bit_end-1) / 8;
    mask_last = 0xff >> (7 - ((bit_end-1) % 8));
    if (byte_start ==  byte_last) {
      value = block[byte_start] & (mask_start & mask_last);
    } else {
      guint p;
      value = block[byte_start] & mask_start;
      for (p = byte_start + 1; p < byte_last; p++) {
	value = (value << 8) | block[p];
      }
      value = (value << 8) | (block[byte_last] & mask_last);
    }
    value >>= shift;
    if (s->type == SIGNAL_TYPE_SIGNED_INT
	&& (value & (1<<(s->bit_width-1)))) { /* High bit set */
      value |= (~0U<<(s->bit_width));
    }
    if (s->last_timestamp_ns >= 0) {
      if (value != s->last_value) {
	callback(user_data, s->id, s->label, s->last_timestamp_ns, timestamp, s->last_value);
	s->last_timestamp_ns = timestamp;
	s->last_value = value;
      }
    } else {
      /* No previous value */
      s->last_timestamp_ns = timestamp;
      s->last_value = value;
    }
  }
}

typedef struct ExtractionFinish ExtractionFinish;
struct ExtractionFinish
{
  gint64 timestamp_ns;
  SignalExtractorCallback callback;
  gpointer user_data;
};

static gboolean
finish_trace(gpointer key, gpointer value, gpointer data)
{
  ExtractionFinish *finish = data;
  PacketTrace *trace = value;
  SignalFilter *s;
  s = trace->signals;
  while(s) {
    if (s->last_timestamp_ns >= 0) {
      finish->callback(finish->user_data, s->id, s->label,
		       s->last_timestamp_ns, finish->timestamp_ns, s->last_value);
      s->last_timestamp_ns = -1;
    }
    s = s->next;
  }
  return FALSE;
}

/**
 * signal_extractor_finish:
 * @se: SignalExtractor object
 * @timestamp: End an active signals at this time stamp
 * @callback: called for all active signals
 * @user_data: for callback
 *
 * Run when all blocks has been processed.
 **/

void
signal_extractor_finish(SignalExtractor *se, 
			gint64 timestamp,
			SignalExtractorCallback callback, gpointer user_data)
{
  ExtractionFinish finish;
  finish.timestamp_ns = timestamp;
  finish.callback = callback;
  finish.user_data = user_data;
  g_tree_foreach(se->traces, finish_trace, &finish);
}


static gboolean
dump_trace(gpointer key, gpointer value, gpointer data)
{
  GString *out = data;
  PacketTrace *trace = value;
  SignalFilter *s;
  g_string_append_printf(out, "  %d -> %d\n", trace->src, trace->dst);
  s = trace->signals;
  while(s) {
    g_string_append_printf(out, "    ID: %s\n", s->id);
    g_string_append_printf(out, "     Label: %s\n", s->label);
    g_string_append_printf(out, "     Mask: %d,%d\n",
			   s->bit_offset, s->bit_width);
    g_string_append_printf(out, "     Type: %d\n", s->type);
    s = s->next;
  }
  return FALSE;
}



gchar *
signal_extractor_dump_filter(SignalExtractor *se)
{
  GString *out = g_string_new("Signal filter\n");
  g_tree_foreach(se->traces, dump_trace, out);
  return g_string_free(out, FALSE);
}

const gchar *
signal_extractor_get_type_string(gint type)
{
  switch(type) {
  case SIGNAL_TYPE_UNKNOWN:
    return "unknown";
  case SIGNAL_TYPE_SIGNED_INT:
    return "signed int";
  case SIGNAL_TYPE_UNSIGNED_INT:
    return "unsigned int";
  default:
    
    return "illegal";
  }
}
