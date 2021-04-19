</$objtype/mkfile

CFLAGS=-FTVw -p -I../lua/shim -I../lua

LIB=libp9.a.$O

OBJS=p9.$O

all:V: $LIB

clean:QV:
	rm -f *.[$OS] *.a.[$OS]

$LIB: $OBJS
	ar cr $target $prereq

%.$O: %.c
	$CC $CFLAGS $stem.c

p9.$O: p9.c fs.c walk.c env.c ns.c proc.c misc.c
