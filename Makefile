all: run

run: main.cpp
	g++ -Wall -Werror `pkg-config --libs --cflags gstreamer-0.10 liblo` -o run main.cpp

clean:
	rm -rf run

