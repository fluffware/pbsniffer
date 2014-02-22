#ifndef __PCAPNG_H__L5HCMC6DBE__
#define __PCAPNG_H__L5HCMC6DBE__

#include <glib.h>
#include <gio/gio.h>

/* Hardcode it here so we don't need to depend on pcap.h */
#ifndef DLT_PROFIBUS_DL	
#define DLT_PROFIBUS_DL		257
#endif

#define PCAPNG_OPTION_END 0
#define PCAPNG_OPTION_COMMENT 1

#define PCAPNG_OPTION_SHB_HARDWARE 2
#define PCAPNG_OPTION_SHB_OS 3
#define PCAPNG_OPTION_SHB_USERAPPL 4

#define PCAPNG_OPTION_IF_NAME 2
#define PCAPNG_OPTION_IF_DESCRIPTION 3
#define PCAPNG_OPTION_IF_IPV4ADDR 4
#define PCAPNG_OPTION_IF_IPV6ADDR 5
#define PCAPNG_OPTION_IF_MACADDR 6
#define PCAPNG_OPTION_IF_EUIADDR 7
#define PCAPNG_OPTION_IF_SPEED 8
#define PCAPNG_OPTION_IF_TSRESOL 9
#define PCAPNG_OPTION_IF_TZONE 10
#define PCAPNG_OPTION_IF_FILTER 11
#define PCAPNG_OPTION_IF_OS 12
#define PCAPNG_OPTION_IF_FCSLEN 13
#define PCAPNG_OPTION_IF_TSOFFSET 14


typedef struct _ByteChain ByteChain;

gboolean
pcapng_write_section_header(GOutputStream *out,
			    gint64 section_len,
			    const ByteChain *options,
			    GError **err);

gboolean
pcapng_write_interface_description(GOutputStream *out,
				   guint link_type, guint snap_len,
				   const ByteChain *options,
				   GError **err);

gboolean
pcapng_write_enhanced_packet(GOutputStream *out,
			     guint if_id, guint64 timestamp,
			     guint captured_len, guint packet_len,
			     const guint8 *packet_data,
			     const ByteChain *options,
			     GError **err);

/* *options must NULL on first call */

gboolean
pcapng_add_option(ByteChain **options, guint code, gsize length,
		  const guint8 *value);

gboolean
pcapng_add_string_option(ByteChain **options, guint code, const gchar *str);


void
pcapng_clear_options(ByteChain **options);

#endif /* __PCAPNG_H__L5HCMC6DBE__ */
