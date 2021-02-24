</$objtype/mkfile

CFLAGS=-FTV -p -I../shim -I../lua

LIB=liblpeg.a.$O

OBJS=\
	lpvm.$O\
	lpcap.$O\
	lptree.$O\
	lpcode.$O\
	lpprint.$O

all:V: $LIB

clean:V:
	rm -f *.[$OS] *.a.[$OS]

$LIB: $OBJS
	ar cr $target $prereq

%.$O: %.c
	$CC $CFLAGS $stem.c
