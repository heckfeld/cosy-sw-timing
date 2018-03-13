
ACLOCAL_AMFLAGS = -I m4

init_ddir = $(sysconfdir)/init.d
defaultdir = $(sysconfdir)/default

dist_init_d_SCRIPTS = init.d/sw_timing
dist_default_DATA = default/sw_timing

sbin_PROGRAMS = sw_timing

sw_timing_SOURCES = sw_timing.c read_tab.c tpopen.c udp_open.c sgetline.c
include_HEADERS = sw_timing.h

