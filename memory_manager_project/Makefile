# ===========================================
#  THREAD-SAFE MEMORY MANAGER + LINKED LIST
#  Compatible with CodeGrade autograder
# ===========================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
LIB_NAME = libmemory_manager.so
APPNAME = linked_list_app

# -------------------------------------------
# Default target
# -------------------------------------------
all: gitinfo mmanager list test_memory_manager test_list

# -------------------------------------------
# Shared library (memory manager)
# -------------------------------------------
$(LIB_NAME): memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIB_NAME) memory_manager.c

mmanager: $(LIB_NAME)

# -------------------------------------------
# Git info (for teacher’s build compatibility)
# -------------------------------------------
gitinfo:
	@echo "const char *git_date = \"$(shell date)\";" > gitdata.h
	@echo "const char *git_sha = \"local-build\";" >> gitdata.h

# -------------------------------------------
# Build linked list app (for manual testing)
# -------------------------------------------
$(APPNAME): linked_list.c linked_list.h main.c $(LIB_NAME)
	$(CC) $(CFLAGS) -I. -L. -Wl,-rpath=$(PWD) -o $(APPNAME) linked_list.c main.c -l:$(LIB_NAME)

list: linked_list.o

# -------------------------------------------
# Tests used by CodeGrade
# -------------------------------------------
test_memory_manager: $(LIB_NAME) test_memory_manager.c
	$(CC) $(CFLAGS) -Wl,-rpath=. -o test_memory_manager test_memory_manager.c -L. -lmemory_manager -lm

test_list: $(LIB_NAME) linked_list.o test_linked_list.c
	$(CC) $(CFLAGS) -Wl,-rpath=. -o test_linked_list linked_list.c test_linked_list.c -L. -lmemory_manager -ldl -lm

# -------------------------------------------
# Cleanup
# -------------------------------------------
clean:
	rm -f $(APPNAME) $(LIB_NAME) test_memory_manager test_linked_list *.o gitdata.h

.PHONY: all clean list mmanager test_list test_memory_manager
