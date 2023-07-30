all: debug release

debug:
	$(MAKE) $(MAKEFLAGS) -f Makefile.debug

release:
	$(MAKE) $(MAKEFLAGS) -f Makefile.release

clean combined:
	$(MAKE) $(MAKEFLAGS) -f Makefile.debug $@
	$(MAKE) $(MAKEFLAGS) -f Makefile.release $@
