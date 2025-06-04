CXX = g++
CXXFLAGS = -std=c++11 -g -Wall -Werror -pedantic-errors -DNDEBUG -pthread

SRCS = main.cpp bank.cpp account.cpp atm.cpp
OBJS = $(SRCS:.cpp=.o)

TARGET = bank

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o log.txt
