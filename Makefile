#
# Makefile for spin compiler
# Copyright (c) 2011 Total Spectrum Software Inc.
#

CC = gcc
CFLAGS = -g -Wall -Werror
YACC = bison
RM = rm -f

PROGS = testlex spin2c
LEXOBJS = lexer.o symbol.o ast.o
OBJS = $(LEXOBJS) spin.tab.o expr.o functions.o

all: $(PROGS)

testlex: testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^

spin.tab.c spin.tab.h: spin.y
	$(YACC) -t -b spin -d spin.y

lexer.c: spin.tab.h

clean:
	$(RM) $(PROGS) *.o spin.tab.c spin.tab.h

test: testlex
	./testlex
	(cd Test; ./runtests.sh)

spin2c: spin2c.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
