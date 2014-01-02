#ifndef __WRITE_LIBPCAP_H__41A06QZ39H__
#define __WRITE_LIBPCAP_H__41A06QZ39H__

#include <glib.h>
#include <gio/gio.h>

gboolean
libpcap_write_global_header(GOutputStream *out,
			    gint32 thiszone,
			    guint32 snap_len,
			    guint32 link_type,
			    GError **err);

gboolean
libpcap_write_packet(GOutputStream *out,
		     guint32 ts_sec, guint32 ts_usec,
		     guint captured_len, guint packet_len,
		     const guint8 *packet_data,
		     GError **err);


#endif /* __WRITE_LIBPCAP_H__41A06QZ39H__ */
