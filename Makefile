all: proxy

proxy: proxy.o
	g++ proxy.o -o proxy -lpthread

proxy.o: proxy.cpp
	g++ -Wall -g -c proxy.cpp

clean:
	rm -f *~ *.o proxy

