PROJ=cellowm
CONN=cellist

INCLUDE_PATH=./src/include
C_SRC=$(shell find ./src \( -name "*.c" ! -iname "cellist.c" \) -type f)
H_SRC=$(shell find ./src/ -name "*.h" -type f)

OBJ=$(subst .c,.o,$(subst src,obj,$(C_SRC)))

PREFIX?=/usr

RM=rm -f

CC=gcc -std=c11
CFLAGS+= -W -Wextra -Wall -I$(INCLUDE_PATH)
LDADD+=-lxcb -lxcb-cursor -lxcb-ewmh -lxcb-icccm

all: obj_dir bin_dir $(PROJ) cellist

$(PROJ): $(OBJ)
	@ echo 'Making $(PROJ)'
	$(CC) -o ./bin/$@ $^ $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo -e 'Build finished!\n'

./obj/main.o: ./src/main.c $(H_SRC)
	@ echo 'Building $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '

./obj/%.o: ./src/%.c $(INCLUDE_PATH)/%.h
	@ echo 'Building $<'
	$(CC) -c -o $@ $< $(CFLAGS) $(LDADD) $(UDEFINES)
	@ echo ' '

cellist: ./obj/cellist.o ./obj/list.o ./obj/utils.o
	@ echo 'Making $<'
	$(CC) -o ./bin/$@ $^ $(CFLAGS) $(LDADD) $(UDEFINES)
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
	@ $(RM) "./bin/$(PROJ)" "./bin/cellist"
	@ rmdir ./bin

	@ echo 'All clear!'

install:
	install -Dm 755 ./bin/$(PROJ) 	$(PREFIX)/bin/$(PROJ)
	install -Dm 755 ./bin/cellist 	$(PREFIX)/bin/cellist
	install -Dm 644 ./cello.desktop /usr/share/xsessions/cello.desktop

uninstall:
	$(RM) $(PREFIX)/bin/$(PROJ)
	$(RM) $(PREFIX)/bin/cellist
	$(RM) /usr/share/xsessions/cello.desktop

.PHONY: all clean install
