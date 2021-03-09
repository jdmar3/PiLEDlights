# tool macros
CC := gcc
CFLAGS := -Wall -O3 -pthread -lpigpio -lrt

# default rule
default: all

# phony rules
.PHONY: all
all: netledPi actledPi hddledPi multiactledPi

netledPi:
	$(CC) $(CFLAGS) -o netledPi netledPi.c

hddledPi:
	$(CC) $(CFLAGS) -o hddledPi hddledPi.c

actledPi:
	$(CC) $(CFLAGS) -o actledPi actledPi.c

multiactledPi:
	$(CC) $(CFLAGS) -o multiactledPi multiactledPi.c

.PHONY: clean
clean:
	@rm -f *.o netledPi hddledPi actledPi multiactledPi
