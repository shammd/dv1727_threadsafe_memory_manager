# ===========================================
#  FINAL FIXED MAKEFILE for CodeGrade (Linux)
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
APPNAME = linked_list_app
LIBNAME = libmemory_manager.so

# -------------------------------------------
# Default rule: build both library and app
# -------------------------------------------
all: $(LIBNAME) $(APPNAME)

# -------------------------------------------
# Build shared library (.so)
# -------------------------------------------
$(LIBNAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIBNAME) memory_manager.c

# -------------------------------------------
# Build linked list app (link with .so)
# -------------------------------------------
$(APPNAME): linked_list.c linked_list.h main.c $(LIBNAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o $(APPNAME) linked_list.c main.c -l:$(LIBNAME)

# -------------------------------------------
# Clean up
# -------------------------------------------
clean:
	rm -f $(APPNAME) $(LIBNAME) *.o

# -------------------------------------------
# Run (for local testing)
# -------------------------------------------
run: all
	./$(APPNAME)

.PHONY: all clean run
