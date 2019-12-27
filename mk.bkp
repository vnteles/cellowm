PROJ_NAME=cellowm


INCLUDE_PATH=./src/include
C_SRC=$(wildcard ./src/*.c)
H_SRC=$(wildcard $(INCLUDE_PATH)/*.h)

OBJ=$(subst .c,.o,$(subst src,obj,$(C_SRC)))

PREFIX?=/usr
BINDIR?=$(PREFIX)/bin

CC=gcc -I$(INCLUDE_PATH)
CFLAGS+=-std=c11 -W -Wextra -Wall
LDADD+=-lxcb -lxcb-cursor -lxcb-ewmh -lxcb-icccm -lxcb-keysyms -lpthread

RM=rm -f

all: obj_folder $(PROJ_NAME)

$(PROJ_NAME): $(OBJ)
	@ echo 'Building binary using GCC linker: $@'
	$(CC) -o $@ $^ $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo -e 'Build finished\n'

./obj/main.o: ./src/main.c $(H_SRC)
	@ echo 'Building target using GCC compiler: $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '

./obj/%.o: ./src/%.c $(INCLUDE_PATH)/%.h
	@ echo 'Building target using GCC compiler: $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '

obj_folder:
	@ mkdir -p ./obj

clean:
	@ $(RM) ./obj/*.o *~ $(PROJ_NAME)
	@ rmdir ./obj

install:
	install -Dm 755 $(PROJ_NAME) $(BINDIR)/$(PROJ_NAME)

.PHONY: all clean install
