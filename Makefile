CC := g++
CFLAGS := -std=c++17 -Werror -Wextra -Wall -pedantic
SOURCES := Main.cpp OrderBook.cpp
OBJS := $(SOURCES:.cpp=.o)
EXECUTABLE := order_book

all: order_book_build

order_book_build: $(OBJS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJS)

.cpp.o:
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) $(OBJS) $(EXECUTABLE)

.PHONY = all clean order_book_build

