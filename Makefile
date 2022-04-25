CPPFLAGS=-DNDEBUG -I/usr/include/chipmunk
LDLIBS=-lm -lchipmunk -lcaca -lncursesw $(shell pkg-config --libs glib-2.0)
CFLAGS=-ggdb3 -O0 $(shell pkg-config --cflags glib-2.0)

.PHONY: clean debug

all: katamascii
	make -C art
	make -C tiles

katamascii: sprite.o sprwin.o logging.o effects.o

debug: katamascii
	env G_MESSAGES_DEBUG=all G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind --log-file=valgrind.out --leak-check=full ./katamascii

clean:
	rm -f *.o *.core katamascii
	rm -f debug.log
	rm -f valgrind.out
	rm -f valgrind.out.*
	make -C art clean
	make -C tiles clean
