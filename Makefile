make wfctool: wfctool.c
	cc wfctool.c -g -DWFC_TOOL -o wfc -lm

# in case you'd rather have a more traditional wfc.o file to link against
wfc.o:
	cp wfc.h /tmp/wfc.c
	cc -c -DWFC_IMPLEMENTATION -DWFC_USE_STB -I. /tmp/wfc.c
	rm -f /tmp/wfc.c

clean:
	rm -f wfc wfc.o

