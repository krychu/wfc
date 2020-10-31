make wfc: wfc.c
	cc wfc.c -DWFC_TOOL `pkg-config --libs --cflags raylib` -o wfc
