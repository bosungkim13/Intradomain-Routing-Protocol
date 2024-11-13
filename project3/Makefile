CC = g++
COPTS = -g -O0 -Wall -std=c++11
LKOPTS =

OBJS =\
	Event.o\
	Link.o\
	Node.o\
	RoutingProtocolImpl.o\
	Simulator.o\
	LinkState.o\
	lsUtils.o\
	DistanceVector.o\
	dvUtils.o\
	sharedUtils.o

%.o: %.cc
	$(CC) $(COPTS) -c $< -o $@

all:	Simulator

Simulator: $(OBJS)
	$(CC) $(LKOPTS) -o Simulator $(OBJS)

$(OBJS): global.h
Event.o: Event.h Link.h Node.h Simulator.h
Link.o: Event.h Link.h Node.h Simulator.h
Node.o: Event.h Link.h Node.h Simulator.h
Simulator.o: Event.h Link.h Node.h RoutingProtocol.h Simulator.h 
RoutingProtocolImpl.o: RoutingProtocolImpl.h LinkState.h DistanceVector.h

LinkState.o: LinkState.h
lsUtils.o: lsUtils.h
sharedUtils.o: sharedUtils.h

DistanceVector.o: DistanceVector.h
dvUtils.o: dvUtils.h

clean:
	rm -f *.o Simulator

