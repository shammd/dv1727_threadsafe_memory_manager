# ===============================
# Thread-Safe Memory Manager + Linked List
# ===============================

CC = gcc
CFLAGS = -Wall -Wextra -fPIC -pthread -g
LDFLAGS = -pthread
LIB_NAME = libmemory_manager.so

# --- Source Files ---
MM_SRC = memory_manager.c
LIST_SRC = linked_list.c
TEST_MM_SRC = test_memory_manager.c
TEST_LIST_SRC = test_linked_list.c

# --- Output Files ---
MM_LIB = $(LIB_NAME)
TEST_MM = test_memory_manager
TEST_LIST = test_linked_list

.PHONY: all clean mmanager list test_memory_manager test_list run_tests

# Default: build everything
all: mmanager list test_memory_manager test_list

# --- Build dynamic library ---
mmanager: $(MM_LIB)

$(MM_LIB): $(MM_SRC)
	$(CC) $(CFLAGS) -shared -o $(MM_LIB) $(MM_SRC)

# --- Build linked list app (non-test) ---
list: $(LIST_SRC) $(MM_LIB)
	$(CC) $(CFLAGS) -o linked_list $(LIST_SRC) -L. -lmemory_manager $(LDFLAGS) -lm -ldl -Wl,-rpath,.

# --- Test: memory manager ---
test_memory_manager: $(MM_LIB) $(TEST_MM_SRC)
	$(CC) $(CFLAGS) -o $(TEST_MM) $(TEST_MM_SRC) -L. -lmemory_manager $(LDFLAGS) -lm -ldl -Wl,-rpath,.

# --- Test: linked list ---
test_list: $(MM_LIB) $(LIST_SRC) $(TEST_LIST_SRC)
	$(CC) $(CFLAGS) -o $(TEST_LIST) $(LIST_SRC) $(TEST_LIST_SRC) -L. -lmemory_manager $(LDFLAGS) -lm -ldl -Wl,-rpath,.

# --- Run all tests ---
run_tests: test_memory_manager test_list
	./$(TEST_MM)
	./$(TEST_LIST)

# --- Clean ---
clean:
	rm -f *.o $(MM_LIB) $(TEST_MM) $(TEST_LIST) linked_list
