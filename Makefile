all: debug release

debug:
	$(MAKE) $(MAKEFLAGS) -f Makefile.debug

release:
	$(MAKE) $(MAKEFLAGS) -f Makefile.release

clean:
	$(MAKE) $(MAKEFLAGS) -f Makefile.debug clean
	$(MAKE) $(MAKEFLAGS) -f Makefile.release clean
