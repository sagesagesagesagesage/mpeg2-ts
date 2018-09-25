CC := gcc
CFLAGS := -g -Wall -I../inc
LDLIBS := -lm

all: ts_base ts_tot_spliter



ts_tot_spliter: spliter/ts_tot_spliter.c
	cd spliter; $(CC) -o ../ts_tot_spliter $(CFLAGS) $(LDLIBS) ts_tot_spliter.c

ts_base: base/ts.c
	cd base; $(CC) -o ../ts_base $(CFLAGS) $(LDLIBS) ts.c

clean:
	$(RM) *.o
	$(RM) base/*.o
	$(RM) spliter/*.o
	$(RM) ts_base
	$(RM) ts_tot_spliter




