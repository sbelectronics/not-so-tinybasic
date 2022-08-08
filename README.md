# Not-So-TinyBASIC in C
Scott Baker, https://www.smbaker.com/

## Summary

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

Ported by Scott Baker to CP/M-8000 for use on his Zilog Z8000 project.

Based on Arduino Tiny Basic, by: Mike Field - hamster@snap.net.nz
Based on TinyBasic for 68000, by Gordon Brandly
(see http://members.shaw.ca/gbrandly/68ktinyb.html)

which itself was derived from Palo Alto Tiny BASIC as 
published in the May 1976 issue of Dr. Dobb's Journal.

## Variables

* A - Z ... 26 variables, each one a 16-bit signed integer
* A - Z ... 26 arrays, each one holding a list of 16-bit signed integers. Arrays must be dimensioned first using DIM. An array can have the same name as a variable, but will be treated differently. They are differentiated as arrays are always referred to with parenthesis. eg A(6)=123. eg X=A(6).

## Commands

* BYE
* CLEAR ... Removes all variables and arrays
* LIST
* LOAD
* NEW
* RUN
* SAVE
* SYSTEM ... synonym for BYE

## Statements

* DIM .. Dimensions an array, eg DIM A(5)
* END ... Ends current Program
* FOR ... STEP ... NEXT
* GOTO
* GOSUB
* IF ... GOTO
* INPUT
* LET ... Assigns the value of an expression to a variable. A=5 and LET A=5 do the same thing.
* OUT ... Outputs a value to a port, eg OUT &H50, &H11
* POKE ... Writes to a memory location, eg POKE &H1234, &H11
* PRINT
* RETURN
* STOP ... like END, but prints "Break!" first

## Functions

* PEEK ... returns the contents of a memory address
* ABS ... return absolute value
* HIGH ... returns 1. Takes no argument.
* LOW ... return 0. Takes no argument.
* INP ... inputs from a port, eg X = INP(&H50)
* FRE ... returns free memory. Takes one argument that doesn't matter.
* RAND ... generates a random number between 0 and the argument.

## Revision History

 0.04 01/08/2022  smbaker

* modified for CPM-8000's wonky zcc compiler
* added hex notation &Hxx
* added load, save commands
* added DIM for dimensioning arrays
* added FRE() and RAND() functions

 0.03 21/01/2011 

* Added INPUT routine
* Reorganised memory layout
* Expanded all error messages
* Break key added
* Removed the calls to printf (left by debugging)

## Build Instructions

CP/M-8000 Instructions:
	  zcc tbasic.c
	  zcc host.c
	  a:asz8k -o inout.o inout.8kn
	  a:ld8k -w -s -o tbasic.z8k startup.o tbasic.o host.o inout.o -lcpm

Linux Build Instructions:
    make