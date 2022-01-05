make wfctool: wfctool.c
	cc wfctool.c -g -DWFC_TOOL -o wfc -lm
