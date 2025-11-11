# ===========================================
# FINAL FIXED MAKEFILE for CodeGrade (Linux)
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
LIBNAME = libmemory_manager.so
APPNAME = linked_list_app

# Default target
all: $(LIBNAME) $(APPNAME) test_linked_list

# Shared library
$(LIBNAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIBNAME) memory_manager.c

# Linked list app
$(APPNAME): linked_list.c linked_list.h main.c $(LIBNAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -o $(APPNAME) linked_list.c main.c -l:$(LIBNAME)

# CodeGrade test build (links with cM2.c and libdl)
test_linked_list: linked_list.c linked_list.h cM2.c common_defs.h memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -o test_linked_list linked_list.c cM2.c -l:$(LIBNAME) -ldl

# Clean
clean:
	rm -f $(APPNAME) $(LIBNAME) test_linked_list *.o

.PHONY: all clean
