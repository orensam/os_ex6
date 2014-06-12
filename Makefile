# Makefile for OS Project 6: Sockets

TAR = ex6.tar
TAR_CMD = tar cvf
CC = g++ -std=c++11 -Wall

all: clftp srftp

clftp: clftp.cpp
	$(CC) clftp.cpp -o clftp

srftp: srftp.cpp
	$(CC) srftp.cpp -o srftp
	
clean:
	rm -f $(TAR) srftp clftp

tar: clftp.cpp srftp.cpp Makefile README
	$(TAR_CMD) $(TAR) clftp.cpp srftp.cpp Makefile README