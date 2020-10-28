make wfc: main.c test.c
	cc main.c -g `pkg-config --libs --cflags raylib`
	cc test.c -g `pkg-config --libs --cflags raylib` -o test
