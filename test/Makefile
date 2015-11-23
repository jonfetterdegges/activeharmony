HARMONY_HOME ?= $(HOME)/bin/harmony

driver: driver.c
	$(CC) -o driver driver.c -I$(HARMONY_HOME)/include -L$(HARMONY_HOME)/lib -lharmony $(CFLAGS)

clean:
	rm driver
