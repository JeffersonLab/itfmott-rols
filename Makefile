#
# File:
#    Makefile
#
# Description:
#    Makefile for the coda primary and secondary readout lists
#    running on an Intel-based controller running Linux
#
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
QUIET=1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

# Plug in your primary readout lists here.. CRL are found automatically
VMEROL			= event_list.so ti_list.so uitf_list.so
# Add shared library dependencies here.  (jvme, ti, are already included)
ROLLIBS			= -ldalmaRol -lfadc -lhd -lconfig

ifdef CODA_VME
INC_CODA_VME	= -isystem${CODA_VME}/include
endif

ifdef CODA_VME_LIB
LIB_CODA_VME	= -L${CODA_VME_LIB}
endif

LINUXVME_LIB	?= .
LINUXVME_INC	?= .

# DEFs for compiling primary readout lists
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -Wno-unused -g
else
CFLAGS			= -O3
endif
CFLAGS			+= -DLINUX -DDAYTIME=\""`date`"\"

INCS			= -I. -I${LINUXVME_INC} ${INC_CODA_VME} \
				-isystem${CODA}/common/include
CFLAGS			+= -L. -L${LINUXVME_LIB} ${LIB_CODA_VME} -DJLAB \
				-lrt -lpthread -ljvme -lti $(ROLLIBS)

# DEFs for compiling CODA readout lists
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I. -I${LINUXVME_INC} ${INC_CODA_VME} -isystem${CODA}/common/include
CODA_LIBDIRS            = -L.
CODA_LIBS		=
CODA_DEFS		= -DLINUX -DDAYTIME=\""`date`"\"
ifdef DEBUG
CODA_CFLAGS		= -Wall -Wno-unused -g
else
CODA_CFLAGS		= -O
endif
CODA_CFLAGS		+= -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
DEPFILES := $(VMEROL:%.so=$(DEPDIR)/%.d)
DEPFILES += $(CFILES:%.c=$(DEPDIR)/%.d)

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(INCS) $(CPPFLAGS) $(TARGET_ARCH)

all:  $(VMEROL) $(SOBJS)

%.c: %.crl
	@echo " CCRL   $@"
	${Q}${CCRL} $<

%.so: %.c
%.so: %.c $(DEPDIR)/%.d | $(DEPDIR)
	@echo " CC     $@"
	${Q}$(COMPILE.c) -fPIC -shared  \
		-DTI_MASTER -DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

clean distclean:
	${Q}rm -f  $(VMEROL) $(SOBJS) $(CFILES) *~ $(DEPS) $(DEPS) *.d.*

$(DEPDIR): ; @mkdir -p $@

$(DEPFILES):
include $(wildcard $(DEPFILES))

.PHONY: all clean distclean
