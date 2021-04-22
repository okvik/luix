</$objtype/mkfile

CFLAGS=-FTVw -p -I../lua/shim -I../lua

LIB=libp9.a.$O
MOD=\
	base\
	note

default:V: all

obj/:
	mkdir -p $target

obj/%.$O: obj/ %/%.c
	$CC $CFLAGS -o $target $stem/$stem.c

$LIB: ${MOD:%=obj/%.$O}
	ar cr $target $prereq

all:V: $LIB

clean:V:
	rm -rf *.a.[$OS] obj/
