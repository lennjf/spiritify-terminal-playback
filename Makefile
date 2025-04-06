
GTK=$(shell pkg-config gtk+-3.0 --cflags --libs)
SDL=$(shell pkg-config sdl3 -cflags --libs)

.PHONY: test clean all


sp: main.c
	gcc main.c -o sp $(GTK) -lmytool -L. -lSDL3_mixer -lasound $(SDL)

spiritify: main.c
	gcc -O3 -ffunction-sections -fdata-sections main.c -o spiritify $(GTK) -lmytool -L. -lSDL3_mixer -lasound $(SDL) -Wl,--gc-sections
	strip spiritify


clean: 
	rm spiritify sp
