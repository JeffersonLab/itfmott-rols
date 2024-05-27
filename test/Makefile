#
# File:
#    Makefile
#
# Description:
#    Makefile for the TI test programs
#
DEBUG	?= 1
QUIET	?= 1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

ARCH	?= $(shell uname -m)
OS	?= LINUX

ifdef CODA_VME
CODA_VME_INC = -I${CODA_VME}/include
endif
ifdef CODA_VME_LIB
CODA_LIB = -L${CODA_VME_LIB}
endif

# linuxvme defaults, if they're not already defined
LINUXVME_INC	?= ../../include
LINUXVME_LIB	?= ../../lib
LINUXVME_BIN	?= ../../bin

CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)gcc
AR                      = ar
RANLIB                  = ranlib
INCS			= -I. -I${LINUXVME_INC} ${CODA_VME_INC}
CFLAGS			= -L. -L${LINUXVME_LIB} ${CODA_LIB}
CFLAGS			+= -lstdc++ -lrt -ljvme -lsd -lti -lts -ltd -lfadc -lhd -lconfig

ifeq ($(DEBUG),1)
	CFLAGS		+= -Wall -g -Wno-unused
endif

SRC			= $(wildcard *.c)
OBJ			= $(SRC:.c=.o)
PROGS			= $(SRC:.c=)

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
DEPFILES := $(SRC:%.c=$(DEPDIR)/%.d)

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(INCS) $(CPPFLAGS) $(TARGET_ARCH)

all: $(PROGS)

clean distclean:
	@rm -f $(PROGS) *~ $(OBJS) $(DEPFILES)

%: %.c
%: %.c $(DEPDIR)/%.d | $(DEPDIR)
	@echo " CC     $@"
	${Q}$(COMPILE.c) -o $@ $<

$(DEPDIR): ; @mkdir -p $@

$(DEPFILES):
include $(wildcard $(DEPFILES))

.PHONY: all clean distclean
