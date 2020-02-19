PROG=example
CC=gcc
CFLAGS= -Wall -Wextra -Wno-unused-function -pedantic -O3
LLIBS=-L../ -lppmtools -L/usr/lib/x86_64-linux-gnu/ -lgsl -lgslcblas -lm
OBJ = $(PROG).o

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) -o $@ $^ $(LLIBS)
	
%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<
	
clean:
	@$(RM) -vrf $(PROG) $(OBJ) *.dat
