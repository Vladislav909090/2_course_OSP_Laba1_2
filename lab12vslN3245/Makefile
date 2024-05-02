CC=gcc
CFLAGS=-Wall -Wextra -Werror -O3

all: main plugin

main: main.c
    $(CC) $(CFLAGS) -o $@ $^ -ldl

plugin: plugin.c
    $(CC) $(CFLAGS) -shared -fPIC -o $@.so $^

clean:
    rm -f main plugin.so