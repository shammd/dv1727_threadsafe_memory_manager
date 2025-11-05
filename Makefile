# ======================================
#  Makefile för trådsäker minneshanterare
#  och länkad lista (Windows + Linux)
# ======================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -pthread
APP = linked_list_app

# Kolla vilket OS som används (Windows_NT = Windows)
ifeq ($(OS),Windows_NT)
    LIB = memory_manager.dll
    LINKFLAG = -l:memory_manager.dll
else
    LIB = libmemory_manager.so
    LINKFLAG = -lmemory_manager
endif

# Standardkommandot: bygger både biblioteket och programmet
all: mmanager list

# Bygger det dynamiska biblioteket för minneshanteraren
mmanager: memory_manager.c memory_manager.h
	$(CC) $(CFLAGS) -shared -o $(LIB) memory_manager.c

# Bygger huvudprogrammet och länkar mot biblioteket
list: linked_list.c linked_list.h main.c
	$(CC) $(CFLAGS) -L. $(LINKFLAG) -o $(APP) linked_list.c main.c -I.

# Rensar bort allt som kompilerats
clean:
	rm -f $(APP) $(LIB) *.o

# Kör programmet (bygger först om det behövs)
run: all
	./$(APP)

.PHONY: all mmanager list clean run
