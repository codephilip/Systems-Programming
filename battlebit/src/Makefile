CXX = gcc
CXXFLAGS = -g -pthread

binaries=main 

all: clean $(binaries)

.PHONY : clean
clean :
	rm -f $(binaries) *.o

main: main.o game.o repl.o char_buff.o server.o
	$(CXX) $(CXXFLAGS) -o main main.o game.o repl.o char_buff.o server.o 

game.o: game.c
	$(CXX) $(CXXFLAGS) -c game.c

repl.o: repl.c
	$(CXX) $(CXXFLAGS) -c repl.c

char_buff.o: char_buff.c
	$(CXX) $(CXXFLAGS) -c char_buff.c

server.o: server.c
	$(CXX) $(CXXFLAGS) -c server.c

main.o: main.c
	$(CXX) $(CXXFLAGS) -c main.c
