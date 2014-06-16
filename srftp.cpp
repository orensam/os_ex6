/**
 * srftp.cpp - A server-side program for a simple file-transfer protocol.
 * Created by Yoni Herzog & Oren Samuel
 */

#include "ftp.h"

#include <limits.h>

#include <vector>
#include <unordered_map>

#define BUFFER_SIZE 1024
#define PORT_PARA_IDX 1
#define TIMEOUT_SEC 3

using namespace std;

static const string ERROR_PREFIX = "ERROR: ";
static const string GETHOSTNAME_ERROR = "Failed retrieving hostname";
static const string GETHOSTBYNAME_ERROR = "Failed retrieving host info";
static const string SOCKET_ERROR = "Failed creating socket";
static const string BIND_ERROR = "Failed binding socket to port";
static const string LISTEN_ERROR = "Failed listening to port";
static const string READ_ERROR = "Failed reading data from socket";
static const string WRITE_ERROR = "Failed writing to file";
static const string CLIENT_FD_ERROR = "Failed retrieving client FD";
static const string ACCEPT_ERROR = "Failed accepting connection";
static const string READ_FILE_ERROR = "Failed reading file from client";
static const string READ_NAME_SIZE_ERROR = "Failed reading filename size";
static const string READ_FILENAME_ERROR = "Failed reading filename";
static const string READ_FILE_SIZE_ERROR = "Failed reading file size";

int srv_port;
int srv_sock;

typedef struct hostent hostent;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct timeval timeval;

// Server definition
char srv_name[HOST_NAME_MAX+1];
hostent *srv_hostent;
sockaddr_in srv_addr;
int server_fd;
timeval timeout;

// Client handling

/**
 * A struct that represents a single client.
 * Saves the clients filename and file pointer,
 * and a counter for the number of bytes left to receive.
 */
struct Client
{
	string filename;
	unsigned int bytes_left;
	FILE * fp;
};
typedef struct Client Client;

// The list of active clients.
static unordered_map<int, Client> clients;

// The list of clients that finished their transfer
vector<int> finished_fds;

// The FD set for the clients
fd_set fds;

// Length of client aockaddr
unsigned int clilen = sizeof(sockaddr_in);

/**
 * Ouputs the given message as an error to stderr, 
 * and returns a failure code.
 */
int error(string msg)
{
	cerr << ERROR_PREFIX << msg << endl;
	return FAIL;
}

/**
 * Resets the FD set according to the list of active clients.
 */
int reset_fds()
{
	cout << "in reset_fds()" << endl;
	int max;
    
    // Delete the existing FDs, and add the server's FD
	FD_ZERO(&fds);
	FD_SET(server_fd, &fds);
	max = server_fd +1;
    
    // Iterate the active clients and add them to the FD set
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

/**
 * Read from the socket sock into the buffer buf,
 * until n_bytes bytes are read.
 */
int read_from_sock(int sock, unsigned int n_bytes, char* buf)
{
    unsigned int bytes_read = 0;
    int result;

    while (bytes_read < n_bytes)
    {
        // Still need to read
    	void* temp = buf + bytes_read;
        result = read(sock, temp, n_bytes-bytes_read);
        if (result < 1 )
        {            
        	free(buf);
            return error(READ_ERROR);
        }
        bytes_read += result;
    }
    return bytes_read;
}

/**
 * Accepts a new connection from the socket, reads the metadata from the client,
 * and adds the client to the list of active clients.
 */
int accept_new_connection()
{
	int cfd, to_read;
	char* buf;
	Client cli;

	if ((cfd = accept(server_fd, (sockaddr*) &srv_addr, &clilen) < 0))
	{
		return error(ACCEPT_ERROR);
	}

	// Read name size
	to_read = sizeof(int);
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return error(READ_NAME_SIZE_ERROR);
	to_read = *((int*) buf) + 1; // Now to_read is the filename size
	free(buf);

	// Read filename
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return error(READ_FILENAME_ERROR);
	cli.filename = buf; // Now we know the client's destination filename
	free(buf);

	// Read file size
	to_read = sizeof(int);
	buf = (char*) malloc(to_read);
	if (read_from_sock(cfd, to_read, buf) < 0) return error(READ_FILE_SIZE_ERROR);
	cli.bytes_left = *((int*) buf); // Now we know we how many file bytes we need to read.
	free(buf);
    
    // Open the file, add the client to the active client list and return the client's FD
	cli.fp = fopen(cli.filename.c_str(), "wb");
	clients[cfd] = cli;

	//cout << "finished accept_con(). filename_size: " << cli.filename_size << ", filename: " << cli.filename << ", filesize: " << cli.bytes_left << endl;

	return cfd;
}

/**
 * Reads a chunk of data from the file being
 * sent from the client.
 */
int read_client_file(int cfd, Client& cli)
{
	unsigned int to_read = min<unsigned int>(cli.bytes_left, BUFFER_SIZE);

	char* buf = (char*) malloc(to_read);

	//cout << "trying to read file. requested " << to_read << " bytes" << endl;
	if (read_from_sock(cfd, to_read, buf) < 0)
	{
		//cout << "error in read" << endl;
        return error(FILE_READ_ERROR);
	}
    
	//string sbuf(buf, to_read);
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

/**
 * Resets the select timeout.
 */
void reset_timeout()
{
	timeout.tv_sec = TIMEOUT_SEC;
	timeout.tv_usec = 0;
}

/**
 * Removes the clients whose file we finished receiving
 * from the list of active clients.
 */
void remove_finished_clients()
{
	vector<int>::iterator vit;
	for(vit = finished_fds.begin(); vit != finished_fds.end(); ++vit)
	{
		clients.erase(*vit);
	}
	finished_fds.clear();
}

/**
 * Waits for new connections, accepts them, and retrieves the files
 * from the clients.
 * On error, terminates the server's operation and exits with code != 0.
 */
int handle_connections()
{
	//cout << "starting to handle connections" << endl;
	int max, cfd;

	while(true)
	{
		max = reset_fds();
		reset_timeout();

		if (select(max, &fds, NULL, NULL, &timeout) > 0)
	    {
			// There's an FD that's ready for reading

			if (FD_ISSET(server_fd, &fds))
	    	{
				// Server FD is ready, i.e a new client is trying to send data.
				// We accept the connection and read the metadata
	    		if ((cfd = accept_new_connection()) < 0)
	    		{
	    			error(CLIENT_FD_ERROR);
	    			exit(EXIT_CODE);
	    		}
	    	}

			// Now check which client FDs are ready for reading
	    	unordered_map<int, Client>::iterator it;
	    	for(it = clients.begin(); it != clients.end(); ++it)
	    	{
	    		cfd = it->first;
	    		Client& cli = it->second;

	    		if FD_ISSET(cfd, &fds)
				{
	    			// Client is ready, read a file chunk
	    			if (read_client_file(cfd, cli) < 0)
					{
	    				error(READ_FILE_ERROR);
	    				exit(EXIT_CODE);
					}
				}
	    	}
            // Stop reading from the clients that we finished reading from
	    	remove_finished_clients();
	    }
	}
	return SUCCESS;
}

/**
 * Establishes the server.
 * Upon returning from this function, the server is ready
 * to accept connections from clients.
 */
int establish(unsigned short port)
{
//	cout << "establishing connection" << endl;
	int sfd;

	if (gethostname(srv_name, HOST_NAME_MAX) < 0)
	{
		error(GETHOSTNAME_ERROR);
		exit(EXIT_CODE);
	}

	if ((srv_hostent = gethostbyname(srv_name)) == NULL)
	{
		error(GETHOSTBYNAME_ERROR);
		exit(EXIT_CODE);
	}

	// Create the address struct
	memset(&srv_addr, 0, sizeof(sockaddr_in));
	srv_addr.sin_family = srv_hostent->h_addrtype;
	memcpy(&srv_addr.sin_addr, srv_hostent->h_addr, srv_hostent->h_length);
	srv_addr.sin_port= htons(port);

	// Create socket
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error(SOCKET_ERROR);
		exit(EXIT_CODE);
	}

	// Bind socket and port
	if (bind(sfd, (sockaddr*) &srv_addr, sizeof(sockaddr_in)) < 0)
	{
		close(sfd);
		error(BIND_ERROR);
		exit(EXIT_CODE);
	}

	// Start listening to the port
	if (listen(sfd, SOMAXCONN) < 0)
	{
		error(LISTEN_ERROR);
		exit(EXIT_CODE);
	}

	return sfd;
}

/**
 * Main function - establishes the server and then lets it
 * wait for connections.
 */
int main(int argc, char** argv)
{
	srv_port = atoi(argv[PORT_PARA_IDX]);
	server_fd = establish(srv_port);
	handle_connections();
}






