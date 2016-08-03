#MAKE=/bin/make
MAKEFILE=picasso.mk

SYSTEM=SYSV
VERSION=3.3

OWNER=bin
GROUP=bin

BINDIR=/usr/bin
FONTDIR=/usr/local/dwb/lib/font
MAN1DIR=/usr/man/u_man/man1
POSTLIB=/usr/local/dwb/lib/postscript

CFLGS=-Wall -Wextra
LDFLGS=-s 
CFLAGS=$(CFLGS)
LDFLAGS=$(LDFLGS)
#YFLAGS=-d
#YACC=yacc
TFLAGS=

OFILES	= picassol.o \
	  ps_include.o \
	  print.o \
	  misc.o \
	  symtab.o \
	  blockgen.o \
	  boxgen.o \
	  circgen.o \
	  arcgen.o \
	  linegen.o \
	  movegen.o \
	  textgen.o \
	  troffgen.o \
	  picgen.o \
	  attrs.o \
	  xform.o \
	  objcopy.o \
	  fonts.o \
	  input.o \
	  for.o \
	  plps.o \
	  math.o \
	  font.o \
	  xstubs.o
FILES	= main.o picassoy.o $(OFILES)
CFILES	= ps_include.c \
	  main.c \
	  print.c \
	  misc.c \
	  symtab.c \
	  blockgen.c \
	  boxgen.c \
	  circgen.c \
	  arcgen.c \
	  linegen.c \
	  movegen.c \
	  textgen.c \
	  troffgen.c \
	  picgen.c \
	  attrs.c \
	  xform.c \
	  objcopy.c \
	  fonts.c \
	  input.c \
	  for.c \
	  plps.c \
	  math.c \
	  font.c \
	  xstubs.c
SRCFILES= picassoy.y picassol.l picasso.h font.h ps_include.h $(CFILES)

all:	picasso

yacc:	y1.o y2.o y3.o y4.o
	$(CC) -o yacc y[1-4].o

picasso:	$(FILES)
	$(CC) -o picasso $(CFLAGS) $(FILES) -lm $(LDFLAGS)

install:	picasso
	@if [ ! -d "$(POSTLIB)" ]; then \
	    mkdir $(POSTLIB); \
	    chmod 755 $(POSTLIB); \
	    chgrp $(GROUP) $(POSTLIB); \
	    chown $(OWNER) $(POSTLIB); \
	fi
	cp picasso $(BINDIR)
	@chmod 755 $(BINDIR)/picasso
	@chgrp $(GROUP) $(BINDIR)/picasso
	@chown $(OWNER) $(BINDIR)/picasso
	cp picasso.ps fonts.ps $(POSTLIB)
	@chmod 644 $(POSTLIB)/fonts.ps $(POSTLIB)/picasso.ps
	@chgrp $(GROUP) $(POSTLIB)/fonts.ps $(POSTLIB)/picasso.ps
	@chown $(OWNER) $(POSTLIB)/fonts.ps $(POSTLIB)/picasso.ps
	cp picasso.1 $(MAN1DIR)/picasso.1
	@chmod 644 $(MAN1DIR)/picasso.1
	@chgrp $(GROUP) $(MAN1DIR)/picasso.1
	@chown $(OWNER) $(MAN1DIR)/picasso.1

main.o:		main.c picasso.h picassoy.o
	$(CC) -c $(CFLAGS) -DGWBFILES=\"$(POSTLIB)\" -DFONTDIR=\"$(FONTDIR)\" main.c

ps_include.o:	ps_include.h

$(OFILES):	picasso.h picassoy.o

picassoy.o:	picassoy.y
	@echo "	yacc -d picassoy.y"
	@if yacc -d picassoy.y ; then \
		true; \
	else \
		echo ; \
		echo "Your yacc can't make picasso.  Trying to make one that will..." ; \
		make  -f yacc.mk ; \
		echo "	yacc -d picassoy.y" ; \
		./yacc -d picassoy.y ; \
	fi
	$(CC) -c $(CFLAGS) y.tab.c
	mv y.tab.o picassoy.o
	rm y.tab.c

clean:
	rm -f *.o y.tab.h picassoy.c picassol.c

clip:
	rm -f picasso

clobber:	clean clip

changes :
	@trap "" 1 2 3 15; \
	sed \
	    -e "s'^SYSTEM=.*'SYSTEM=$(SYSTEM)'" \
	    -e "s'^VERSION=.*'VERSION=$(VERSION)'" \
	    -e "s'^GROUP=.*'GROUP=$(GROUP)'" \
	    -e "s'^OWNER=.*'OWNER=$(OWNER)'" \
	    -e "s'^BINDIR=.*'BINDIR=$(BINDIR)'" \
	    -e "s'^POSTLIB=.*'POSTLIB=$(POSTLIB)'" \
	    -e "s'^FONTDIR=.*'FONTDIR=$(FONTDIR)'" \
	    -e "s'^MAN1DIR=.*'MAN1DIR=$(MAN1DIR)'" \
	$(MAKEFILE) >X$(MAKEFILE); \
	mv X$(MAKEFILE) $(MAKEFILE)

