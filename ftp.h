/*
 * ftp.h
 * Header file for clftp.cpp and srftp.cpp
 *  Created on: Jun 16, 2014
 *      Author: orensam
 */

#ifndef FTP_H_
#define FTP_H_

// General includes
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

// Socket handling includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Constants
#define SUCCESS 0
#define FAIL -1
#define EXIT_CODE 1
#define BUFFER_SIZE 1024
static const std::string ERROR_PREFIX = "ERROR: ";
static const std::string MALLOC_ERROR = "Failed to allocate memory";

typedef struct hostent hostent;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct timeval timeval;

#endif /* FTP_H_ */
