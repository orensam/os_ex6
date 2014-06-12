/**
 * Server side
 */

#include <limits.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <unordered_map>

#define SUCCESS 0
#define FAIL -1
#define EXIT_CODE 1
#define BUFFER_SIZE 1024
#define PORT_PARA_INDX 1

using namespace std;

static const string ERROR_PREFIX = "ERROR: ";
static const string GETHOSTBYNAME_ERROR = "gethostbyname() failed";
static const string SOCKET_ERROR = "socket() failed";
static const string BIND_ERROR = "bind() failed";
static const string LISTEN_ERROR = "listen() failed";
static const string READ_ERROR = "read() failed";
static const string WRITE_ERROR = "fwrite() failed";

int srv_port;
int srv_sock;

int _nameSize;
int _fileSize;
char* _fileToTransfer;
char* _fileToSave;

typedef struct hostent hostent;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct timeval timeval;

// Server definition
char srv_name[HOST_NAME_MAX+1];
hostent *srv_hostent;
sockaddr_in srv_addr;
int server_fd;
timeval tv;

// Client handling
fd_set fds;

struct Client
{
	//sockaddr_in cli_addr;

	string filename;
	unsigned int filename_size;
	unsigned int bytes_left;
	FILE * fp;
};

typedef struct Client Client;

static unordered_map<int, Client> clients;

unsigned int clilen = sizeof(sockaddr_in);
vector<int> finished_fds;

int error(string msg)
{
	cerr << ERROR_PREFIX << msg << endl;
	return FAIL;
}

int reset_fds()
{
	cout << "in reset_fds()" << endl;
	int max;
	FD_ZERO(&fds);
	FD_SET(server_fd, &fds);
	max = server_fd +1;

	unordered_map<int, Client>::iterator it;

	for(it = clients.begin(); it != clients.end(); ++it)
	{
		FD_SET(it->first, &fds);
		if (it->first >= max)
		{
			max = it->first + 1;
		}
	}
	return max;
}

int read_from_sock(int sock, unsigned int n_bytes, char* buf)
{
    unsigned int bytes_read = 0;
    int result;

    while (bytes_read < n_bytes)
    {
    	void* temp = buf + bytes_read;
        result = read(sock, temp, n_bytes-bytes_read);
        if (result < 1 )
        {
            return error(READ_ERROR);
        }
        bytes_read += result;
    }
    return bytes_read;
}

int accept_new_connection()
{
	int to_read;
	char* buf;
	Client cli;

	int cfd = accept(server_fd, (sockaddr*) &srv_addr, &clilen);

	// Read name size
	to_read = sizeof(int);
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return FAIL;
	cli.filename_size = to_read = *((int*) buf) + 1;
	free(buf);

	// Read filename
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return FAIL;
	cli.filename = buf;
	free(buf);

	// Read file size
	to_read = sizeof(int);
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return FAIL;
	cli.bytes_left = *((int*) buf);
	free(buf);

	cli.fp = fopen(cli.filename.c_str(), "wb");
	clients[cfd] = cli;

	//cout << "finished accept_con(). filename_size: " << cli.filename_size << ", filename: " << cli.filename << ", filesize: " << cli.bytes_left << endl;

	return cfd;
}


int read_client_file(int cfd, Client& cli)
{
	unsigned int to_read = min<unsigned int>(cli.bytes_left, BUFFER_SIZE);

	char* buf = (char*) malloc(to_read);

	//cout << "trying to read file. requested " << to_read << " bytes" << endl;


	if (read_from_sock(cfd, to_read, buf) < 0)
	{
		//cout << "error in read" << endl;
		return FAIL;
	}

	string sbuf(buf, to_read);
	//cout << "buffer is: " << sbuf << endl;

	unsigned int written = fwrite(buf, sizeof(char), to_read, cli.fp);

	if (written < to_read)
	{
		return error(WRITE_ERROR);
	}
	free(buf);
	cli.bytes_left -= to_read;

	//cout << "bytes left now: " << cli.bytes_left << endl;

	if (cli.bytes_left == 0)
	{
		// Finished reading file
		fclose(cli.fp);
		finished_fds.push_back(cfd);
	}

	return SUCCESS;
}

void reset_tv()
{
	tv.tv_sec = 2;
	tv.tv_usec = 0;
}

int handle_connections()
{
	//cout << "starting to handle connections" << endl;
	int max, cfd;


	while(true)
	{
		max = reset_fds();

		reset_tv();

		if (select(max, &fds, NULL, NULL, &tv) > 0)
	    {
	    	if (FD_ISSET(server_fd, &fds))
	    	{
	    		cfd = accept_new_connection();
	    		if (cfd < 0)
	    		{
	    			//ERROR
	    		}
	    	}
	    	unordered_map<int, Client>::iterator it;

	    	for(it = clients.begin(); it != clients.end(); ++it)
	    	{
	    		if FD_ISSET(it->first, &fds)
				{
	    			if (read_client_file(it->first, it->second) < 0)
					{
	    				return FAIL;
					}
				}
	    	}
	    	vector<int>::iterator vit = finished_fds.begin();
	    	for(;vit != finished_fds.end();++vit)
	    	{
	    		clients.erase(*vit);
	    	}
	    	finished_fds.clear();
	    }
	}
	return SUCCESS;
}

int establish(unsigned short port)
{
	cout << "establishing connection" << endl;
	int sfd;

	gethostname(srv_name, HOST_NAME_MAX);

	srv_hostent = gethostbyname(srv_name);

	if (srv_hostent == NULL)
	{
		return error(GETHOSTBYNAME_ERROR);
	}

	// sockaddr_in filling
	memset(&srv_addr, 0, sizeof(sockaddr_in));
	srv_addr.sin_family = srv_hostent->h_addrtype;
	memcpy(&srv_addr.sin_addr, srv_hostent->h_addr, srv_hostent->h_length); /* this is our host address */
	srv_addr.sin_port= htons(port); /* this is our port number */

	// Create socket
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		return error(SOCKET_ERROR);
	}

	// And bind it
	if (bind(sfd, (sockaddr*) &srv_addr, sizeof(sockaddr_in)) < 0)
	{
		close(sfd);
		return error(BIND_ERROR);
	}

	if (listen(sfd, SOMAXCONN) < 0)
	{
		return error(LISTEN_ERROR);
	}
	return sfd;
}

int main(int argc, char** argv)
{
	srv_port = atoi(argv[PORT_PARA_INDX]);
	cout << "Opening server with port " << srv_port << endl;
	if ((server_fd = establish(srv_port)) < 0)
	{
		cout << "establish error" << endl;
		exit(EXIT_CODE);
	}
	handle_connections();
}






