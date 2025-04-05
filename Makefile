
GTK=$(shell pkg-config gtk+-3.0 --cflags --libs)
SDL=$(shell pkg-config sdl3 -cflags --libs)

.PHONY: test clean all


spiritify: main.c
	gcc main.c -o spiritify $(GTK) -lmytool -L. -lSDL3_mixer -lasound $(SDL)



clean: 
	rm spiritify
