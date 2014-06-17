/**
 * clftp.cpp - Client side for a simple file-transfer protocol.
 * This program created by Netanel Zakay for OS course, 2013-2014, HUJI
 * Edited by Yoni Herzog & Oren Samuel
 */



//********************************* INCLUDES **********************************

#include "ftp.h"

#include <fstream>

using namespace std;

//********************************* VARIABLES *********************************

#define PORT_PARA_IDX 1
#define HOST_PARA_IDX 2
#define FILE_NAME_PARA_IDX 3
#define OUT_FILE_NAME_PARA_IDX 4

static const string OPEN_FILE_ERROR = "Failed opening source file";
static const string CONNECT_ERROR = "Failed connecting to server";
static const string SEND_ERROR = "Failed sending data to server";

int server_port;
int server_socket;
int name_size;
int file_size;
char* filename_to_transfer;
char* filename_to_save;

sockaddr_in serv_addr;
hostent *server = NULL;

//****************************** HELPER FUNCTIONS ****************************

void error(string msg)
{
	cerr << ERROR_PREFIX << msg << endl;
	exit(EXIT_CODE);
}

/**
 * Returns the size of the given file.
 */
int get_file_size(ifstream &ifs)
{
	long begin, end;
	begin = ifs.tellg();
	ifs.seekg(0, ios::end);
	end = ifs.tellg();
	ifs.seekg(ios::beg);
	return end - begin;
}

void send_buffer(char* buf, int buf_size)
{
	int bytes_sent = 0;
	int sent = 1;

	while (bytes_sent < buf_size)
	{
		sent = send(server_socket, buf + bytes_sent, buf_size - bytes_sent, 0);
		if (sent < 0)
		{
			error(SEND_ERROR);
		}
		bytes_sent += sent;
	}
}

void send_file_content(ifstream& ifs)
{
	char* buffer = (char*) malloc(BUFFER_SIZE);
	if (buffer == NULL)
		error(MALLOC_ERROR);
	int to_send=file_size;

	while (to_send > BUFFER_SIZE){
		ifs.read(buffer, BUFFER_SIZE);
		send_buffer(buffer, BUFFER_SIZE);
		to_send -= BUFFER_SIZE;
	}
	if (to_send != 0){
		ifs.read(buffer, to_send);
		send_buffer(buffer, to_send);
	}
	free (buffer);
}


//*********************************** MAIN ************************************


int main(int argc, char** argv)
{

	// Get the port and host
	server_port = atoi(argv[PORT_PARA_IDX]);
	server = gethostbyname(argv[HOST_PARA_IDX]);
	string tmp;

	//get the (local) file name
	tmp = argv[FILE_NAME_PARA_IDX];
	filename_to_transfer = (char*) malloc(tmp.size() + 1);
	if (filename_to_transfer == NULL)
		error(MALLOC_ERROR);
	memcpy(filename_to_transfer, tmp.c_str(), tmp.size() + 1);
	tmp = argv[OUT_FILE_NAME_PARA_IDX];

	//get the file to save name
	name_size = tmp.length();
	filename_to_save = (char*) malloc(tmp.size() + 1);
	if (filename_to_save == NULL)
		error (MALLOC_ERROR);
	memcpy (filename_to_save, tmp.c_str(), tmp.size() + 1);

	//create a socket:
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	//open the file and get it's size:
	ifstream ifs(filename_to_transfer, ios::in);
	if (ifs == NULL)
		error(OPEN_FILE_ERROR);

	//connect to the server.
	if (connect(server_socket, (sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error(CONNECT_ERROR);

	file_size = get_file_size(ifs);

	//send all the information
	send_buffer((char*) &name_size , sizeof(int));
	send_buffer(filename_to_save, name_size + 1);
	send_buffer ((char*) &file_size, sizeof(int));
	send_file_content(ifs);

	//closing
	free(filename_to_save);
	free(filename_to_transfer);
	close(server_socket);
	ifs.close();

	return SUCCESS;
}

//******************************* END OF PROGRAM*******************************
