# ===========================================
#  THREAD-SAFE MEMORY MANAGER - CODEGRADE FIX
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
LIB_NAME = libmemory_manager.so
APPNAME = linked_list_app

# Default rule
all: $(LIB_NAME) $(APPNAME)

# -------------------------------------------
# Build shared library
# -------------------------------------------
$(LIB_NAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIB_NAME) memory_manager.c

# -------------------------------------------
# Build app for local testing
# -------------------------------------------
$(APPNAME): linked_list.c linked_list.h main.c $(LIB_NAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o $(APPNAME) linked_list.c main.c -l:$(LIB_NAME)

# -------------------------------------------
# REQUIRED BY CODEGRADE
# They call "make list" to build the linked list + library test
# -------------------------------------------
list: linked_list.c linked_list.h $(LIB_NAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o test_linked_list linked_list.c test_linked_list.c -l:$(LIB_NAME) -ldl -lm

# -------------------------------------------
# For teacher’s memory manager tests
# -------------------------------------------
test_memory_manager: $(LIB_NAME) test_memory_manager.c
	$(CC) $(CFLAGS) -Wl,-rpath=$(PWD) -o test_memory_manager test_memory_manager.c -L. -lmemory_manager -lm

# -------------------------------------------
# Cleanup
# -------------------------------------------
clean:
	rm -f $(APPNAME) $(LIB_NAME) test_linked_list test_memory_manager *.o

.PHONY: all clean list test_memory_manager
