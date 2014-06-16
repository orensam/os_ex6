//********************************************************************
// clftp.cpp - Client side for a simple file-transfer protocol.
// This program created by Netanel Zakay for OS course, 2013-2014, HUJI
// Edited by Yoni Herzog & Oren Samuel
//********************************************************************



//********************************* INCLUDES **********************************

#include "ftp.h"

#include <fstream>

using namespace std;

//********************************* VARIABLES *********************************

#define PORT_PARA_INDX 1
#define HOST_PARA_INDX 2
#define FILE_NAME_PARA_INDX 3
#define OUT_FILE_NAME_PARA_INDX 4

int server_port;
int server_socket;
int name_size;
int file_size;
char* filename_to_transfer;
char* filename_to_save;

struct sockaddr_in serv_addr;
struct hostent *server = NULL;

//****************************** HELPER FUNCTIONS ****************************

void error(string errorMessage)
{
	cerr << errorMessage;
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
		if (sent== FAIL)
		{
			error ("ERROR: receiving data");
		}
		bytes_sent += sent;
	}
}

void sendFileContent(ifstream& ifs){
	char* buffer = (char*) malloc(BUFFER_SIZE);
	if (buffer==NULL)
		error ("ERROR: malloc error.");
	int toSend=file_size;

	while (toSend>BUFFER_SIZE){
		ifs.read(buffer,BUFFER_SIZE);
		send_buffer(buffer , BUFFER_SIZE);
		toSend-=BUFFER_SIZE;
	}
	if (toSend!=0){
		ifs.read(buffer,toSend);
		send_buffer(buffer , toSend);
	}
	
	free (buffer);
}


//*********************************** MAIN ************************************


int main(int argc, char** argv){

	// Get the port and host
	server_port = atoi(argv[PORT_PARA_INDX]);
	server = gethostbyname(argv[HOST_PARA_INDX]);
	string tmp;

	//get the (local) file name
	tmp = argv[FILE_NAME_PARA_INDX]; 
	filename_to_transfer = (char*)malloc(tmp.size()+1);
	if (filename_to_transfer==NULL)
		error ("ERROR: malloc error.");
	memcpy (filename_to_transfer, tmp.c_str(), tmp.size()+1);
	tmp = argv[OUT_FILE_NAME_PARA_INDX]; 

	//get the file to save name
	name_size=tmp.length();
	filename_to_save = (char*)malloc(tmp.size()+1);
	if (filename_to_save==NULL)
		error ("ERROR: malloc error.");
	memcpy (filename_to_save, tmp.c_str(), tmp.size()+1);

	//create a socket:
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(server_port);

	//open the file and get it's size:
	ifstream ifs (filename_to_transfer, ios::in);
	if (ifs == NULL)
		error("ERROR: open file");

	//connect to the server.
	if (connect(server_socket,((struct sockaddr*)&serv_addr),sizeof(serv_addr)) < 0)//connecting to server
		error("ERROR connecting");

	file_size = get_file_size(ifs);

	//send all the information
	send_buffer((char*) &name_size , sizeof(int));
	send_buffer(filename_to_save, name_size+1);
	send_buffer ((char*) &file_size, sizeof (int));
	sendFileContent(ifs);

	//closing
	free (filename_to_save);
	free (filename_to_transfer);
	close(server_socket);
	ifs.close();

	return SUCCESS;
}

//******************************* END OF PROGRAM*******************************
