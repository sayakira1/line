CC = g++
CFLAGS = -g -Wall
CFLAGS += `pkg-config opencv4 --cflags --libs`
DXLFLAGS = -I/usr/local/include/dynamixel_sdk_cpp #헤더파일경로 지정
DXLFLAGS += -ldxl_x64_cpp #라이브러리파일 지정,
SRCS = main.cpp
TARGET = maintest
OBJS = main.o dxl.o
.PHONY: all clean
$(TARGET) : $(OBJS)
		$(CC) -o $(TARGET) $(OBJS) $(DXLFLAGS) $(CFLAGS)
main.o : main.cpp
		$(CC) -c main.cpp $(DXLFLAGS) $(CFLAGS)
dxl.o : dxl.hpp dxl.cpp
		$(CC) -c dxl.cpp $(DXLFLAGS) $(CFLAGS)
all: $(TARGET)
clean:
	rm -f $(OBJS) $(TARGET)