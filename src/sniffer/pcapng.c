#include "pcapng.h"
#include <string.h>

struct _ByteChain
{
  gsize len; /* If 0 then the next field is a sub chain */
  union {
    const guint8 *bytes;
    ByteChain *sub_chain; /* May be NULL to denote an empty block */
  } data;
  ByteChain *next;
};
  
static gboolean
pcapng_write_chain(GOutputStream *out, ByteChain *chain,  GError **err)
{
  while(chain) {
    if (chain->len) {
      if (!g_output_stream_write(out, chain->data.bytes, chain->len, NULL,err))
	return FALSE;
    } else {
      if (chain->data.sub_chain) {
	if (!pcapng_write_chain(out, chain->data.sub_chain, err)) {
	  return FALSE;
	}
      }
    }
    chain = chain->next;
  }
  return TRUE;
}

static gsize
chain_length(ByteChain *chain)
{
  gsize len = 0;
  while(chain) {
    if (chain->len) {
      len += chain->len;
    } else {
      if (chain->data.sub_chain) {
	len += chain_length(chain->data.sub_chain);
      }
    }
    chain = chain->next;
  }
  return len;
}

/* l - data length
   p - multiple to pad to

   Returns length of padding needed (or 0 for none)
*/

#define PADLEN(l,p) (((p)-1)-(((l)+ ((p)-1)) % (p)))

static gboolean
pcapng_write_block(GOutputStream *out, guint type, ByteChain *body,  GError **err)
{
  static const guint8 pad[] = {0,0,0};
  ByteChain chain[4];
  guint32 header[2];
  gsize body_len = chain_length(body);
  header[0] = type;
  header[1] = body_len + 12;
  chain[0].len = sizeof(header);
  chain[0].data.bytes = (guint8*)header;
  chain[0].next = &chain[1];
  chain[1].len = 0;
  chain[1].data.sub_chain = body;
  chain[1].next = &chain[2];
  chain[2].len = PADLEN(body_len, 4);
  chain[2].data.bytes = chain[2].len == 0 ? (guint8*)NULL : pad;
  chain[2].next = &chain[3];
  chain[3].len = sizeof(header[1]);
  chain[3].data.bytes = (guint8*)&header[1];
  chain[3].next = NULL;
  
  return pcapng_write_chain(out, chain, err);
}

static const ByteChain *
default_options(const ByteChain *options)
{
  static const guint8 data[4] = {0x00, 0x00, 0x00, 0x00};
  static const ByteChain empty_options = {4,{data}, NULL};
  if (options) return options;
  return &empty_options;
}

gboolean
pcapng_write_section_header(GOutputStream *out,
			    gint64 section_len,
			    const ByteChain *options,
			    GError **err)
{
  ByteChain chain[1];
  static const guint32 magic = 0x1a2b3c4d;
  static const guint16 major_version = 1;
  static const guint16 minor_version = 0;
  guint8 body[16];
  *(guint32*)body = magic;
  *(guint16*)(body + 4) = major_version;
  *(guint16*)(body + 6) = minor_version;
  *(gint64*)(body + 8) = section_len;
  chain[0].len = sizeof(body);
  chain[0].data.bytes = body;
  chain[0].next = (ByteChain*)default_options(options);
  
  return pcapng_write_block(out, 0x0a0d0d0a, chain, err);
}

gboolean
pcapng_write_interface_description(GOutputStream *out,
				   guint link_type, guint snap_len,
				   const ByteChain *options,
				   GError **err)
{
  ByteChain chain[1];
  guint8 body[8];
  *(guint16*)(body) = link_type;
  *(guint16*)(body + 2) = 0;
  *(guint32*)(body + 4) = snap_len;
  
  chain[0].len = sizeof(body);
  chain[0].data.bytes = body;
  chain[0].next = (ByteChain*)default_options(options);
  
  return pcapng_write_block(out, 0x00000001, chain, err);
}

gboolean
pcapng_write_enhanced_packet(GOutputStream *out,
			     guint if_id, guint64 timestamp,
			     guint captured_len, guint packet_len,
			     const guint8 *packet_data,
			     const ByteChain *options,
			     GError **err)
{
  static const guint8 pad[] = {0,0,0};
  ByteChain chain[3];
  guint8 header[20];
  *(guint32*)(header) = if_id;
  *(guint32*)(header + 4) = timestamp >> 32;
  *(guint32*)(header + 8) = timestamp;
  *(guint32*)(header + 12) = captured_len;
  *(guint32*)(header + 16) = packet_len;
  chain[0].len = sizeof(header);
  chain[0].data.bytes = header;
  chain[0].next = &chain[1];
  
  chain[1].len = captured_len;
  chain[1].data.bytes = packet_data;
  chain[1].next = &chain[2];

  chain[2].len = PADLEN(packet_len,4);
  chain[2].data.bytes = chain[2].len > 0 ? pad : NULL;
  chain[2].next = (ByteChain*)default_options(options);

  return pcapng_write_block(out, 0x00000006, chain, err);
}

static ByteChain *
allocate_option(guint code, gsize length, const guint8 *value)
{
  ByteChain *chain = g_new(ByteChain, 1);
  guint8 padding = PADLEN(length,4);
  guint8 *bytes = g_new(guint8, length + 4 + padding);
  *(guint16*)bytes = code;
  *(guint16*)(bytes+2) = length;
  if (length > 0) {
    memcpy(bytes+4, value, length);
  }
  chain->len = length + 4 + padding;
  chain->data.bytes = bytes;
  chain->next = NULL;
  return chain;
}

gboolean
pcapng_add_option(ByteChain **options, guint code, gsize length,
		  const guint8 *value)
{
  ByteChain *chain;
  if (!*options) {
    *options = allocate_option(PCAPNG_OPTION_END,0, NULL);
  }
  chain = allocate_option(code, length, value);
  chain->next = *options;
  *options = chain;
  return TRUE;
}

gboolean
pcapng_add_string_option(ByteChain **options, guint code, const gchar *str)
{
  return pcapng_add_option(options, code, strlen(str), (const guint8*)str);
}


void
pcapng_clear_options(ByteChain **options)
{
  ByteChain *chain = *options;
  while(chain) {
    ByteChain *next = chain->next;
    if (chain->len > 0) {
      g_free((guint8*)chain->data.bytes);
    }
    g_free(chain);
    chain = next;
  }
  *options = chain;
}

