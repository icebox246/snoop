snoop: main.c
	gcc main.c -o snoop `pkg-config --cflags --libs xcb sdl2`
