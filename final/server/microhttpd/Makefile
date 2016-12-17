### DEFINITIONS

-include Makefile.settings

all: $(OUTFILE)
install: install-bin
uninstall: uninstall-bin

### MAIN PROGRAM

$(OUTFILE): fileserver.o connection.o reason_phrase.o daemon.o internal.o memorypool.o mhd_mono_clock.o sysfdsetsize.o mhd_str.o mhd_threads.o mhd_sockets.o mhd_itc.o response.o mhd_compat.o tsearch.o postprocessor.o digestauth.o md5.o basicauth.o base64.o test_postprocessor.o test_postprocessor_amp.o test_postprocessor_large.o test_shutdown_select.o connection_https.o
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