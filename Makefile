.DEFAULT_GOAL := help

help: ## Show available commands
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-15s %s\n", $$1, $$2}'

wfctool: wfctool.c ## Build wfctool
	cc wfctool.c -O3 -DWFC_TOOL -o wfc -lm

debug: wfctool.c ## Build wfctool with debug symbols
	cc wfctool.c -g -DWFC_TOOL -o wfc -lm

wfc.o: ## Build wfc.o for linking
	cp wfc.h /tmp/wfc.c
	cc -c -DWFC_IMPLEMENTATION -DWFC_USE_STB -I. /tmp/wfc.c
	rm -f /tmp/wfc.c

test: tests/test.c wfc.h ## Run unit and integration tests
	@mkdir -p tests/output tests/reference
	cc tests/test.c -O2 -I. -o tests/test -lm
	./tests/test

test-asan: tests/test.c wfc.h ## Run tests with AddressSanitizer
	@mkdir -p tests/output tests/reference
	cc tests/test.c -g -fsanitize=address -I. -o tests/test -lm
	./tests/test

bench: tests/bench.c wfc.h ## Run benchmarks
	cc tests/bench.c -O2 -I. -o tests/bench -lm
	./tests/bench

deps: ## Download stb_image.h and stb_image_write.h
	curl -sL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o stb_image.h
	curl -sL https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h -o stb_image_write.h

clean: ## Remove build artifacts
	rm -f wfc wfc.o tests/test tests/bench
