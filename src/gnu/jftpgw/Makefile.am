SUBDIRS = support
AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = $(man_MANS) jftpgw.conf.sample.in rel2abs.c snprintf.c ChangeLog README

INCLUDES = @all_includes@

man_MANS = jftpgw.1

bin_PROGRAMS = jftpgw

jftpgw_SOURCES = active.c bindport.c cmds.c config.c \
		 jftpgw.c log.c login.c openport.c \
		 passive.c util.c ftpread.c std_cmds.c  \
		 states.c cache.c rel2abs.c fw_auth_cmds.c

jftpgw_LDFLAGS = @all_libraries@


LIBS = @LIBS@

#LIBTOOL_DEPS = @LIBTOOL_DEPS@
#libtool: $(LIBTOOL_DEPS)
#	(SHELL) ./config.status --recheck


noinst_HEADERS = jftpgw.h log.h std_cmds.h cmds.h \
		 cache.h config_header.h fw_auth_cmds.h

VERSION = @JFTPGW_VERSION@

# just install datas here. Use install-exec-data for scripts and etc.
# the binary itself is already installed from automake
# use mkinstalldirs, not "install -d"
# don't install a list of file. Just one file per install.
# if you have more of them, create a subdirectory with an extra Makefile
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(CONFPATH)
	if test -f $(sysconfdir)/jftpgw.conf; then echo -e "\n\nMoving $(sysconfdir)/jftpgw.conf   to   $(sysconfdir)/jftpgw.conf.old\n\n"; mv $(sysconfdir)/jftpgw.conf $(sysconfdir)/jftpgw.conf.old; fi
	$(INSTALL_DATA) jftpgw.conf.sample $(sysconfdir)/jftpgw.conf

# remove ALL you have installed in install-data-local or install-exec-local
uninstall-local:
	rm @CONFFILE@
