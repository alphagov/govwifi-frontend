TARGETNAME	:= rlm_govlogger

ifneq "$(TARGETNAME)" ""
TARGET		:= $(TARGETNAME).a
endif

SOURCES		:= $(TARGETNAME).c json.c

SRC_CFLAGS      :=  -I$(top_builddir)/src/modules/rlm_eap -I$(top_builddir)/src/modules/rlm_eap/libeap -I$(top_builddir)/src/modules/rlm_eap/libeap
TGT_LDLIBS      :=  -ljson-c

