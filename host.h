/* set this to the amount of program memory to reserve for programs and variables */
#define MEMSIZE 32768

/* maximum size of a filename */
#define FNSIZE 32

/* zcc hates the static keyword */
#define static /**/

/* zcc gets wound up if you use const */
#define const /**/

/* zcc isn't very fond of unsigned applied to char */
typedef char uchar;

/* zcc seems to really dislike the void keyword; also dislikes funcs that don't declare a return kind */
typedef int voidret;

/* this holds the program and variable memory */
extern uchar memory[MEMSIZE];

#define CR	'\r'
#define NL	'\n'
#define EOFC 0x1A

/* for port input/output */
voidret outp(x,y);
uchar inp(x);

int enable_raw_mode();
voidret disable_raw_mode();
int kbhit();
char getch();
voidret putch(c);
voidret put_nl();
voidret poke(x,y);
uchar peek(x);
int open_write(fn);
int open_read(fn);
voidret close_file();
unsigned short rand(amount);
