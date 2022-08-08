/* host.c
 *
 * This file holds host-specific code, for character IO, file IO, etc.
 */

#ifdef DONOTUSE
#include <sys/ioctl.h>
#include <termios.h>
#endif

#include <stdio.h>
#include "host.h"

uchar memory[MEMSIZE];
FILE *r_file = NULL;
FILE *w_file = NULL;

#ifdef LINUX
voidret outp(x,y)
unsigned short x;
char y;
{
  printf("<OUTP %02X, %02X>", x, y);
}

uchar inp(x)
unsigned short x;
{
  printf("<INP %02X -> 0x33>", x);
  return 0x33;
}
#endif

/* return 1 if raw_mode successfully enabled */
voidret enable_raw_mode()
{
#ifdef DONOTUSE
    /* there's really no reason for raw mode; everything seems to
     *  work fine in cooked mode. So just avoid the nuisance...
     */
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);
    return 1;
#endif
  return 0; /* raw_mode unsupported */
}

voidret disable_raw_mode()
{
#ifdef DONOTUSE
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(0, TCSANOW, &term);
#endif
}

int kbhit() {
#ifdef DONOTUSE
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    return byteswaiting > 0;
#else
  return 0;
#endif
}

/* after a successful open_write, all putch() will go
 * to the file
 */
int open_write(fn)
char *fn;
{
  w_file = fopen(fn, "wt");
  if (w_file == NULL) {
    return 0;
  } else {
    return 1;
  }
}

/* after a successful open_read, all getch() will come
 * from the file
 */
int open_read(fn)
char *fn;
{
  r_file = fopen(fn, "rt");
  if (r_file == NULL) {
    return 0;
  } else {
    return 1;
  }
}

voidret close_file()
{
  if (w_file != NULL) {
    fclose(w_file);
    w_file = NULL;
  }
  if (r_file != NULL) {
    fclose(r_file);
    r_file = NULL;
  }
}

char getch()
{
  if (r_file != NULL) {
    if (feof(r_file)) {
      return EOFC;
    } else {
      return fgetc(r_file);
    }
  } else {
    return getchar();
  }
}

voidret putch(c)
uchar c;
{
  if (w_file) {
    fputc(c, w_file);
  } else {
    putchar(c);
  }
}

voidret put_nl()
{
#ifndef LINUX
  putch(CR);
#endif	
  putch(NL);
}

voidret poke(x,y)
unsigned short x;
uchar y;
{
  memory[x] = y;
}

uchar peek(x)
unsigned short x;
{
  return memory[x];
}

long seed = 1;

/* random nunmber - may be machine dependent */
unsigned short rand(amount)
unsigned short amount;
{
    long int a = 16807L, m = 2147483647L, q = 127773L, r = 2836L;
    long int lo, hi, test;

    hi = seed / q;
    lo = seed % q;
    test = a * lo - r * hi;
    if (test > 0) 
		    seed = test; /* test for overflow */
    else 
		    seed = test + m;
    return(seed % amount);
}
