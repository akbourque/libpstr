# --- libpstr Makefile ---
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -fPIC
DEBUG_FLAGS = -g -O1 -fsanitize=address -fno-omit-frame-pointer
RELEASE_FLAGS = -O3 -flto
LDFLAGS = -shared

# Targets
LIB_NAME = libpstr
SRC = panic.c pstr_utf8.c pstr_vec.c libpstr.c
OBJ = $(SRC:.c=.o)

all: static shared

# Build the static library (.a)
static: $(OBJ)
	ar rcs $(LIB_NAME).a $(OBJ)

# Build the shared library (.so)
shared: $(OBJ)
	$(CC) $(LDFLAGS) -o $(LIB_NAME).so $(OBJ)

# Build for testing with ASan
test: clean
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -DENABLE_PANIC_TESTING $(SRC) test.c -o pstr_test
	./pstr_test

# Compile individual object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so *.a pstr_test
