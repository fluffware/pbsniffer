#include "write_libpcap.h"

/* Copied from http://wiki.wireshark.org/Development/LibpcapFileFormat */

typedef struct pcap_hdr_s {
        guint32 magic_number;   /* magic number */
        guint16 version_major;  /* major version number */
        guint16 version_minor;  /* minor version number */
        gint32  thiszone;       /* GMT to local correction */
        guint32 sigfigs;        /* accuracy of timestamps */
        guint32 snaplen;        /* max length of captured packets, in octets */
        guint32 network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
        guint32 ts_sec;         /* timestamp seconds */
        guint32 ts_usec;        /* timestamp microseconds */
        guint32 incl_len;       /* number of octets of packet saved in file */
        guint32 orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

gboolean
libpcap_write_global_header(GOutputStream *out,
			    gint32 thiszone,
			    guint32 snap_len,
			    guint32 link_type,
			    GError **err)
{
  pcap_hdr_t hdr;
  hdr.magic_number = 0xa1b2c3d4;
  hdr.version_major = 2;
  hdr.version_minor = 4;
  hdr.thiszone = thiszone;
  hdr.sigfigs = 0;
  hdr.snaplen = snap_len;
  hdr.network = link_type;
  
  return g_output_stream_write(out, &hdr, sizeof(hdr), NULL,err);
}
  
  

gboolean
libpcap_write_packet(GOutputStream *out,
		     guint32 ts_sec, guint32 ts_usec,
		     guint captured_len, guint packet_len,
		     const guint8 *packet_data,
		     GError **err)
{
  pcaprec_hdr_t hdr;
  hdr.ts_sec = ts_sec;
  hdr.ts_usec = ts_usec;
  hdr.incl_len = captured_len;
  hdr.orig_len = packet_len;
  if (!g_output_stream_write(out, &hdr, sizeof(hdr), NULL,err)) {
    return FALSE;
  }
  return g_output_stream_write(out, packet_data, captured_len, NULL,err);
}
