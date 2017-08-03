all:
	g++ asmmisc.cpp -Os -std=c++11 -o asmmisc
	strip asmmisc -o asmmisc
