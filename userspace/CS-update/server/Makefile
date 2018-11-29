.PHONY: all clean

TARGET = power-update-server
OBJS = main.o

all: $(TARGET) Makefile

%.o: %.c $(wildcard *.h)
	$(CC) -c -Wall -Wextra -pedantic -std=gnu99 -g $(CFLAGS) $(CPPFLAGS) -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJS)


clean:
	rm -f $(TARGET) $(OBJS)

ifneq (,$(wildcard ../../lint-config))
LINT_SOURCES_file-trx-server:=$(OBJS:.o=.c)
LINT_C_PROGS:=file-trx-server
LINT_CC:=arm-atlas-linux-gnueabi-gcc
LINT_CXX:=arm-atlas-linux-gnueabi-g++
include ../../lint-config/include.mk
clean: lint_clean
endif
