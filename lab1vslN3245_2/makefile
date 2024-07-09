CC=gcc
CFLAGS=-Wall -Wextra -Werror -O3
LDFLAGS=-ldl -lm

.PHONY: all clean

# Имена целевых файлов
TARGETS=lab1vslN3245 libvslN3245.so

all: $(TARGETS)

lab1vslN3245: lab1vslN3245.c plugin_api.h
	$(CC) $(CFLAGS) -o $@ lab1vslN3245.c $(LDFLAGS)

libvslN3245.so: libvslN3245.c plugin_api.h
	$(CC) $(CFLAGS) -shared -fPIC -o $@ libvslN3245.c $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o
