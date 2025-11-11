# ===========================================
# DV1727 - Thread-Safe Memory Manager (Final Fix)
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
LIB_NAME = libmemory_manager.so
APPNAME = linked_list_app

# Default target
all: $(LIB_NAME) $(APPNAME)

# -------------------------------------------
# Build shared memory manager library
# -------------------------------------------
$(LIB_NAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIB_NAME) memory_manager.c

# -------------------------------------------
# Build linked list app (for your local testing)
# -------------------------------------------
$(APPNAME): linked_list.c linked_list.h main.c $(LIB_NAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o $(APPNAME) linked_list.c main.c -l:$(LIB_NAME)

# -------------------------------------------
# Required by CodeGrade — this is what they test
# -------------------------------------------
list: linked_list.o
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o test_linked_list linked_list.c -l:$(LIB_NAME) -ldl -lm

test_memory_manager: $(LIB_NAME)
	$(CC) $(CFLAGS) -Wl,-rpath=$(PWD) -o test_memory_manager test_memory_manager.c -L. -lmemory_manager -lm

# -------------------------------------------
# Clean up
# -------------------------------------------
clean:
	rm -f $(APPNAME) $(LIB_NAME) test_linked_list test_memory_manager *.o

.PHONY: all clean list test_memory_manager
