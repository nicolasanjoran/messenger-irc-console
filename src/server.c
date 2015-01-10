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

#define CMD_CONNECT 1
#define CMD_JOIN 2
#define CMD_SAY 3
#define CMD_LEAVE 4
#define CMD_DISCONNECT 5
#define CMD_ACK 6

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
int nbActiveChannels;
unsigned int frameCounter;
//==========================================================================//


//============================ Fonctions ===================================//

void init()
{
	int i;
	nbConnectedClients = 0;
	nbActiveChannels = 0;
	frameCounter = 0;

	for(i=0 ; i<MAX_CLIENTS ; i++)
	{
		clients[i].idClient = -1;
		clients[i].name = "";
	}

	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		channels[i].idChannel = -1;
		channels[i].name = "";
		channels[i].nbClients = 0;
	}
}
unsigned char getChecksum(char*s, int stringSize)
{
	unsigned char sum = 0;
	int i=0;
	//printf("LEN: %d -- ", stringSize);
	//printf("%s\n",s);

	for(i=0 ; i<stringSize ; i++)
	{
		//printf("%x - ",s[i]);
		sum += s[i];
	}
	//printf("\n");
	return sum;
}

void send2Client(char* msg, struct sockaddr_in sockaddr_client)
{
	unsigned char chkSum = getChecksum(msg, strlen(msg));

	char finalMsg[MAX_MSG];

	sprintf(finalMsg,"%s%c%u%c%c", msg, 0x01, frameCounter++, 0x01, chkSum);

	sendto(sd, finalMsg, strlen(finalMsg), 0, (struct sockaddr *)&sockaddr_client, addr_len);
}

// Returns -1 if client does not exist.
int getIdClient(int idClient)
{
	int i;
	int clientExists = -1;
	for(i=0 ; i<MAX_CLIENTS ; i++)
	{
		if(clients[i].idClient == idClient)
		{
			clientExists = idClient;
		}
	}
	return clientExists;
}

// Return the idChannel
int CHANNEL_JoinClient(int idClient, char* idChannel_s)
{
	int i;
	int idChannel_i = -1;
	int idNullChannel = -1;


	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		if(strcmp(idChannel_s, channels[i].name) == 0)
		{
			idChannel_i = i;
		}
		if(channels[i].idChannel == -1)
		{
			idNullChannel = i;
		}
	}

	if(idChannel_i == -1 && nbActiveChannels<MAX_CHANNELS && idNullChannel != -1)
	{
		idChannel_i = idNullChannel;
	}

	if(idChannel_i == -1 || getIdClient(idClient) == -1)
	{
		idChannel_i = -1;
		printf("Client id %d cannot be joined to %s channel.", idClient, idChannel_s);
	}else{
		for(i=0 ; i<MAX_CLIENTS && idClient<MAX_CLIENTS ; i++)
		{
			if(channels[idChannel_i].clients[i] == -1)
			{
				channels[idChannel_i].clients[i] = idClient;
			}
		}
	}
	return idChannel_i;
}

int CHANNEL_LeaveClient(int idClient, int idChannel)
{
	int i;

	for(i=0 ; i<MAX_CLIENTS && idClient<MAX_CLIENTS && idChannel<MAX_CHANNELS ; i++)
	{
		if(channels[idChannel].clients[i] == idClient)
		{
			channels[idChannel].clients[i] = -1;
		}
	}
	return 1;
}

// Return the idClient
int acceptClient(char* clientNamePtr, struct sockaddr_in sockaddr_client)
{
	int i;
	int id = -1;
	char uniqueName = 1;
	int result;
	char*clientName = malloc(strlen(clientNamePtr));
	strcpy(clientName, clientNamePtr);
	if(nbConnectedClients == MAX_CLIENTS)
	{
		printf("New client cannot be accepted, MAX_CLIENTS number has already been reached.\n");
		result = -1;
	}else{
		printf("%s %s STRCMP=%d\n",clientName,clients[i].name,strcmp(clientName, clients[i].name));
		for(i=0 ; i<MAX_CLIENTS ; i++)
		{
			if(clients[i].idClient!= -1 && strcmp(clientName, clients[i].name) == 0)
			{
				uniqueName = 0;
				break;
			}

			if(clients[i].idClient == -1 && id == -1)
			{
				id=i;
			}
		}

		if(id!=-1 && uniqueName == 1)
		{
			clients[id].idClient = id;
			clients[id].name = clientName;
			clients[id].client_addr = sockaddr_client;
			printf("Client %s just connected => idClient: %d.\n", clientName, id);
			result = id;
		}
		else if(id==-1 && uniqueName == 0)
		{
			printf("New client cannot be accepted, specified name already exists.\n");
			result=-2;
		}else{
			printf("Error.. id=%d, uniqueName=%d\n", id, uniqueName);
			result=-3;
		}
	}

	return result;
}

int disconnectClient(int clientNB)
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
	return 0;
}

void transmit(int client, char* message);

void ack_frame(int idFrame, int idClient, int cmd_i, char* cmd_s, struct sockaddr_in sockaddr_client,int result)
{
	char rsp_value[10];
	char rsp[MAX_MSG];
	if(cmd_s == NULL || cmd_i==-1)
	{
		sprintf(cmd_s, "ERR");
	}
	if(result < 0)
	{
		sprintf(rsp_value, "ERR");
	}else{
		sprintf(rsp_value, "%d", result);
	}

	sprintf(rsp, "ACK%c%s%c%s", (char)0x01, cmd_s, (char)0x01, rsp_value);

	send2Client(rsp, sockaddr_client);

}

void analyzeFrame(char* frame, struct sockaddr_in addr_client_frame)
{
	/*
	 * This is used for extracting fields from the received frame.
	 */
	char** extractedFrame;
	char*rcvdFrame = malloc(strlen(frame)+1);
	strcpy(rcvdFrame, frame);
	extractedFrame = (char**)malloc(5*sizeof(char*));
	int totalExtracted=0;
	int result = 0;
	char cmd = -1;
	int rcvdIdFrame = -1;
	char delimiter = 0x01;
	int idClient=-1;
	int idx=0;
	int i=0;
	char sum=0;
	extractedFrame[idx]=strtok(frame, &delimiter); // transform 0x01 to string.
	for(idx=0 ; idx<4 ; idx++)
	{
		extractedFrame[idx+1] = strtok(NULL, &delimiter);
		if(extractedFrame[idx]==NULL) break;
		//if(extractedFrame[idx])

	}
	//printf("TotalExtracted: %d\n", idx);
	totalExtracted=idx;

	/*
	 * Check the correctness of the frame (from its Checksum)
	 */
	for(i=0 ; i<idx ; i++)
	{
		//printf("i=%d, rcvd: %s\n", i, extractedFrame[i]);
	}

	/*
	 * Extract idFrame
	 */
	if(totalExtracted > 3)
	{
		/*
		 * Checksum processing
		 */
		printf("Processed checksum: %x    ::  Actual:%x\n",getChecksum(rcvdFrame, strlen(rcvdFrame)-1), (unsigned char)extractedFrame[totalExtracted-1][0]);
		if((unsigned char)extractedFrame[totalExtracted-1][0] != getChecksum(rcvdFrame, strlen(rcvdFrame)-1))
		{
			totalExtracted = 0;
		}else
		{
			printf("idFrame: %s\n", extractedFrame[totalExtracted-2]);
			rcvdIdFrame = atoi(extractedFrame[totalExtracted-2]);
		}
	}

	/*
	 * This is to identify which kind of frame it is.
	 */
	int strSize = sizeof(extractedFrame[0])/sizeof(char);
	if(totalExtracted == 4 && strncmp(extractedFrame[0], "CONNECT", strSize) == 0)
	{
		result = acceptClient(extractedFrame[1], addr_client_frame);
		idClient = result;
		cmd = CMD_CONNECT;
	}
	else if(totalExtracted == 5 && strncmp(extractedFrame[0], "JOIN", strSize) == 0)
	{
		result = CHANNEL_JoinClient(atoi(extractedFrame[1]), extractedFrame[2]);
		cmd = CMD_JOIN;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "SAY", strSize) == 0)
	{
		//TODO Say
		cmd = CMD_SAY;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "LEAVE", strSize) == 0)
	{
		result = CHANNEL_LeaveClient(atoi(extractedFrame[1]), atoi(extractedFrame[2]));
		cmd = CMD_LEAVE;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "DISCONNECT", strSize) == 0)
	{
		result = disconnectClient(atoi(extractedFrame[1]));
		cmd = CMD_DISCONNECT;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{
		//TODO ACK system.
		cmd = CMD_ACK;
	}else{
		result = -1;
		printf("Incoming frame does not correspond to expected scheme : ACK will not be granted.\n");
	}

	//TODO Produce response (ACK).
	if(cmd != CMD_ACK)
	{
		ack_frame(rcvdIdFrame, idClient, cmd, extractedFrame[0], addr_client_frame, result);
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
    	analyzeFrame(msgbuf, client_addr);

      //sendto(sd, "OK", sizeof("OK"), 0, (struct sockaddr *)&client_addr, addr_len);
    }
  }
  return 0;
}
//==========================================================================//

