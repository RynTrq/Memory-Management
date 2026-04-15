CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
SANITIZE_FLAGS ?= -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all test demo sanitize clean

all: demo

demo: mems_demo

mems_demo: main.c mems.c mems.h
	$(CC) $(CFLAGS) main.c mems.c -o mems_demo

test_mems: test_mems.c mems.c mems.h
	$(CC) $(CFLAGS) test_mems.c mems.c -o test_mems

test: test_mems
	./test_mems

sanitize: CFLAGS += $(SANITIZE_FLAGS)
sanitize: clean test

clean:
	rm -f mems_demo test_mems
