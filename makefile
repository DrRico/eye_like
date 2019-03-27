all:
	g++ -Wall -o server main.cpp -I../ TCPServer.cpp -std=c++11 -lpthread
