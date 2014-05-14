all: proxy

proxy: proxy.o
	g++ proxy.o -o proxy

proxy.o: proxy.cpp
	g++ -Wall -g -c proxy.cpp

clean:
	rm -f *~ *.o proxy

