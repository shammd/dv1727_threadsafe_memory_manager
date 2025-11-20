# Compiler and Linking Variables
CC = gcc
CFLAGS = -Wall -fPIC
LIB_NAME = libmemory_manager.so
PTHREAD_LIB = -pthread

# Source and Object Files
SRC = memory_manager.c
OBJ = $(SRC:.c=.o)

# Default target
all: gitinfo mmanager test_mmanager test_list

# Rule to create the dynamic library
$(LIB_NAME): $(OBJ)
	$(CC) -shared -o $@ $(OBJ)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Git info (optional)
gitinfo:
	@echo "const char *git_date = \"$(GIT_DATE)\";" > gitdata.h
	@echo "const char *git_sha = \"$(GIT_COMMIT)\";" >> gitdata.h

# Build the memory manager library
mmanager: $(LIB_NAME)

# Build linked list object
linked_list.o: linked_list.c linked_list.h
	$(CC) $(CFLAGS) -c linked_list.c -o linked_list.o $(PTHREAD_LIB)

# Build and link test for memory manager
test_mmanager: gitinfo $(LIB_NAME)
	$(CC) $(CFLAGS) -o test_memory_manager test_memory_manager.c -L. -lmemory_manager -lm

# Build and link test for linked list
test_list: $(LIB_NAME) linked_list.o
	$(CC) $(CFLAGS) -o test_linked_list linked_list.c test_linked_list.c -L. -lmemory_manager -lm $(PTHREAD_LIB)

# Run test for memory manager
run_test_mmanager:
	@LD_LIBRARY_PATH=$$PWD ./test_memory_manager $${test}

# Run test for linked list
run_test_list:
	@LD_LIBRARY_PATH=$$PWD ./test_linked_list

# Clean target
clean:
	rm -f $(OBJ) $(LIB_NAME) test_memory_manager test_linked_list linked_list.o gitdata.h
