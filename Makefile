CC=gcc
PREFIX = /usr/local

OPT= -O2 -Wall -fPIC  -shared -g


CFLAGS  = -I$(PREFIX)/include/luajit-2.1 


LDFLAGS += -L/usr/lib -lluajit-5.1 


CFLAGS += -I$(PREFIX)/include
LDFLAGS += -L$(PREFIX)/lib -lcurl 

MOD = xcurl
MOD_SO = $(MOD).so

SRCS  =		$(MOD).c 



all: $(MOD_SO)

$(MOD_SO): $(SRCS)
	@echo CC $@
	@$(CC) $(OPT)  $(CFLAGS) -o $@ $^ $(LDFLAGS) 

test: $(MOD_SO)
	luajit test.lua

clean:
	@rm -fv $(MOD_SO)