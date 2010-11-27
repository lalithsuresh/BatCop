BINDIR=../bin
LOCALESDIR=../share/locals
MANDIR=../share/man/man8
WARNFLAGS=-Wall  -W -Wshadow
CFLAGS?=-O1 -g ${WARNFLAGS}


# 
# The w in -lncursesw is not a typo; it is the wide-character version
# of the ncurses library, needed for multi-byte character languages
# such as Japanese and Chinese etc.
#
# On Debian/Ubuntu distros, this can be found in the
# libncursesw5-dev package. 
#

OBJS = batcop.o analysis.o perf.o\
       alglibinternal.o alglibmisc.o ap.o\
       dataanalysis.o optimization.o linalg.o\
	     specialfunctions.o statistics.o solvers.o

batcop: $(OBJS) Makefile batcop.h
	g++ ${CFLAGS} $(LDFLAGS) $(OBJS) -lncursesw -o batcop
	@(cd po/ && $(MAKE))

install: batcop
	mkdir -p ${DESTDIR}${BINDIR}
	cp batcop ${DESTDIR}${BINDIR}
	mkdir -p ${DESTDIR}${MANDIR}
	@(cd po/ && env LOCALESDIR=$(LOCALESDIR) DESTDIR=$(DESTDIR) $(MAKE) $@)

# This is for translators. To update your po with new strings, do :
# svn up ; make uptrans LG=fr # or de, ru, hu, it, ...
uptrans:
	@(cd po/ && env LG=$(LG) $(MAKE) $@)

clean:
	rm -f *~ batcop batcop.8.gz po/batcop.pot DEADJOE svn-commit* *.o *.orig 
	@(cd po/ && $(MAKE) $@)


dist:
	rm -rf .svn po/.svn DEADJOE po/DEADJOE todo.txt Lindent svn-commit.* dogit.sh git/ *.rej *.orig
