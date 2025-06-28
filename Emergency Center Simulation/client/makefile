all: clean compile link run

# Linking step
link:
	g++ -o bin/StompEMIClient bin/ConnectionHandler.o bin/event.o bin/StompClient.o bin/StompProtocol.o -lpthread

# Compilation step
compile:
	g++ -g -Wall -Weffc++ -std=c++17 -Iinclude -c -o bin/ConnectionHandler.o src/ConnectionHandler.cpp
	g++ -g -Wall -Weffc++ -std=c++17 -Iinclude -c -o bin/event.o src/event.cpp
	g++ -g -Wall -Weffc++ -std=c++17 -Iinclude -c -o bin/StompClient.o src/StompClient.cpp
	g++ -g -Wall -Weffc++ -std=c++17 -Iinclude -c -o bin/StompProtocol.o src/StompProtocol.cpp

# Cleaning step
clean:
	rm -f bin/*

# Run the compiled program
run:
	./bin/StompEMIClient

	