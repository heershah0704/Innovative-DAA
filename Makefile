# Linux / macOS:  make        then  ./server
# Windows (MinGW): see README for the one-line g++ command.
CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

all: server tests

server: backend/src/*.hpp backend/src/main.cpp
	$(CXX) $(CXXFLAGS) -o server backend/src/main.cpp

tests: backend/src/*.hpp backend/tests/tests.cpp
	$(CXX) $(CXXFLAGS) -I backend/src -o tests backend/tests/tests.cpp

test: tests
	./tests

clean:
	rm -f server tests server.exe tests.exe
