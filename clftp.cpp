//********************************************************************
//This program created by Netanel Zakay for OS course, 2013-2014, HUJI
//********************************************************************



//********************************* INCLUDES **********************************

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>

using namespace std;


//********************************* VARIABLES *********************************

#define SUCCESS 0
#define ERROR -1
#define EXIT_CODE 1
#define BUFFER_SIZE 1024

#define PORT_PARA_INDX 1
#define HOST_PARA_INDX 2
#define FILE_NAME_PARA_INDX 3
#define OUT_FILE_NAME_PARA_INDX 4

int _sPort;
int _serverSocket;
int _nameSize;
int _fileSize;
char* _fileToTransfer;
char* _fileToSave;


struct sockaddr_in serv_addr;
struct hostent *server = NULL;


//****************************** HELPERS FUNCTIONS ****************************

void error(string errorMessage){
	cerr << errorMessage;
	exit(EXIT_CODE);
}

int getFileSize(ifstream &ifs) {
	long begin,end;
	begin = ifs.tellg();
	ifs.seekg (0, ios::end);
	end = ifs.tellg();
	ifs.seekg(ios::beg);
	return end-begin;
}

void sendAllBuffer (char* buffer, int bufferSize){
	int bytesSent=0;
	int sent=1;

	while (bytesSent<bufferSize){
		sent = send(_serverSocket, buffer + bytesSent, bufferSize- bytesSent,0);
		if (sent== ERROR)
			error ("ERROR: reciving data");
		bytesSent += sent;
	}
}

void sendFileContent(ifstream& ifs){
	char* buffer = (char*) malloc(BUFFER_SIZE);
	if (buffer==NULL)
		error ("ERROR: malloc error.");
	int toSend=_fileSize;

	while (toSend>BUFFER_SIZE){
		ifs.read(buffer,BUFFER_SIZE);
		sendAllBuffer(buffer , BUFFER_SIZE);
		toSend-=BUFFER_SIZE;
	}
	if (toSend!=0){
		ifs.read(buffer,toSend);
		sendAllBuffer(buffer , toSend);
	}
	
	free (buffer);
}


//*********************************** MAIN ************************************


int main(int argc, char** argv){

	//get the port and host
	_sPort = atoi(argv[PORT_PARA_INDX]);
	server = gethostbyname(argv[HOST_PARA_INDX]);
	string tmp;

	//get the (local) file name
	tmp = argv[FILE_NAME_PARA_INDX]; 
	_fileToTransfer = (char*)malloc(tmp.size()+1);
	if (_fileToTransfer==NULL)
		error ("ERROR: malloc error.");
	memcpy (_fileToTransfer, tmp.c_str(), tmp.size()+1);
	tmp = argv[OUT_FILE_NAME_PARA_INDX]; 

	//get the file to save name
	_nameSize=tmp.length();
	_fileToSave = (char*)malloc(tmp.size()+1);
	if (_fileToSave==NULL)
		error ("ERROR: malloc error.");
	memcpy (_fileToSave, tmp.c_str(), tmp.size()+1);

	//create a socket:
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(_sPort);

	//open the file and get it's size:
	ifstream ifs (_fileToTransfer, ios::in);
	if (ifs == NULL)
		error("ERROR: open file");

	//connect to the server.
	if (connect(_serverSocket,((struct sockaddr*)&serv_addr),sizeof(serv_addr)) < 0)//connecting to server
		error("ERROR connecting");

	_fileSize = getFileSize(ifs);

	//send all the information
	sendAllBuffer((char*) &_nameSize , sizeof(int));
	sendAllBuffer(_fileToSave, _nameSize+1);
	sendAllBuffer ((char*) &_fileSize, sizeof (int));
	sendFileContent(ifs);

	//closing
	free (_fileToSave);
	free (_fileToTransfer);
	close(_serverSocket);
	ifs.close();

	return SUCCESS;
}

//******************************* END OF PROGRAM*******************************
