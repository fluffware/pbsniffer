#include <pcapng.h>
#include <stdlib.h>

int
main(int argc, const char *argv[])
{
  static const guint8 pdata[] = {0x23,0x67,0x12,0x87,0x12};
  GError *err = NULL;
  GFile *file;
  GFileOutputStream *out;
  ByteChain *options = NULL;
  file = g_file_new_for_commandline_arg (argv[1]);
  out = g_file_replace(file,NULL, FALSE, G_FILE_CREATE_NONE, NULL, &err);
  if (!out) {
    g_printerr("Failed open output file: %s\n", err->message);
    return EXIT_FAILURE;
  }
  pcapng_add_string_option(&options, PCAPNG_OPTION_SHB_USERAPPL, "pcapngtst"); 
  if (!pcapng_write_section_header(G_OUTPUT_STREAM(out), -1, options, &err)) {
     g_printerr("Failed to write section header: %s\n", err->message);
     return EXIT_FAILURE;
  }
  pcapng_clear_options(&options);
  pcapng_add_string_option(&options, PCAPNG_OPTION_IF_NAME, "/dev/ttyUSB0"); 
  if (!pcapng_write_interface_description(G_OUTPUT_STREAM(out), DLT_PROFIBUS_DL, 256, options, &err)) {
     g_printerr("Failed to write interface description: %s\n", err->message);
     return EXIT_FAILURE;
  }
  pcapng_clear_options(&options);
  
  if (!pcapng_write_enhanced_packet(G_OUTPUT_STREAM(out), 0, 0, 5, 5, pdata, options, &err)) {
    g_printerr("Failed to write packet data: %s\n", err->message);
     return EXIT_FAILURE;
  }
  g_object_unref(out);
  g_object_unref(file);
  return EXIT_SUCCESS;
}
