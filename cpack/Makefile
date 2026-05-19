CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I./src -I./src/vendor

TARGET = cpack
OBJS = src/main.o \
	   src/parser.o \
	   src/vec_cpack_dependency.o \
	   src/vec_pstr_ptr.o \
	   src/crawler.o \
	   src/emitter.o \
	   src/vendor/libpstr.o \
	   src/vendor/pstr_utf8.o \
	   src/vendor/pstr_vec.o \
	   src/vendor/panic.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

src/vendor/%.o: src/vendor/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/vendor/*.o $(TARGET)
