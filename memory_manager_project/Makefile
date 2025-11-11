# ===========================================
#  CROSS-PLATFORM MAKEFILE (Linux / CodeGrade)
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
APPNAME = linked_list_app
LIBNAME = libmemory_manager.so

# -------------------------------------------
# Default rule: build both library and app
# -------------------------------------------
all: mmanager list


mmanager: memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIBNAME) memory_manager.c


list: linked_list.c linked_list.h main.c
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=. -lmemory_manager -o $(APPNAME) linked_list.c main.c


clean:
	rm -f $(APPNAME) $(LIBNAME) *.o


run: all
	./$(APPNAME)

.PHONY: all mmanager list clean run
