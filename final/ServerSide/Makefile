### DEFINITIONS

-include Makefile.settings

all: $(OUTFILE)
install: install-bin
uninstall: uninstall-bin

### MAIN PROGRAM

$(OUTFILE): ServerSide.o conf.o conn.o ftp.o http.o search.o tcp.o text.o
	$(CC) *.o -o $(OUTFILE) $(LFLAGS)
ifndef DEBUG
	-$(STRIP) $(OUTFILE)
endif

.c.o:
	$(CC) -c $*.c -o $*.o -Wall $(CFLAGS)

install-bin:
	mkdir -p $(DESTDIR)$(BINDIR)/
	cp $(OUTFILE) $(DESTDIR)$(BINDIR)/$(OUTFILE)

uninstall-bin:
	rm -f $(BINDIR)/$(OUTFILE)

tar:
	version=$$(sed -n 's/#define ServerSide_VERSION_STRING[ \t]*"\([^"]*\)"/\1/p' < ServerSide.h) && \
	tar --create --transform "s#^#ServerSide-$${version}/#" "--file=ServerSide-$${version}.tar" --exclude-vcs -- *.c *.h configure Makefile ServerSiderc.example && \
	gzip --best < "ServerSide-$${version}.tar" > "ServerSide-$${version}.tar.gz" && \
	bzip2 --best < "ServerSide-$${version}.tar" > "ServerSide-$${version}.tar.bz2"