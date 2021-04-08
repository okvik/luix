</$objtype/mkfile

MOD=p9
MODPATH=/sys/lib/lua

CFLAGS=-FTVw -p -I../lua/shim -I../lua

LIB=libp9.a.$O

OBJS=p9.$O

all:V: $LIB

install:V: all
	if(~ $#luav 0)
		luav=`{lu9 -v}
	for(p in $MODPATH/$luav)
		mkdir -p $p/$MOD && dircp mod $p/$MOD

clean:V:
	rm -f *.[$OS] *.a.[$OS]

$LIB: $OBJS
	ar cr $target $prereq

%.$O: %.c
	$CC $CFLAGS $stem.c

p9.$O: p9.c fs.c proc.c

