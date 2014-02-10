#include "pseudo_ports.h"
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_PTY_H
#include <pty.h>
#endif

#define PRGNAME "pty"
#define PERROR(str) perror(PRGNAME ": " str)

int
setup_pseudo_terminal()
{
  char ptyname[FILENAME_MAX];
  int tty = -1;
#ifdef HAVE_OPENPTY
  {
    int slave;
    struct termios term;
    struct winsize wsize;
    term.c_iflag = IGNPAR ;
    term.c_oflag = 0;
    term.c_cflag = CS8 | CREAD | CLOCAL | B38400;
    term.c_lflag = 0;
    wsize.ws_row=24;
    wsize.ws_col=80;

    if (openpty(&tty,&slave,ptyname,&term,&wsize)) {
      PERROR("openpty");
      return -1;
    }
  }
#else
  {
    static char ptychars[] = "0123456789abcdef";
    char *ptychar = ptychars;    
    strcpy(ptyname,"/dev/ptyp0");
    while(*ptychar != '\0') {
      ptyname[9] = *ptychar;
      tty = open(ptyname,O_RDWR);
      if (tty >= 0) break;
      ptychar++;
    }

    if (*ptychar == '\0') {
      fprintf(stderr,PRGNAME ": No pty's available\n");
      return -1;
    }
    if (tty <0) {
      PERROR("open");
      return -1;
    }
  }
#endif
  fprintf(stdout,"Use %s as remote port\n",ptyname);
  return tty;
}
 
