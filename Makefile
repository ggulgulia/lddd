.PHONY: all clean

SUBDIRS := $(shell find . -mindepth 2 -type f -name Makefile -size +0c -printf '%h\n' | sort)

# Skip empty placeholder Makefiles until they define build targets.
all clean:
	@set -e; for dir in $(SUBDIRS); do \
		$(MAKE) -C "$$dir" $@; \
	done
