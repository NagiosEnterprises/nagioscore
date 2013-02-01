###################################
# Makefile for NEB examples
#
###################################


# Source code directories
SRC_PING_WORKER=./ping

default:
		@echo "Please supply a command line argument (i.e. 'make all').  Other targets are:"
		@echo "   ping-worker"
		@echo "   clean distclean"
		@echo "   install-all install-ping"

ping-worker:
	cd $(SRC_PING_WORKER) && $(MAKE) all
	
all:
	cd $(SRC_PING_WORKER) && $(MAKE) all

clean:
	cd $(SRC_PING_WORKER) && $(MAKE) $@
	rm -f *~ *.*~

distclean: clean
	cd $(SRC_PING_WORKER) && $(MAKE) $@
	rm -f Makefile

devclean: distclean

install-ping:
	cd $(SRC_PING_WORKER) && $(MAKE) install
	
install-all: install-ping

