CC=/usr/bin/gcc
LD=/usr/bin/g++
AR=ar
CFLAGS=-Wall -O3 -fPIC
LIBS=-lz
ARFLAGS=rcs

LIB_FILE = lib.a
PERL_EXT = ext/Zat.o

MAN1_DIR = /usr/local/share/man/man1/
EXEC_DIR = /usr/local/bin/

O_FILES = \
	zatbuf_node_push.o \
	zatbuf_out.o \
	zatbuf_dump.o \
	zatbuf_crc.o \
	zat_deflate.o \
	zat_exec.o \
	zat_stream_zip.o \
	zat_stream_mat.o \
	zat_stream_png.o \
	zat_detect_stream.o \
	zat_noflate.o \
	zat_out.o \
	zat_outbuf.o \
	zat_pipe.o \
	zat_pipinflate.o \
	zat_read.o \
	zat_reinflate.o \
	zat_refill.o

all: bin/zat $(PERL_EXT) git-zat.1 zat.1

clean:
	rm -rf *.o bin/zat $(LIB_FILE) git-zat.1 zat.1
	@if [ -f ext/Makefile ] ; then (cd ext ; make realclean ) ; fi

install: install-zat install-git-zat install-ext

install-zat: zat.1 $(MAN1_DIR)
	install -m 755 bin/zat $(EXEC_DIR)zat
	install -m 644 zat.1 /$(MAN1_DIR)

install-git-zat: install-ext git-zat.1 $(MAN1_DIR)
	install -m 755 git-zat.perl $(EXEC_DIR)git-zat
	install -m 644 git-zat.1 $(MAN1_DIR)

$(MAN1_DIR):
	install -d $@

git-zat.1: git-zat.adoc
	asciidoctor -b manpage git-zat.adoc

zat.1: zat.adoc
	asciidoctor -b manpage zat.adoc

install-ext: ext/Makefile
	$(MAKE) -C ext install

%.o : src/%.c
	$(CC) -c $(CFLAGS) -o $@ -Isrc -Iext $^

bin/zat:	main.o $(LIB_FILE)
	mkdir -p bin/
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

ext/Makefile: $(PERL_EXT)

$(PERL_EXT):	ext/Makefile.PL $(LIB_FILE)
	(cd ext ; perl Makefile.PL && make CC=$(CC) LD=$(LD))

$(LIB_FILE):	$(O_FILES)
	$(AR) $(ARFLAGS) $@ $^

