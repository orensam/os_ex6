# Makefile for OS Project 6: Reliable File Transfer

TAR = ex6.tar
TAR_CMD = tar cvf
CC = g++ -std=c++11 -Wall

all: clftp srftp

clftp: clftp.cpp ftp.h
	$(CC) clftp.cpp -o clftp

srftp: srftp.cpp ftp.h
	$(CC) srftp.cpp -o srftp
	
clean:
	rm -f $(TAR) srftp clftp

tar: clftp.cpp srftp.cpp ftp.h Makefile README
	$(TAR_CMD) $(TAR) clftp.cpp srftp.cpp ftp.h Makefile README