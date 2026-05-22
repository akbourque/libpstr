# --- libpstr Modernized Module Makefile ---
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -fPIC -Isrc
DEBUG_FLAGS = -g -O1 -fsanitize=address -fno-omit-frame-pointer
RELEASE_FLAGS = -O3 -flto
LDFLAGS = -shared

# Targets
LIB_NAME = libpstr
SRC = src/panic.c src/libpstr.c
OBJ = $(SRC:.c=.o)

all: static shared

# Build the static library (.a)
static: $(OBJ)
	ar rcs $(LIB_NAME).a $(OBJ)

# Build the shared library (.so)
shared: $(OBJ)
	$(CC) $(LDFLAGS) -o $(LIB_NAME).so $(OBJ)

# 🚀 The Test Rule: Runs 'clean' first, then compiles from 'src/tests/' and runs it
test: clean
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -DENABLE_PANIC_TESTING $(SRC) src/tests/test_libpstr.c -o pstr_test
	./pstr_test

# Compile individual object files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o src/tests/*.o *.so *.a pstr_test
