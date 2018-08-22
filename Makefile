
output: main.o
	g++ main.o -o client -std=gnu++11 -lpthread -lboost_system -lboost_iostreams

main.o: main.cpp
	g++ -c main.cpp 

clean:
	rm *.o client