#include "pcapng.h"
gboolean
pcapng_write_block(GOutputStream *out, guint32 type, 
		   gconstpointer body, gsize body_len,  GError **err)
{
  static const guint8 pad[] = {0,0,0};
  guint32 header[2];
  header[0] = type;
  header[1] = body_len + 12;
  if (!g_output_stream_write(out, header, sizeof(header), NULL, err)) return FALSE;
  if (!g_output_stream_write(out, body, body_len, NULL, err)) return FALSE;
  if ((body_len & 3) != 0) {
    if (!g_output_stream_write(out, pad, body_len & 3, NULL, err)) return FALSE;
  }
  if (!g_output_stream_write(out, &header[1], sizeof(header[1]), NULL, err))
    return FALSE;
  return TRUE;
}


gboolean
pcapng_write_section_header(GOutputStream *out,
			    gint64 section_len,
			    gconstpointer options, gsize options_len,
			    GError **err)
{
  static const guint32 magic = 0x1a2b3c4d;
  static const guint16 major_version = 1;
  static const guint16 minor_version = 0;
  guint8 body[16];
  *(guint32*)body = magic;
  *(guint16*)(body + 4) = major_version;
  *(guint16*)(body + 6) = minor_version;
  *(gint64*)(body + 8) = section_len;
  
  return pcapng_write_block(out, 0x0a0d0d0a, body, sizeof(body), err);
}

gboolean
pcapng_add_interface_description(GOutputStream *out, GError **err)
{
  const guint8 body[] = {0x01,0x8d,0x00,0x00, };
  return TRUE;
}
