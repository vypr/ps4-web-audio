CLANG ?= clang
LLD ?= ld.lld
PYTHON ?= python3
APPFLAGS = --target=x86_64-pc-freebsd12-elf -O2 -fPIE -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fno-asynchronous-unwind-tables -Wall -Wextra -Werror -I build
SRC=app/launcher.c app/worker.c
all: payload
build:
	mkdir -p build
build/web_page.h: app/web.html scripts_embed.py | build
	$(PYTHON) scripts_embed.py $< $@ web_page
build/launcher.o: app/launcher.c app/runtime.h app/bta_types.h | build
	$(CLANG) $(APPFLAGS) -c $< -o $@
build/worker.o: app/worker.c app/runtime.h app/web_audio.h build/web_page.h | build
	$(CLANG) $(APPFLAGS) -c $< -o $@
build/BTA-Web-Audio.bin: build/launcher.o build/worker.o app/payload.ld
	$(LLD) --oformat=binary -T app/payload.ld build/launcher.o build/worker.o -o $@
	$(LLD) -T app/payload.ld build/launcher.o build/worker.o -o build/BTA-Web-Audio.debug.elf
payload: build/BTA-Web-Audio.bin
test: build/web_page.h | build
	cc -O2 -Wall -Wextra -Werror -I app tests/test_audio.c -o build/test_audio
	./build/test_audio
	cc -O2 -Wall -Wextra -Werror -Wno-unused-function -I app -I build tests/test_transport.c -o build/test_transport
	./build/test_transport
clean:
	rm -rf build
.PHONY: all payload clean test
