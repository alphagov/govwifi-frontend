TARGET		:= govlogger_unit

SOURCES		:= $(TARGET).c ${top_srcdir}/src/modules/rlm_govlogger/json.c ${top_srcdir}/src/main/modules.c ${top_srcdir}/src/main/mainconfig.c ${top_srcdir}/src/main/modcall.c

TGT_PREREQS	:= libfreeradius-server.a libfreeradius-radius.a
TGT_LDLIBS	:= $(LIBS) -ljson-c

SRC_CFLAGS      += -I$(top_builddir)/src/modules/rlm_eap -I$(top_builddir)/src/modules/rlm_eap/libeap -I$(top_builddir)/src/modules/rlm_eap/libeap

