all: debug release

debug:
	$(MAKE) -f Makefile.debug

release:
	$(MAKE) -f Makefile.release

clean combined:
	$(MAKE) -f Makefile.debug $@
	$(MAKE) -f Makefile.release $@
