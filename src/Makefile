
LINUX_FLAGS=-lpthread -lc -lm
SOLARIS_FLAGS=-lthread -lc -lm -lnsl -lsocket -lrt
SPECIFIC_FLAGS=$($(PLATFORM)_FLAGS)

LIB330_DIR = ../lib330
LIBMSEED_DIR = ../libmseed
LIBDALI_DIR = ../libdali

BINDIR = ..
LIBDIR = lib

CFLAGS = $(GLOBALFLAGS) -I$(LIB330_DIR) -I${LIBMSEED_DIR} -I${LIBDALI_DIR} -I. -g
LDFLAGS = -L$(LIB330_DIR) -l330 -L${LIBMSEED_DIR} -lmseed -L${LIBDALI_DIR} -ldali  $(SPECIFIC_FLAGS)

SRCS = q3302dali.c config.c kom.c

OBJS = $(SRCS:%.c=%.o)


.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

all: q3302dali

q3302dali: $(OBJS)
	$(CC) $(GLOBALFLAGS) -o q3302dali $(OBJS) $(LDFLAGS)
	cp q3302dali $(BINDIR)

clean:
	rm -f *.o
	rm -f q3302dali

clean_bin:
	rm -f $(BINDIR)/q3302dali
