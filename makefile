NAME=cellowm
CONN=celloc

INCLUDE_PATH=./src/include
C_SRC=$(shell find ./src -name "*.c" -type f)
H_SRC=$(shell find ./src/ -name "*.h" -type f)

OBJ=$(subst .c,.o,$(subst src,obj,$(C_SRC)))

PREFIX?=/usr

RM=rm -f

CC=gcc -std=c11
CFLAGS+= -W -Wextra -Wall -I$(INCLUDE_PATH)
LDADD+=`pkg-config --libs --cflags xcb xcb-cursor xcb-ewmh xcb-icccm xcb-keysyms xcb-shape` -lpthread

all: obj_folder bin_dir $(NAME)


$(NAME): $(OBJ)
	@ echo 'Building binary using GCC linker: $@'
	$(CC) -o $@ $^ $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo -e 'Build finished!\n'

./obj/main.o: ./src/main.c $(H_SRC)
	@ echo 'Building target using GCC compiler: $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '

./obj/%.o: ./src/%.c $(INCLUDE_PATH)/%.h
	@ echo 'Building target using GCC compiler: $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '


bin_dir:
	@ mkdir -p ./bin
obj_dir:
	@ mkdir -p ./obj

clean:
	@ echo 'Cleaning objects..'
	@ $(RM) ./obj/*.o *~
	@ rmdir ./obj

	@ echo 'Cleaning binaries..'
	@ $(RM) "./bin/$(NAME)" "./bin/$(NAME)C"
	@ rmdir ./bin

	@ echo 'All clear!'

install:
	install -Dm 755 ./bin/$(NAME) $(PREFIX)/bin/$(NAME)

.PHONY: all clean install
