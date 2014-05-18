CC=g++
CXXFLAGS=-Wall -Wextra -g -O3 -funroll-loops
LXXFLAGS=-lboost_thread
TARGET=sudo-ku
OBJECTS= \
	Main.o \
	Grid.o \
	Timer.o
PPFLAGS=-DNDEBUG -DENABLE_MULTITHREADING

default:
	make objects
	make target

Main.o: Main.cpp
	$(CC) $(CXXFLAGS) $(PPFLAGS) -c   -o $@    $<
Grid.o: Grid.cpp
	$(CC) $(CXXFLAGS) $(PPFLAGS) -c   -o $@    $<
Timer.o: Timer.cpp
	$(CC) $(CXXFLAGS) $(PPFLAGS) -c   -o $@    $<

objects: $(OBJECTS)
target:
	## for some reason, need to specify
	## linker flags *after* the objects
	## or undefined references result
	$(CC) -o $(TARGET) $(OBJECTS) $(LXXFLAGS)

clean:
	rm -rf *.o $(TARGET)

tarball:
	tar -czf $(TARGET).tar.gz   *.cpp *.hpp makefile sudokus/

