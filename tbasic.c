/*  tbasic.c : An implementation of Not-So-TinyBASIC in C

 Scott Baker, https://www.smbaker.com/

 Supports 26 16-bit integer variables named A-Z. Also supports up
 to 26 arrays of 16-bit integer variables also named A-Z. You can
 have both a simple variable and an array of the same name should
 you choose to do so (though you may confuse yourself).
 
 Code is split into a host-agnostic portion (tbasic.c) and a host
 specific version (host.c), with the intent that porting to a new
 environement should generally only involve changes to host.c and
 not changes to tbasic.c.

 The dialect of C is an early variant of K&R, with old-style function
 prototypes and missing some features. The code is the least common
 denominator that should compile on old as well as new environments
 such as Linux.

 The code is very much "assembly converted to C and hacked upon by
 each person to create features they inidividually deemed necessary".
 It is not the quality of a green field implementation and carries
 much baggage.

 Based on Arduino Tiny Basic, by: Mike Field - hamster@snap.net.nz
 Based on TinyBasic for 68000, by Gordon Brandly
 (see http://members.shaw.ca/gbrandly/68ktinyb.html)

 which itself was derived from Palo Alto Tiny BASIC as 
 published in the May 1976 issue of Dr. Dobb's Journal.

 0.04 01/08/2022  : modified for CPM-8000's wonky zcc compiler
 								  : added hex notation &Hxx
									: added load, save commands
									: added DIM for dimensioning arrays
									: added FRE() and RAND() functions
 
 0.03 21/01/2011 : Added INPUT routine 
                 : Reorganised memory layout
                 : Expanded all error messages
                 : Break key added
                 : Removed the calls to printf (left by debugging)

CP/M-8000 Instructions:
	zcc tbasic.c
	zcc host.c
	a:asz8k -o inout.o inout.8kn
	a:ld8k -w -s -o tbasic.z8k startup.o tbasic.o host.o inout.o -lcpm

Linux Build Instructions:
  make
*/

#include <stdio.h>
#include "host.h"

#define MAXLINENUM 65000

/* return values for procline() */
#define PROCLINE_EOF 0
#define PROCLINE_OKAY 1
#define PROCLINE_DIRECT 2
#define PROCLINE_BADLINE 3
#define PROCLINE_DELETE 4
#define PROCLINE_EMPTY 5

/* ASCII Characters */
#define TAB	'\t'
#define BELL	'\b'
#define DEL	'\177'
#define SPACE   ' '
#define CTRLC	0x03
#define CTRLH	0x08
#define CTRLS	0x13
#define CTRLX	0x18

#define debugf if (0) printf

typedef unsigned short LINENUM;

/***********************************************************/
/* Keyword table and constants - the last character has 0x80 added to it */
uchar keywords[] = {
	'L','I','S','T'+0x80,
	'L','O','A','D'+0x80,
	'N','E','W'+0x80,
	'R','U','N'+0x80,
	'S','A','V','E'+0x80,
	'N','E','X','T'+0x80,
	'L','E','T'+0x80,
	'I','F'+0x80,
	'G','O','T','O'+0x80,
	'G','O','S','U','B'+0x80,
	'R','E','T','U','R','N'+0x80,
	'R','E','M'+0x80,
	'F','O','R'+0x80,
	'I','N','P','U','T'+0x80,
	'P','R','I','N','T'+0x80,
	'P','O','K','E'+0x80,
	'S','T','O','P'+0x80,
	'B','Y','E'+0x80,
	'S','Y','S','T','E','M'+0x80,    /* synonym for BYE */
	'O','U','T'+0x80,
  'S','L','E','E','P'+0x80,
  'C','L','E','A','R'+0x80,
	'D','I','M'+0x80,
	'E','N','D'+0x80,               /* synomym for STOP but with the Break! message */
	0
};

#define KW_LIST		0
#define KW_LOAD		1
#define KW_NEW		2
#define KW_RUN		3
#define KW_SAVE		4
#define KW_NEXT		5
#define KW_LET		6
#define KW_IF		7
#define KW_GOTO		8
#define KW_GOSUB	9
#define KW_RETURN	10
#define KW_REM		11
#define KW_FOR		12
#define KW_INPUT	13
#define KW_PRINT	14
#define KW_POKE		15
#define KW_STOP		16
#define KW_BYE		17
#define KW_SYSTEM 18
#define KW_OUT    19
#define KW_SLEEP	20
#define KW_CLEAR  21
#define KW_DIM    22
#define KW_END    23
#define KW_DEFAULT	24


struct stack_for_frame {
	char frame_type;
	char for_var;
	short int terminal;
	short int step;
	uchar *sff_current_line;
	uchar *sff_txtpos;
};

struct stack_gosub_frame {
	char frame_type;
	uchar *sgf_current_line;
	uchar *sgf_txtpos;
};

uchar func_tab[] = {
	'P','E','E','K'+0x80,
	'A','B','S'+0x80,
	'H','I','G','H'+0x80,
	'L','O','W'+0x80,
	'I','N','P'+0x80,
	'F','R','E'+0x80,
	'R','A','N', 'D'+0x80,
	0
};
#define FUNC_PEEK  0
#define FUNC_ABS  1
#define FUNC_HIGH  2
#define FUNC_LOW  3
#define FUNC_INP 4
#define FUNC_FRE 5
#define FUNC_RAND 6
#define FUNC_UNKNOWN 7

uchar to_tab[] = {
	'T','O'+0x80,
	0
};

uchar step_tab[] = {
	'S','T','E','P'+0x80,
	0
};

uchar relop_tab[] = {
	'>','='+0x80,
	'<','>'+0x80,
	'>'+0x80,
	'='+0x80,
	'<','='+0x80,
	'<'+0x80,
	0
};

#define RELOP_GE		0
#define RELOP_NE		1
#define RELOP_GT		2
#define RELOP_EQ		3
#define RELOP_LE		4
#define RELOP_LT		5
#define RELOP_UNKNOWN	        6

#define NUM_VAR 27  /* why is this 27 and not 26 ?? */
#define VAR_SIZE sizeof(short int) /* Size of variables in bytes */

char fn[FNSIZE]; /* filename buffer */
uchar *txtpos, *list_line;
uchar exp_error;
uchar *tempsp;
uchar *stack_limit;
uchar *pgm_start;
uchar *pgm_end;
uchar *stack; /* Software stack for things that should go on the CPU stack */
uchar *variables_table;
uchar *array_table;
uchar *array_sz;
uchar *current_line;
uchar *sp;
uchar *top_sp; /* points to the top of the stack */
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
uchar table_index;
LINENUM linenum;
uchar lecho;

const uchar iomsg[] = "IO Error";
const uchar okmsg[]		= "OK";
const uchar badlinemsg[]		= "Invalid line number";
const uchar invalidexprmsg[] = "Invalid expression";
const uchar syntaxmsg[] = "Syntax Error";
const uchar badinputmsg[] = "\nBad number";
const uchar boundsmsg[] = "Bounds error";
const uchar nomemmsg[]	= "Not enough memory!";
const uchar initmsg[]	= "Z8000 TinyBasic, www.smbaker.com";
const uchar memorymsg[]	= " bytes free.";
const uchar breakmsg[]	= "break!";
const uchar stackstuffedmsg[] = "Stack is stuffed!\n";
const uchar unimplimentedmsg[]	= "Unimplemented";
const uchar backspacemsg[]		= "\b \b";

short int expression();
uchar breakcheck();
/***************************************************************************/
voidret ignore_blanks()
{
	while(*txtpos == SPACE || *txtpos == TAB)
		txtpos++;
}

/* zcc does not support "unsigned char" */
#define SIGNCONV(x) (((x)<0) ? (256+(x)) : (x))

/* zcc does not like word writes that are at odd addresses. Write it as two
 * bytes instead.
 */
voidret encode_linenum(s, linenum)
uchar *s;
unsigned short linenum;
{
	*s = linenum >> 8;
	*(s+1) = linenum & 0xFF;
	/* *((unsigned short *)s) = linenum; */
}

/* see encode_linenum for explanation */
unsigned short decode_linenum(s)
uchar *s;
{
	return (SIGNCONV(*s) << 8) + SIGNCONV(*(s+1));
	/* return *((LINENUM *)(s)); */
}

/***************************************************************************/
voidret scantable(table)
uchar *table;
{
	int i = 0;
	ignore_blanks();
	table_index = 0;
	while(1)
	{
		/* Run out of table entries? */
		if(table[0] == 0)
      return 0;

		/* Do we match this character? */
		if(txtpos[i] == table[0])
		{
			i++;
			table++;
		}
		else
		{
			/* do we match the last character of keyword (with 0x80 added)? If so, return */
			if(txtpos[i]+0x80 == SIGNCONV(table[0]))
			{
				txtpos += i+1;  /* Advance the pointer to following the keyword */
				ignore_blanks();
				return 0;
			}

			/* Forward to the end of this keyword */
			while((SIGNCONV(table[0]) & 0x80) == 0)
				table++;

			/* Now move on to the first character of the next word, and reset the position index */
			table++;
			table_index++;
			i = 0;
		}
	}
}

/***************************************************************************/
voidret pushb(b)
uchar b;
{
	sp--;
	*sp = b;
}

/***************************************************************************/
uchar popb()
{
	uchar b;
	b = *sp;
	sp++;
	return b;
}

/***************************************************************************/
voidret printnum(num)
int num;
{
	int digits = 0;

	if(num < 0)
	{
		num = -num;
		putch('-');
	}

	do {
		pushb(num%10+'0');
		num = num/10;
		digits++;
	}
	while (num > 0);

	while(digits > 0)
	{
		putch(popb());
		digits--;
	}
}
/***************************************************************************/
unsigned short testnum()
{
	unsigned short num = 0;
	ignore_blanks();
	
	while(*txtpos>= '0' && *txtpos <= '9' )
	{
		/* Trap overflows */
		if(num >= MAXLINENUM) /* was 0xFFFF/10, but this seems to be miscomputed in zcc */
		{
			num = 0xFFFF;
			break;
		}

		num = num *10 + *txtpos - '0';
		txtpos++;
	}
	return	num;
}

/***************************************************************************/
uchar check_statement_end()
{
	ignore_blanks();
	return (*txtpos == NL) || (*txtpos == ':');
}

/***************************************************************************/
voidret printnnl(msg)
const uchar *msg;
{
	while(*msg)
	{
		putch(*msg);
		msg++;
	}
}

/***************************************************************************/
uchar print_quoted_string()
{
	int i=0;
	uchar delim = *txtpos;
	if(delim != '"' && delim != '\'')
		return 0;
	txtpos++;

	/* Check we have a closing delimiter */
	while(txtpos[i] != delim)
	{
		if(txtpos[i] == NL)
			return 0;
		i++;
	}

	/* Print the characters */
	while(*txtpos != delim)
	{
		putch(*txtpos);
		txtpos++;
	}
	txtpos++; /* Skip over the last delimiter */
	ignore_blanks();

	return 1;
}

/***************************************************************************/
uchar get_quoted_string(dest)
char *dest;
{
	int i=0;
	int maxlen=FNSIZE-1;
	uchar delim = *txtpos;
	if(delim != '"' && delim != '\'')
		return 0;
	txtpos++;

	/* Check we have a closing delimiter */
	while(txtpos[i] != delim)
	{
		if(txtpos[i] == NL)
			return 0;
		i++;
	}

	/* Print the characters */
	while(*txtpos != delim)
	{
    if (maxlen<=0) {
			return 0; /* complain */
		}

		*dest = *txtpos;
		dest++;
		txtpos++;
		maxlen--;
	}
	txtpos++; /* Skip over the last delimiter */
	ignore_blanks();

	*dest = '\0';

	return 1;
}

/***************************************************************************/
voidret printmsg(msg)
const uchar *msg;
{
	printnnl(msg);
  put_nl();
}

/***************************************************************************/
uchar getln(prompt)
char prompt;
{
	if (prompt) {
	  putch(prompt);
	}
	txtpos = pgm_end+sizeof(LINENUM);

	while(1)
	{
		char c = getch();
		switch(c)
		{
			case EOFC:
			case CR:
			case NL:
			  if (lecho) {
          put_nl();
				}
				/* Terminate all strings with a NL */
				txtpos[0] = NL;
				return 1;
			case CTRLC:
				return 0;
			case CTRLH:
			case DEL:
				if(txtpos == pgm_end)
					break;
				txtpos--;
				printnnl(backspacemsg);
				break;
			default:
				/* We need to leave at least one space to allow us to shuffle the line into order */
				if(txtpos == sp-2)
					putch(BELL);
				else
				{
					txtpos[0] = c;
					txtpos++;
					if (lecho)
					  putch(c);
				}
		}
	}
}

/***************************************************************************/
uchar *findline()
{
	uchar *line = pgm_start;
	while(1)
	{
		if(line == pgm_end) {
			return line;
		}

		if (decode_linenum(line) >= linenum) {
			return line;
		}

		/* Add the line length onto the current address, to get to the next line; */
		line += line[sizeof(LINENUM)];
	}
}

/***************************************************************************/
voidret toUppercaseBuffer()
{
	uchar *c = pgm_end+sizeof(LINENUM);
	uchar quote = 0;

	while(*c != NL)
	{
		/* Are we in a quoted string? */
		if(*c == quote)
			quote = 0;
		else if(*c == '"' || *c == '\'')
			quote = *c;
		else if(quote == 0 && *c >= 'a' && *c <= 'z')
			*c = *c + 'A' - 'a';
		c++;
	}
}

/***************************************************************************/
voidret printline()
{
	LINENUM line_num;

	line_num = decode_linenum(list_line);
	
  list_line += sizeof(LINENUM) + sizeof(char);

	/* Output the line */
	printnum(line_num);
	putch(' ');
	while(*list_line != NL) {
		putch(*list_line);
		list_line++;
	}
	list_line++;
	put_nl();
}

voidret printpgm(linestart)
unsigned short linestart;
{
	list_line = findline();
	while(list_line != pgm_end) {
      printline();
	}
}

voidret dim(name, size)
uchar name;
unsigned short size;
{
	int i;
	unsigned short arr_start;

	if (((short int *)array_sz)[name] >= size) {
		/* use existing array */
    arr_start = ((short int *)array_table)[name];
	} else {
		/* new array, or expanded array */
		/* note: expanding array will cause loss of space */
	  top_sp = top_sp - size*VAR_SIZE;
	  sp = top_sp;
	  arr_start = top_sp-memory;
	}

  /* clear the array */
	for (i=0; i<size; i++) {
		((short int *) (memory+arr_start))[i] = 0;
	}

	((short int *)array_table)[name] = arr_start;
	((short int *)array_sz)[name] = size;
}

/***************************************************************************/
short int expr4()
{
	uchar f;
	short int a = 0;
	short int b = 0;

	ignore_blanks(); /* smbaker */

	if(*txtpos == '0') {
		txtpos++;
		a = 0;
		goto success;
	}

  /* is it a decimal number */
	if(*txtpos >= '1' && *txtpos <= '9')
	{
		do 	{
			a = a*10 + *txtpos - '0';
			txtpos++;
		} while(*txtpos >= '0' && *txtpos <= '9');
		goto success;
	}

  /* is it a hexadecimal number? */
	if ((*txtpos == '&') && (*(txtpos+1)=='H') || ((*txtpos+1)=='h'))
	{
		txtpos++;
		txtpos++;
		do {
			if ((*txtpos >= 'a') && (*txtpos <= 'f')) {
				a = a * 16 + *txtpos - 'a' + 10;
			} else if ((*txtpos >= 'A') && (*txtpos <= 'F')) {
				a = a * 16 + *txtpos - 'A' + 10;
			} else if ((*txtpos >= '0') && (*txtpos <= '9')) {
				a = a * 16 + *txtpos - '0';
			} else {
				break;
			}
			txtpos++;
		} while (1);
		goto success;
	}

	/* Is it a function or variable reference? */
	if(txtpos[0] >= 'A' && txtpos[0] <= 'Z')
	{
		/* is it an array reference */
		if (txtpos[1]=='(') {
			unsigned int arr_ofs = ((short int *)array_table)[*txtpos - 'A'];
			unsigned int arr_siz = ((short int *)array_sz)[*txtpos - 'A'];
			unsigned int index;
			txtpos++; /* now pointing at the paren */
			index = expression();
			if ((index < 0) || (index >= arr_siz)) {
				printmsg(boundsmsg);
				goto expr4_error;
			}
			a = ((short int *) (memory+arr_ofs))[index];
			goto success;
		}

		/* Is it a variable reference (single alpha) */
		if(txtpos[1] < 'A' || txtpos[1] > 'Z')
		{
			a = ((short int *)variables_table)[*txtpos - 'A'];
			txtpos++;
			goto success;
		}

		/* Is it a function with a single parameter */
		scantable(func_tab);
		if(table_index == FUNC_UNKNOWN)
			goto expr4_error;

		f = table_index;

		/* Pseudo Functions added by DCJ for things that need no parms */
		if (f == FUNC_HIGH) {
			a=1;
			goto success;
		}
		if (f == FUNC_LOW) {
			a=0;
			goto success;
		}

		if(*txtpos != '(')
			goto expr4_error;

		txtpos++;
		a = expression();
		if(*txtpos != ')')
				goto expr4_error;
		txtpos++;
		switch(f)
		{
			case FUNC_PEEK:
			  a = peek(a);
				goto success;
			case FUNC_ABS:
				if(a < 0)
					a = -a;
				goto success;
			case FUNC_INP:
			  a = inp(a);
				goto success;
			case FUNC_FRE:
			  a = sp-pgm_end;
				goto success;
			case FUNC_RAND:
			  a = rand(a);
				goto success;
		}
	}

	if(*txtpos == '(')
	{
		txtpos++;
		a = expression();
		if(*txtpos != ')')
			goto expr4_error;

		txtpos++;
		goto success;
	}

expr4_error:
	exp_error = 1;

success:
	ignore_blanks();
	return a;
}

/***************************************************************************/
short int expr3()
{
	short int a,b;

	a = expr4();
	while(1)
	{
		if(*txtpos == '*') {
			txtpos++;
			b = expr4();
			a *= b;
		}
		else if(*txtpos == '/') {
			txtpos++;
			b = expr4();
			if(b != 0)
				a /= b;
			else
				exp_error = 1;
		} else if (*txtpos == 'M' && *(txtpos+1)=='O' && *(txtpos+2)=='D') {
			txtpos++;
			txtpos++;
			txtpos++;
			b=expr4();
			a = a % b;
		}
		else
			return a;
	}
}

/***************************************************************************/
short int expr2()
{
	short int a,b;

	if(*txtpos == '-' || *txtpos == '+')
		a = 0;
	else
		a = expr3();

	while(1)
	{
		if(*txtpos == '-')
		{
			txtpos++;
			b = expr3();
			a -= b;
		}
		else if(*txtpos == '+')
		{
			txtpos++;
			b = expr3();
			a += b;
		}
		else
			return a;
	}
}

/***************************************************************************/
short int expression()
{
	short int a,b;

	a = expr2();
	/* Check if we have an error */
	if(exp_error)	return a;

	scantable(relop_tab);
	if(table_index == RELOP_UNKNOWN)
		return a;
	
	switch(table_index)
	{
	case RELOP_GE:
		b = expr2();
		if(a >= b) return 1;
		break;
	case RELOP_NE:
		b = expr2();
		if(a != b) return 1;
		break;
	case RELOP_GT:
		b = expr2();
		if(a > b) return 1;
		break;
	case RELOP_EQ:
		b = expr2();
		if(a == b) return 1;
		break;
	case RELOP_LE:
		b = expr2();
		if(a <= b) return 1;
		break;
	case RELOP_LT:
		b = expr2();
		if(a < b) return 1;
		break;
	}
	return 0;
}

uchar procline()
{
	uchar *start;
	uchar *newEnd;
	uchar linelen;

	if (!getln('\0')) {
		return PROCLINE_EOF;
	}
	toUppercaseBuffer();

	txtpos = pgm_end+sizeof(unsigned short);

	/* Find the end of the freshly entered line */
	linelen=0;
	while(*txtpos != NL) {
		linelen++;
		txtpos++;
	}

	/* Move it to the end of program_memory */
	{
		uchar *dest;
		dest = sp-1;

    /* zcc does not like words written to odd offsets */
    if ((linelen%2)==0) {
			dest--;
		}

		while(1)
		{
			*dest = *txtpos;
			if(txtpos == pgm_end+sizeof(unsigned short))
				break;
			dest--;
			txtpos--;
		}
		txtpos = dest;
	}

	/* Now see if we have a line number */
	linenum = testnum();
	ignore_blanks();
	if(linenum == 0) {
		if ((*txtpos==NL) || (*txtpos==CR)) {
			return PROCLINE_EMPTY;
		}
	  return PROCLINE_DIRECT;
	}

	if(linenum == 0xFFFF)
	  return PROCLINE_BADLINE;

	/* Find the length of what is left, including the (yet-to-be-populated) line header */
	linelen = 0;
	while(txtpos[linelen] != NL)
		linelen++;
	linelen++; /* Include the NL in the line length */
	linelen += sizeof(unsigned short)+sizeof(char); /* Add space for the line number and line length */

	/* Now we have the number, add the line header. */
	txtpos -= 3;
	encode_linenum(txtpos, linenum);
	txtpos[sizeof(LINENUM)] = linelen;

	/* Merge it into the rest of the program */
	start = findline();

	/* If a line with that number exists, then remove it */
	/*if(start != pgm_end && *((LINENUM *)start) == linenum) {*/
	if (start != pgm_end && decode_linenum(start)==linenum) {
		uchar *dest, *from;
		unsigned tomove;

		from = start + start[sizeof(LINENUM)];
		dest = start;

		tomove = pgm_end - from;
		while( tomove > 0)
		{
			*dest = *from;
			from++;
			dest++;
			tomove--;
		}	
		pgm_end = dest;
	}

	if(txtpos[sizeof(LINENUM)+sizeof(char)] == NL) {
		/* If the line has no txt, it was just a delete */
		return PROCLINE_DELETE;
	}

	/* Make room for the new line, either all in one hit or lots of little shuffles */
	while(linelen > 0)
	{	
		unsigned int tomove;
		uchar *from,*dest;
		unsigned int space_to_make;

		space_to_make = txtpos - pgm_end;

		if(space_to_make > linelen)
			space_to_make = linelen;
		newEnd = pgm_end+space_to_make;
		tomove = pgm_end - start;

		/* Source and destination - as these areas may overlap we need to move bottom up */
		from = pgm_end;
		dest = newEnd;
		while(tomove > 0)
		{
			from--;
			dest--;
			*dest = *from;
			tomove--;
		}

		/* Copy over the bytes into the new space */
		for(tomove = 0; tomove < space_to_make; tomove++)
		{
			*start = *txtpos;
			txtpos++;
			start++;
			linelen--;
		}
		pgm_end = newEnd;
	}
	return PROCLINE_OKAY;
}

voidret loadpgm()
{
  uchar res;
	uchar lecho_save;

  lecho_save = lecho;
	lecho = 0;
	pgm_end = pgm_start;
	while (1) {
		res = procline();
		if ((res != PROCLINE_OKAY) && (res != PROCLINE_EMPTY)) {
			lecho = lecho_save;
			return 0;
		}
	}
}

/* erase all variables and un-declare all arrays */
voidret clear()
{
	int i;
	for (i=0; i<26; i++) {
		((short int *)variables_table)[i] = 0;
		((short int *)array_table)[i] = 0;
		((short int *)array_sz)[i] = 0;
	}
	top_sp = memory+sizeof(memory);
	sp = top_sp;  /* Needed for printnum */
}

voidret initialize()
{
	variables_table = memory;
	array_table = memory + NUM_VAR*VAR_SIZE;
	array_sz = array_table + NUM_VAR*VAR_SIZE;
	pgm_start = array_sz + NUM_VAR*VAR_SIZE;
	pgm_end = pgm_start;
	clear();
}

voidret banner()
{
	printmsg(initmsg);
	printnum(sp-pgm_end);
	printmsg(memorymsg);
}

/***************************************************************************/
voidret loop(autorun)
uchar autorun;
{
  if (autorun) {
		current_line = pgm_start;
		goto execline;
	}

warmstart:
  if (autorun) {
		/* autorun means autoexit when we're done */
		return 0;
	}
	/* this signifies that it is running in 'direct' mode. */
	current_line = 0;
	sp = top_sp;
	printmsg(okmsg);

prompt:
  switch (procline()) {
		case PROCLINE_BADLINE:
		  goto badline;
		case PROCLINE_DIRECT:
		  goto direct;
		/* PROCLINE_OKAY */
		/* PROCLINE_EOF */
		/* PROCLINE_DELETE */
		default:
		  goto prompt;			
	}

unimplemented:
	printmsg(unimplimentedmsg);
	goto prompt;

badline:	
	printmsg(badlinemsg);
	goto prompt;

invalidexpr:
	printmsg(invalidexprmsg);
	goto prompt;

ioerror:
	printmsg(iomsg);
	goto prompt;

syntaxerror:
	printmsg(syntaxmsg);
	if(current_line != 0)  /* smbaker was typecast to vd ptr */
	{
       uchar tmp = *txtpos;
		   if(*txtpos != NL)
				*txtpos = '^';
           list_line = current_line;
           printline();
           *txtpos = tmp;
	}
    put_nl();
	goto prompt;

stackstuffed:	
	printmsg(stackstuffedmsg);
	goto warmstart;

nomem:	
	printmsg(nomemmsg);
	goto warmstart;

run_next_statement:
	while(*txtpos == ':')
		txtpos++;
	ignore_blanks();
	if(*txtpos == NL)
		goto execnextline;
	goto interperateAtTxtpos;

direct: 
	txtpos = pgm_end+sizeof(LINENUM);
	if(*txtpos == NL)
		goto prompt;

interperateAtTxtpos:
        if(breakcheck())
        {
          printmsg(breakmsg);
          goto warmstart;
        }

	scantable(keywords);
	ignore_blanks();

	switch(table_index)
	{
		case KW_LIST:
			goto list;
		case KW_LOAD:
		  goto load;
		case KW_NEW:
			if(txtpos[0] != NL)
				goto syntaxerror;
			pgm_end = pgm_start;
			clear();
			goto prompt;
		case KW_RUN:
			current_line = pgm_start;
			goto execline;
		case KW_SAVE:
			goto save;
		case KW_NEXT:
			goto next;
		case KW_LET:
			goto assignment;
		case KW_IF:
			{
			short int val;
			exp_error = 0;
			val = expression();
			if(exp_error || *txtpos == NL)
				goto invalidexpr;
			if(val != 0)
				goto interperateAtTxtpos;
			goto execnextline;
			}
		case KW_GOTO:
			exp_error = 0;
			linenum = expression();
			if(exp_error || *txtpos != NL)
				goto invalidexpr;
			current_line = findline();
			goto execline;

		case KW_GOSUB:
			goto gosub;
		case KW_RETURN:
			goto gosub_return; 
		case KW_REM:	
			goto execnextline;	/* Ignore line completely */
		case KW_FOR:
			goto forloop; 
		case KW_INPUT:
			goto input; 
		case KW_PRINT:
			goto print;
		case KW_POKE:
			goto do_poke;
		case KW_STOP:
		  printmsg(breakmsg);
			/* fallthrough */
		case KW_END:
			/* This is the easy way to end - set the current line to the end of program attempt to run it */
			if(txtpos[0] != NL)
				goto syntaxerror;
			current_line = pgm_end;
			goto execline;
		case KW_BYE:
		case KW_SYSTEM:
			/* Leave the basic interperater */
			return 0;
		case KW_OUT:
		  goto do_outp;
    case KW_SLEEP:
			goto sleep;
		case KW_CLEAR:
		  goto do_clear;
		case KW_DIM:
		  goto do_dim;
    case KW_DEFAULT:
			goto assignment;
		default:
			break;
	}
	
execnextline:
	if(current_line == 0)		/* Processing direct commands? smbaker: was typecast to vdptr */
		goto prompt;
	current_line +=	 current_line[sizeof(LINENUM)];

execline:
  	if(current_line == pgm_end) /* Out of lines to run */
		goto warmstart;
	txtpos = current_line+sizeof(LINENUM)+sizeof(char);
	goto interperateAtTxtpos;

input:
	{
		uchar isneg=0;
		uchar *temptxtpos;
		short int *var;
		ignore_blanks();
		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;
		var = ((short int *)variables_table)+*txtpos-'A';
		txtpos++;
		if(!check_statement_end())
			goto syntaxerror;
again:
		temptxtpos = txtpos;
		if(!getln('?'))
			goto warmstart;

		/* Go to where the buffer is read */
		txtpos = pgm_end+sizeof(LINENUM);
		if(*txtpos == '-')
		{
			isneg = 1;
			txtpos++;
		}

		*var = 0;
		do 	{
			*var = *var*10 + *txtpos - '0';
			txtpos++;
		} while(*txtpos >= '0' && *txtpos <= '9');
		ignore_blanks();
		if(*txtpos != NL)
		{
			printmsg(badinputmsg);
			goto again;
		}
	
		if(isneg)
			*var = -*var;

		goto run_next_statement;
	}

forloop:
	{
		uchar var;
		short int initial, step, terminal;

		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;
		var = *txtpos;
		txtpos++;
		
		scantable(relop_tab);
		if(table_index != RELOP_EQ)
			goto syntaxerror;

		exp_error = 0;
		initial = expression();
		if(exp_error)
			goto invalidexpr;
	
		scantable(to_tab);
		if(table_index != 0)
			goto syntaxerror;
	
		terminal = expression();
		if(exp_error)
			goto invalidexpr;
	
		scantable(step_tab);
		if(table_index == 0)
		{
			step = expression();
			if(exp_error)
				goto invalidexpr;
		}
		else
			step = 1;
		if(!check_statement_end())
			goto syntaxerror;


		if(!exp_error && *txtpos == NL)
		{
			struct stack_for_frame *f;
			if(sp + sizeof(struct stack_for_frame) < stack_limit)
				goto nomem;

			sp -= sizeof(struct stack_for_frame);
			f = (struct stack_for_frame *)sp;
			((short int *)variables_table)[var-'A'] = initial;
			f->frame_type = STACK_FOR_FLAG;
			f->for_var = var;
			f->terminal = terminal;
			f->step     = step;
			f->sff_txtpos   = txtpos;
			f->sff_current_line = current_line;
			goto run_next_statement;
		}
	}
	goto syntaxerror;

gosub:
	exp_error = 0;
	linenum = expression();
	if(exp_error)
		goto invalidexpr;
	if(!exp_error && *txtpos == NL)
	{
		struct stack_gosub_frame *f;
		if(sp + sizeof(struct stack_gosub_frame) < stack_limit)
			goto nomem;

		sp -= sizeof(struct stack_gosub_frame);
		f = (struct stack_gosub_frame *)sp;
		f->frame_type = STACK_GOSUB_FLAG;
		f->sgf_txtpos = txtpos;
		f->sgf_current_line = current_line;
		current_line = findline();
		goto execline;
	}
	goto syntaxerror;

next:
	/* Fnd the variable name */
	ignore_blanks();
	if(*txtpos < 'A' || *txtpos > 'Z')
		goto syntaxerror;
	txtpos++;
	if(!check_statement_end())
		goto syntaxerror;
	
gosub_return:
	/* Now walk up the stack frames and find the frame we want, if present */
	tempsp = sp;
	while(tempsp < memory+sizeof(memory)-1)
	{
		switch(tempsp[0])
		{
			case STACK_GOSUB_FLAG:
				if(table_index == KW_RETURN)
				{
					struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
					current_line	= f->sgf_current_line;
					txtpos			= f->sgf_txtpos;
					sp += sizeof(struct stack_gosub_frame);
					goto run_next_statement;
				}
				/* This is not the loop you are looking for... so Walk back up the stack */
				tempsp += sizeof(struct stack_gosub_frame);
				break;
			case STACK_FOR_FLAG:
				/* Flag, Var, Final, Step */
				if(table_index == KW_NEXT)
				{
					struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
					/* Is the the variable we are looking for? */
					if(txtpos[-1] == f->for_var)
					{
						short int *varaddr = ((short int *)variables_table) + txtpos[-1] - 'A'; 
						*varaddr = *varaddr + f->step;
						/* Use a different test depending on the sign of the step increment */
						if((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
						{
							/* We have to loop so don't pop the stack */
							txtpos = f->sff_txtpos;
							current_line = f->sff_current_line;
							sp = tempsp; /* SMBAKER: pop any stack pointers for inner loops */
							goto run_next_statement;
						}
						/* We've run to the end of the loop. drop out of the loop, popping the stack */
						sp = tempsp + sizeof(struct stack_for_frame);
						goto run_next_statement;
					}
				}
				/* This is not the loop you are looking for... so Walk back up the stack */
				tempsp += sizeof(struct stack_for_frame);
				break;
			default:
				goto stackstuffed;
		}
	}
	/* Didn't find the variable we've been looking for */
	goto syntaxerror;

assignment:
	{
		short int value;
		short int *var;

		if(*txtpos < 'A' || *txtpos > 'Z')
			goto syntaxerror;

    /* array assignment */
    if(*(txtpos+1) == '(') {
			unsigned int arr_ofs = ((short int *)array_table)[*txtpos - 'A'];
			unsigned int arr_siz = ((short int *)array_sz)[*txtpos - 'A'];
			unsigned int index;
			txtpos++; /* now pointing at the paren */
			index = expr2();
			if ((index <0 ) || (index>=arr_siz)) {
				printmsg(boundsmsg);
				goto invalidexpr;
			}
			var = (short int *) (memory + arr_ofs + index*VAR_SIZE);
			goto asg_var;
		}

		var = (short int *)variables_table + *txtpos - 'A';
		txtpos++;

asg_var:
		ignore_blanks();

		if (*txtpos != '=')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();
		exp_error = 0;
		value = expression();
		if(exp_error)
			goto invalidexpr;
		/* Check that we are at the end of the statement */
		if(!check_statement_end())
			goto syntaxerror;
		*var = value;
	}
	goto run_next_statement;

sleep:
        {
                short int value;
                exp_error = 0;
		value = expression();
		if(exp_error)
  	            goto invalidexpr;
                /* delay(value); */
        }
        goto run_next_statement;

do_clear:
  clear();
	goto run_next_statement;

do_dim:
  {
		uchar varnum;
		unsigned int arrsize;
    if(*txtpos < 'A' || *txtpos > 'Z')
	    goto syntaxerror;
		varnum = *txtpos - 'A';
		txtpos++;

		ignore_blanks();
		if (*txtpos != '(')
		  goto syntaxerror;

		arrsize = expression();
		dim(varnum, arrsize+1);
		if(!check_statement_end())
			goto syntaxerror;

		goto run_next_statement;
	}

do_poke:
	{
		short int value;
		uchar *address;

		/* Work out where to put it */
		exp_error = 0;
		value = expression();
		if(exp_error)
			goto invalidexpr;
		address = (uchar *)value;

		/* check for a comma */
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		/* Now get the value to assign */
		exp_error = 0;
		value = expression();
		if(exp_error)
			goto invalidexpr;
		poke(address, (uchar) value);
		/* Check that we are at the end of the statement */
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;

do_outp:
	{
		short int value;
		uchar *address;

		/* Work out where to put it */
		exp_error = 0;
		value = expression();
		if(exp_error)
			goto invalidexpr;
		address = (uchar *)value;

		/* check for a comma */
		ignore_blanks();
		if (*txtpos != ',')
			goto syntaxerror;
		txtpos++;
		ignore_blanks();

		/* Now get the value to assign */
		exp_error = 0;
		value = expression();
		if(exp_error)
			goto invalidexpr;
		outp(address, (uchar) value);
		/* Check that we are at the end of the statement */
		if(!check_statement_end())
			goto syntaxerror;
	}
	goto run_next_statement;	

list:
	linenum = testnum(); /* Retuns 0 if no line found. */

	/* Should be EOL */
	if(txtpos[0] != NL)
		goto syntaxerror;

	printpgm(linenum);
	goto warmstart;

save:
  if (!get_quoted_string(fn))
	  goto syntaxerror;
	if (!open_write(fn)) 
	  goto ioerror;
  printpgm(0);
	close_file();
	goto warmstart;

load:
  if (!get_quoted_string(fn))
	  goto syntaxerror;
	if (!open_read(fn))
	  goto ioerror;
  loadpgm();
	close_file();
	goto warmstart;

print:
	/* If we have an empty list then just put out a NL */
	if(*txtpos == ':' )
	{
        put_nl();
		txtpos++;
		goto run_next_statement;
	}
	if(*txtpos == NL)
	{
		goto execnextline;
	}

	while(1)
	{
		ignore_blanks();
		if(print_quoted_string())
		{
			;
		}
		else if(*txtpos == '"' || *txtpos == '\'')
			goto syntaxerror;
		else
		{
			short int e;
			exp_error = 0;
			e = expression();
			if(exp_error)
				goto invalidexpr;
			printnum(e);
		}

		/* At this point we have three options, a comma or a new line */
		if(*txtpos == ',')
			txtpos++;	/* Skip the comma and move onto the next */
		else if(txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
		{
			txtpos++; /* This has to be the end of the print - no newline */
			break;
		}
		else if(check_statement_end())
		{
			put_nl();	/* The end of the print statement */
			break;
		}
		else
			goto syntaxerror;	
	}
	goto run_next_statement;
}

/***********************************************************/
uchar breakcheck() {
  if(kbhit())
    return getch() == CTRLC;
  else
    return 0;
}

int main(argc, argv)
int argc;
char **argv;
{
	lecho = enable_raw_mode();
	initialize();

	if (argc>1) {
	  if (!open_read(argv[1])) {
			printmsg("Failed to load program\n");
			disable_raw_mode();
			return -1;
		}
    loadpgm();
	  close_file();
		loop(1);     /* automatically RUN */
	} else {
		banner();
    loop(0);     /* don't acutomatically RUN */
	}

	disable_raw_mode();
}
