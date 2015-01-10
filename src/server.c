#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


//========================== DEFINES =======================================//
#define SERVER_PORT 1500
#define MAX_MSG 1500
#define MAX_CLIENTS 100
#define MAX_CLIENTS_digits 3
#define MAX_CHANNELS 10
//==========================================================================//



//=========================== GLOBAL VARs & Types ==========================//
struct channel {
  int idChannel; // -1 means not specified.
  char* name;
  int nbClients;
  int clients[MAX_CLIENTS];
};

struct client{
  int idClient; // -1 means not specified.
  char* name;
  struct sockaddr_in client_addr;
};

struct client clients[MAX_CLIENTS];
struct channel channels[MAX_CHANNELS];
struct sockaddr_in client_addr, server_addr;
socklen_t addr_len;
int sd;
int nbConnectedClients;
//==========================================================================//


//============================ Fonctions ===================================//

init()
{
	int i;
	for(i=0 ; i<MAX_CLIENTS ; i++)
	{
		clients[i].idClient = -1;
		clients[i].name = NULL;
	}

	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		channels[i].idChannel = -1;
		channels[i].name = NULL;
		channels[i].nbClients = 0;
	}
}

// Return the idChannel
int CHANNEL_JoinClient(int client, char* channel)
{
	return 0;
}

void CHANNEL_LeaveClient(int idClient, int channel);

// Return the idClient
int acceptClient(char* clientName)
{
	int i;
	int id = -1;
	char uniqueName = 1;
	int result;
	if(nbConnectedClients == MAX_CLIENTS)
	{
		printf("New client cannot be accepted, MAX_CLIENTS number has already been reached.\n");
		result = -1;
	}else{
		for(i=0 ; i<MAX_CLIENTS ; i++)
		{
			if(clients[i].idClient!= -1 && strcmp(clientName, clients[i]) == 0)
			{
				uniqueName = 0;
				break;
			}

			if(clients[i].idClient == -1)
			{
				id=i;
			}
		}

		if(id!=-1 && uniqueName == 1)
		{
			clients[id].idClient = id;
			clients[id].name = clientName;
			printf("Client %s just connected.\n", clientName);
			result = id;
		}
		else if(id!=-1 && uniqueName == 0)
		{
			printf("New client cannot be accepted, specified name already exists.\n");
			result=-2;
		}
	}

	return result;
}

void disconnectClient(int clientNB)
{
	int i;

	if(clientNB < MAX_CLIENTS && clients[clientNB].idClient != -1)
	{
		printf("Client %s disconnected.\n", clients[clientNB].name);
		clients[clientNB].idClient = -1;
		clients[clientNB].name = NULL;
	}

	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		CHANNEL_LeaveClient(clientNB, i);
	}
}

int transmit(int client, char* message);

void analyzeFrame(char* frame)
{
	/*
	 * This is used for extracting fields from the received frame.
	 */
	char** extractedFrame;
	extractedFrame = (char**)malloc(5*sizeof(char*));
	char delimiter = 0x01;
	int idx=0;
	int i=0;
	char sum=0;
	extractedFrame[idx]=strtok(frame, &delimiter); // Transformer le 0x00 en string.
	for(idx=0 ; idx<4 ; idx++)
	{
		extractedFrame[idx+1] = strtok(NULL, &delimiter);
		if(extractedFrame[idx]==NULL) break;

	}
	printf("TotalExtracted: %d\n", idx);

	/*
	 * Check the correctness of the frame (from its Checksum)
	 */
	for(i=0 ; i<5 && extractedFrame[i]!=NULL ; i++)
	{
		printf("i=%d, rcvd: %s\n", i, extractedFrame[i]);
	}


	/*
	 * This is to identify which kind of frame it is.
	 */
	int strSize = sizeof(extractedFrame[0])/sizeof(char);
	if(strncmp(extractedFrame[0], "CONNECT", strSize) == 0)
	{
		acceptClient(extractedFrame[1]);
	}
	else if(strncmp(extractedFrame[0], "JOIN", strSize) == 0)
	{

	}
	else if(strncmp(extractedFrame[0], "SAY", strSize) == 0)
	{

	}
	else if(strncmp(extractedFrame[0], "LEAVE", strSize) == 0)
	{

	}
	else if(strncmp(extractedFrame[0], "DISCONNECT", strSize) == 0)
	{

	}
	else if(strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{

	}


}

int printStatus();
//==========================================================================//


//============================= MAIN =======================================//
int main(void)
{
  int n;
  char msgbuf[MAX_MSG];

  init();

  // Create socket
  if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("socket creation");
    return 1;
  }
  // Bind it
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(SERVER_PORT);
  if (bind(sd, (struct sockaddr *)&server_addr, sizeof server_addr) == -1)
  {
    perror("bind");
    return 1;
  }

  // Callback
  for (;;)
  {
    addr_len = sizeof(client_addr);
    n = recvfrom(sd, msgbuf, MAX_MSG, 0, (struct sockaddr *)&client_addr, &addr_len);
    if (n == -1)
      perror("recvfrom");
    else {
    	printf("received from %s: %s\n", inet_ntoa(client_addr.sin_addr), msgbuf);
    	analyzeFrame(msgbuf);

      //sendto(sd, "OK", sizeof("OK"), 0, (struct sockaddr *)&client_addr, addr_len);
    }
  }
  return 0;
}
//==========================================================================//

