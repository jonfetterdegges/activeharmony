all_subsystems = build code-server example/client_api example/constraint example/code_generation example/synth

.PHONY: all install clean distclean doc

all install clean distclean:
	+@for subsystem in $(all_subsystems); do \
	    $(MAKE) -C $$subsystem $@;           \
	    RETVAL=$$?;                          \
	    if [ $$RETVAL != 0 ]; then           \
		exit $$RETVAL;                   \
	    fi;                                  \
	done

doc:
	$(MAKE) -C doc
