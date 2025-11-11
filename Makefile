CC = gcc
CFLAGS = -Wall -Wextra -fPIC -pthread -g
LDFLAGS = -pthread

# memory manager library
MM_SRC = memory_manager.c
MM_LIB = libmemory_manager.so

# linked list app
LIST_SRC = linked_list.c test_linked_list.c
LIST_EXE = test_linked_list

# memory manager test
MM_TEST_SRC = test_memory_manager.c
MM_TEST_EXE = test_memory_manager

.PHONY: all clean mmanager list mmtest

all: mmanager list mmtest

mmanager: $(MM_LIB)

$(MM_LIB): $(MM_SRC)
	$(CC) $(CFLAGS) -shared -o $(MM_LIB) $(MM_SRC)

list: $(LIST_EXE)

$(LIST_EXE): $(LIST_SRC) $(MM_LIB)
	$(CC) $(CFLAGS) -o $(LIST_EXE) $(LIST_SRC) -L. -lmemory_manager $(LDFLAGS) -lm -Wl,-rpath,.

mmtest: $(MM_TEST_EXE)

$(MM_TEST_EXE): $(MM_TEST_SRC) $(MM_LIB)
	$(CC) $(CFLAGS) -o $(MM_TEST_EXE) $(MM_TEST_SRC) -L. -lmemory_manager $(LDFLAGS) -lm -Wl,-rpath,.

clean:
	rm -f *.o $(MM_LIB) $(LIST_EXE) $(MM_TEST_EXE)
