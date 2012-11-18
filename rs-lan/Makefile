PROJ = rs-lan

include ../Makedefs

SRCS =  rs-lan.c conf.c
OBJS =  $(SRCS:.c=.o)
LIBS =  $(LIBDIR)/nutinit.o $(LIBDIR)/arm-da.o $(LIBDIR)/arm-pfa.o $(LIBDIR)/arm-swi.o $(LIBDIR)/arm-udf.o -lnutpro -lnutnet -lnutos -lnutcrt -lnutdev -lnutarch -lnutgorp
TARG =  $(PROJ).hex

all: $(OBJS) $(TARG) $(ITARG) $(DTARG)

include ../Makerules

clean:
	-rm -f $(OBJS)
	-rm -f $(TARG) $(ITARG) $(DTARG)
	-rm -f $(PROJ).eep
	-rm -f $(PROJ).obj
	-rm -f $(PROJ).map
	-rm -f $(SRCS:.c=.lst)
	-rm -f $(SRCS:.c=.bak)
	-rm -f $(SRCS:.c=.i)
	-rm -f $(SRCS:.c=.d)
