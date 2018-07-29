LIBRARY = lib0stor.so

PREFIX = /usr/local
LIBDIR = lib

SRC = lib0stor.c 3rdparty/xxtea-c/xxtea.c
OBJ = lib0stor.o xxtea.o

CFLAGS  += -std=c99 -pthread -fPIC -W -Wall -O2 -I3rdparty/xxtea-c -g
LDFLAGS += -lsnappy -lz -lm -lb2

all: $(LIBRARY)

$(LIBRARY): $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	ar rcs lib0stor.a $(OBJ)

lib0stor.o: lib0stor.c
	$(CC) $(CFLAGS) -c $<

xxtea.o: 3rdparty/xxtea-c/xxtea.c
	$(CC) $(CFLAGS) -c $<

install: all
	mkdir -p $(DESTDIR)/$(PREFIX)/$(LIBDIR)
	cp -f $(LIBRARY)* $(DESTDIR)/$(PREFIX)/$(LIBDIR)
	cp -f *.a $(DESTDIR)/$(PREFIX)/$(LIBDIR)
	mkdir -p $(DESTDIR)/$(PREFIX)/include
	cp lib0stor.h $(DESTDIR)$(PREFIX)/include/
	@echo -e "\nYou may want to run ldconfig if you install this locally"

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(LIBRARY) *.a
