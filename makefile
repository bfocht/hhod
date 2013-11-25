SRC = ./
INC = include
LIBS = 
CC = gcc
CFLAGS = -Wall -g -O3  


hhod_SOURCES = hhod.c decode.c global.c \
		 decode.h global.h
		 
hhod_OBJECTS = mochad.o decode.o global.o


# -----------------------------------------------------------------------
# Makefile script for building all programs and docs

all: clean build

# -----------------------------------------------------------------------


# -----------------------------------------------------------------------
# Makefile script for building all programs

code: 	build

# -----------------------------------------------------------------------

build: hhod.o decode.o global.o
	$(CC) $(LIBS) $(CFLAGS) hhod.o decode.o global.o -o hhod

hhod.o: $(SRC)/hhod.c $(SRC)/global.h $(SRC)/decode.h
	$(CC) $(CFLAGS)-c $(SRC)/hhod.c
	
decode.o: $(SRC)/hhod.c $(SRC)/global.h
	$(CC) $(CFLAGS)-c $(SRC)/decode.c
	
global.o: $(SRC)/hhod.c 
	$(CC) $(CFLAGS)-c $(SRC)/global.c	
	
# ---------------------------------------------------------------------

#----------------clean old objects for rebuilding --------------------
clean:
	rm *.o -f
