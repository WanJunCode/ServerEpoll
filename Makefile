.PHONY: all

CXX = g++ -g -std=c++11
LDFLAGS = -pthread
EXECUTABLE = wj-server

all: ${EXECUTABLE} origin

${EXECUTABLE} : main.o EpollService.o Tool.o
	$(CXX) $^ $(LDFLAGS) -o $@ -g 

origin: origin.o
	$(CXX) $^ $(LDFLAGS) -o $@ 

clean:
	rm -rf ${EXECUTABLE} origin *.o core
