# ===========================================
# CodeGrade-compatible Makefile (FINAL)
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
LIBNAME = libmemory_manager.so
APPNAME = linked_list_app

# -------------------------------------------
# Default rule: build library and app
# -------------------------------------------
all: $(LIBNAME) $(APPNAME)

# -------------------------------------------
# Build the shared library (.so)
# -------------------------------------------
$(LIBNAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIBNAME) memory_manager.c

# -------------------------------------------
# Build linked list app (your program)
# -------------------------------------------
$(APPNAME): linked_list.c linked_list.h main.c $(LIBNAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -o $(APPNAME) linked_list.c main.c -l:$(LIBNAME)

# -------------------------------------------
# Build test binaries for CodeGrade (important!)
# -------------------------------------------
test_list: $(LIBNAME) linked_list.c test_linked_list.c
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -o test_linked_list linked_list.c test_linked_list.c -l:$(LIBNAME) -ldl -lm

test_memory_manager: $(LIBNAME) test_memory_manager.c
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -o test_memory_manager test_memory_manager.c -l:$(LIBNAME) -ldl -lm

# -------------------------------------------
# Clean up
# -------------------------------------------
clean:
	rm -f $(APPNAME) $(LIBNAME) test_linked_list test_memory_manager *.o

.PHONY: all clean test_list test_memory_manager
