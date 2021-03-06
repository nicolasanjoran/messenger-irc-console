#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include "protocole.h"


//=========================== GLOBAL VARs & Types ==========================//
struct channel {
  int idChannel; // -1 means not specified.
  char* name;
  int nbClients;
  int clients[MAX_CLIENTS];
  int file;
};

struct client{
  int idClient; // -1 means not specified.
  char* name;
  struct sockaddr_in client_addr;
  char isAlive;
};

struct frameStruct{
	char active;
	char* msg;
	char clientAcked[MAX_CLIENTS]; // 0: Client is not concerned or already acked
								   // 1: Client has not acked yet, wait 1s more.
								   // 2: Server has waited for 1s, re-try to send frame.
};

struct client clients[MAX_CLIENTS];
struct channel channels[MAX_CHANNELS];
struct frameStruct framesHist[FRAME_HIST_LEN];
struct sockaddr_in client_addr, server_addr;
socklen_t addr_len;
int sd;
int nbConnectedClients;
int nbActiveChannels;
unsigned int frameCounter;
pthread_t threadTimeOut;
pthread_t threadAckThr;

time_t timestamp; 
struct tm * t; 
//==========================================================================//

//============================ Threads ===================================//

/**
 * Surveille que les clients sont encore connectés
 */
void * threadTimeOutChecker(void* arg)
{
	int i=0;
	while(1)
	{
		sleep(10);

		for(i=0 ; i<MAX_CLIENTS ; i++)
		{
			if(getIdClient(i) >= 0)
			{
				clients[i].isAlive = 0;
				char aliveMsg[7];
				sprintf(aliveMsg, "ALIVE");
				send2Client(aliveMsg, i, clients[i].client_addr);
			}
		}
		sleep(2);

		for(i=0 ; i<MAX_CLIENTS ; i++)
		{
			if(getIdClient(i) >= 0 && clients[i].isAlive==0)
			{
				// TIMEOUT !!
				disconnectClient(i);
			}
		}
	}

	return NULL;
}

/**
 * Gère les acquittements des clients
 */
void * threadAckSystem(void * arg)
{
	int i, j;
	while(1)
	{
		sleep(1);
		for(i=0 ; i<FRAME_HIST_LEN && framesHist[i].active == 1 ; i++)
		{
			int clientCounter = 0;
			for(j=0 ; j<MAX_CLIENTS ; j++)
			{
				if(framesHist[i].clientAcked[j] == 1)
				{
					framesHist[i].clientAcked[j]++;
					clientCounter++;
				}
				else if(framesHist[i].clientAcked[j] == 2)
				{
					framesHist[i].clientAcked[j] = 1;
					sendto(sd, framesHist[i].msg, strlen(framesHist[i].msg), 0, (struct sockaddr *)&clients[j].client_addr, addr_len);
					clientCounter++;
				}
			}
			if(clientCounter == 0)
			{
				framesHist[i].active = 0;
				free(framesHist[i].msg);
			}
		}
	}
	return NULL;
}
//==========================================================================//


//============================ Fonctions ===================================//

/**
 * Initialisation
 */
void init()
{
	int i,j;
	nbConnectedClients = 0;
	nbActiveChannels = 0;
	frameCounter = 1;

	//Initialisation de la liste des clients : -1 = pas de client
	for(i=0 ; i<MAX_CLIENTS ; i++)
	{
		clients[i].idClient = -1;
		clients[i].name = "";
		clients[i].isAlive = '0';
	}

	//Initialisation de la liste des salons : -1 = salon inexistant
	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		channels[i].idChannel = -1;
		channels[i].name = "";
		channels[i].nbClients = 0;
	}

	//Initialisation de la pile des message à traiter
	for(i=0 ; i<FRAME_HIST_LEN ; i++)
	{
		framesHist[i].active = 0;
		for (j = 0; j < MAX_CLIENTS; j++)
		{
			framesHist[i].clientAcked[j] = 0;
		}
	}

	mkdir("channels", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}
/**
 * Incrémente le numéro de la trame à envoyer
 */
int incFrameCounter()
{
	frameCounter = (frameCounter + 1) %FRAME_HIST_LEN;
	return frameCounter;
}

/**
 * Convertie la date courante en string format dd-mm-yyyy hh:min:sec
 */
char* time2string()
{
	char* stringTime = malloc(20);
	timestamp = time(NULL); 
    t = localtime(&timestamp); 
    sprintf(stringTime, "%02d-%02d-%04d %02d:%02d:%02d", t->tm_mday, t->tm_mon+1, t->tm_year+1900, t->tm_hour, t->tm_min, t->tm_sec);

    return stringTime;
}

/**
 *param *s : string à envoyer
 *param stringSize : nombre de caractères
 *return check sum du message
 */
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
	return (sum%10)+97;
}


/**
 * Envoi une chaîne de caractère à un client
 * param msg : message à envoyer
 * param idClient : id du destinataire
 * sockaddr_client : socket du destinataire
 */
void send2Client(char* msg, int idClient, struct sockaddr_in sockaddr_client)
{
	char finalMsg[MAX_MSG];
	int i;


	//Calcul du check sum pour la trame à envoyer
	sprintf(finalMsg,"%s%c%u%c", msg,0x01, incFrameCounter(),0x01);
	unsigned char chkSum = getChecksum(msg, strlen(msg));

	//Concatenation de la trame
	sprintf(finalMsg,"%s%c", finalMsg, chkSum);

	//Si ACK
	if(idClient != -1)
	{
		framesHist[frameCounter].msg = finalMsg;
		framesHist[frameCounter].clientAcked[idClient] = 1;
	}

	printf("SEND=> %s\n", finalMsg);
	for(i=0 ; i<strlen(finalMsg) ; i++)
	{
		printf("%02x ", finalMsg[i]);
	}
	printf("\n");

	//Envoi du message
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


int CHANNEL_getID(int idChannel)
{
	int i;
	int channelExists = -1;
	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		if(channels[i].idChannel == idChannel)
		{
			channelExists = idChannel;
			break;
		}
	}
	return channelExists;
}

// Return the idChannel
int CHANNEL_JoinClient(int idClient, char* idChannel_s_ptr)
{
	int i;
	int idChannel_i = -1;
	int idNullChannel = -1;
	char*idChannel_s = malloc(strlen(idChannel_s_ptr)+1);
	strcpy(idChannel_s, idChannel_s_ptr);

	if(getIdClient(idClient) != -1)
	{
		for(i=0 ; i<MAX_CHANNELS ; i++)
		{
			if(strcmp(idChannel_s, channels[i].name) == 0)
			{
				idChannel_i = i;
			}
			if(channels[i].idChannel == -1 && idNullChannel == -1)
			{
				idNullChannel = i;
			}
		}

		if(idChannel_i == -1 && nbActiveChannels<MAX_CHANNELS && idNullChannel != -1)
		{
			char * filename = malloc(strlen(idChannel_s)+15);

			idChannel_i = idNullChannel;
			printf("%s: Channel %s does not exist, it will be created ==> id=%d.\n",time2string(), idChannel_s, idNullChannel);
			sprintf(filename,"channels/%s.hist",idChannel_s);
			channels[idNullChannel].idChannel = idChannel_i;
			channels[idNullChannel].name = idChannel_s;
			channels[idNullChannel].file = open(filename, O_CREAT | O_RDWR, 0666);
		}

		if(idChannel_i == -1 || getIdClient(idClient) == -1)
		{
			idChannel_i = -1;
			printf("%s: Client id %d cannot be joined to %s channel.", time2string(), idClient, idChannel_s);
		}else{
			for(i=0 ; i<MAX_CLIENTS && idClient<MAX_CLIENTS ; i++)
			{
				if(channels[idChannel_i].clients[i] == -1)
				{
					channels[idChannel_i].clients[i] = idClient;
				}
			}
		}
	}
	else
	{
		printf("Client does not exist. Cannot join channel.\n");
	}
	return idChannel_i;
}

/**
 * Supprime un client d'un salon
 * param idClient
 * param idChannel
 */
int CHANNEL_LeaveClient(int idClient, int idChannel)
{
	int i;
	printf("User %d left channel %d\n", idClient, idChannel);

	for(i=0 ; i<MAX_CLIENTS && idClient<MAX_CLIENTS && idChannel<MAX_CHANNELS ; i++)
	{
		if(channels[idChannel].clients[i] == idClient)
		{
			channels[idChannel].clients[i] = -1;
		}
	}
	return 1;
}

/**
 * Gere les client lors d'une demande de connexion
 * param clientNamePtr : nom du client
 * param sockaddr_client : socket du client
 * return id du client
 */
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
		printf("%s: New client cannot be accepted, MAX_CLIENTS number has already been reached.\n", time2string());
		result = -1;
	}else{
		//printf("%s %s STRCMP=%d\n",clientName,clients[i].name,strcmp(clientName, clients[i].name));
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
			printf("%s: Client %s just connected => idClient: %d.\n",time2string(), clientName, id);
			result = id;
		}
		else if(id==-1 && uniqueName == 0)
		{
			printf("%s: New client cannot be accepted, specified name already exists.\n", time2string());
			result=-2;
		}else{
			printf("Error.. id=%d, uniqueName=%d\n", id, uniqueName);
			result=-3;
		}
	}

	return result;
}

/**
 * Supprime un client du serveur et de tout les salons
 * param clientNB : id du client
 */
int disconnectClient(int clientNB)
{
	int i;

	if(clientNB < MAX_CLIENTS && clients[clientNB].idClient != -1)
	{
		printf("%s: Client %s disconnected.\n", time2string(), clients[clientNB].name);
		clients[clientNB].idClient = -1;
		clients[clientNB].name = "";
	}

	for(i=0 ; i<MAX_CHANNELS ; i++)
	{
		CHANNEL_LeaveClient(clientNB, i);
	}
	return 0;
}


/**
 * retransmet un message en provenance d'un client à tous les autres
 * param idClient : auteur du message
 * param idChannel : salon de destination
 */
void transmit(int idClient, int msg_type, int idChannel, char* message)
{
	char finalMsg[MAX_MSG];

	if(msg_type == MSG_TEXT)
	{
		sprintf(finalMsg, "TRANSMIT%c%d%cMSG%c%s", 0x01, idChannel,0x01, 0x01, message);
	}else if(msg_type == MSG_CLIENTS)
	{

	}else if(msg_type == MSG_HIST){

	}else{
		msg_type = -1;
	}

	if(msg_type != -1)
	{
		if(idClient == TRANSMIT_ALL)
		{
			int i;
			for (i = 0; i < MAX_CLIENTS; i++)
			{
				if(channels[idChannel].clients[i] != -1 && clients[i].idClient != -1)
				{
					send2Client(finalMsg, i, clients[i].client_addr);
				}
			}
		}else{
			send2Client(finalMsg, idClient, clients[idClient].client_addr);
		}
	}
}


int manageMsg(int idClient, int idChannel, char* msg)
{
	int result = -1;
	int j=0;
	char finalMsg[MAX_MSG];
	int msgLen = strlen(msg);
	printf("msgLen=%d\n", msgLen);
	if(CHANNEL_getID(idChannel)>=0 && getIdClient(idClient)>=0)
	{
		sprintf(finalMsg, "<<%s> -- %s>", clients[idClient].name, time2string());
		sprintf(finalMsg, "%s %s", finalMsg, msg);
		/*
		for (j = 0; j*100+i < msgLen ; j++)
		{
			sprintf(finalMsg, "%s\n> ", finalMsg);
			for (i = 0; i < 100 && j*100+i < msgLen ; i++)
			{
				sprintf(finalMsg,"%s%c", finalMsg, msg[j*100+i]);
			}
		}
		sprintf(finalMsg, "%s\n ", finalMsg);
		//*/
		write(channels[idChannel].file, finalMsg, strlen(finalMsg));
		transmit(TRANSMIT_ALL, MSG_TEXT,idChannel,finalMsg);
		result=0;
		
	}
	return result;
}


void ack_frame(int idFrame, int idClient, int cmd_i, char* cmd_s, struct sockaddr_in sockaddr_client,int result)
{
	char rsp_value[15];
	char rsp[MAX_MSG];
	int i;
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

	sprintf(rsp, "ACK%c%d%c%s", 0x01, idFrame, 0x01, rsp_value);
	printf("ACK=> %s\n", rsp);
	for(i=0 ; i<strlen(rsp) ; i++)
	{
		printf("%02x ", rsp[i]);
	}
	printf("\n");
	send2Client(rsp, -1,sockaddr_client);

}

void analyzeAck(int idClient, int idFrame)
{
	if(getIdClient(idClient) >= 0)
	{
		clients[idClient].isAlive = 1;
	}
	if(getIdClient(idClient) >= 0 && idFrame >= 0 && idFrame < FRAME_HIST_LEN)
	{
		framesHist[idFrame].clientAcked[idClient] = 0;
	}
}

/**
 * Décortique les messages reçus
 */
void analyzeFrame(char* rcvdFrameFromCall, struct sockaddr_in addr_client_frame)
{
	/*
	 * This is used for extracting fields from the received frame.
	 */
	char** extractedFrame;
	int cnt = 0;
	//char*rcvdFrame = malloc(strlen_int(frame));
	//memcpy(rcvdFrame, frame, strlen_int(frame));
	//rcvdFrame = frame;
	char frameCopy[MAX_MSG];
	char rcvdFrame[MAX_MSG];
	strcpy(frameCopy, rcvdFrameFromCall);
	strcpy(rcvdFrame, rcvdFrameFromCall);
	extractedFrame = (char**)malloc(6*sizeof(char*));
	int totalExtracted=0;
	int result = 0;
	char cmd = -1;
	int rcvdIdFrame = -1;
	char delimiter = 0x01;
	int idClient=-1;
	int idx=0;
	int i=0;
	char sum=0;
	//printf("analyzeFrame(%s)\n", rcvdFrame);
	extractedFrame[idx]=strtok(rcvdFrame, &delimiter); // transform 0x01 to string.
	for(idx=0 ; idx<6 ; idx++)
	{
		extractedFrame[idx+1] = strtok(NULL, &delimiter);
		printf("idx=%d, rcvd: %s\n", idx, extractedFrame[idx]);
		if(extractedFrame[idx]==NULL) break;

	}
	printf("TotalExtracted: %d\n", idx);
	totalExtracted=idx;

	/*
	 * Check the correctness of the frame (from its Checksum)
	 */
	for(i=0 ; i<idx ; i++)
	{
		//printf("i=%d, rcvd: %s\n", i, extractedFrame[i]);
	}

	if(frameCopy[strlen(frameCopy)-1] == 0x01) totalExtracted++;
	/*
	 * Extract idFrame
	 */
	if(totalExtracted > 3)
	{
		/*
		 * Checksum processing
		 */
		 //printf("rcvdFrameLen: %d\n", strlen(frameCopy));
		printf("Processed checksum: %x    ::  Actual:%x\n",getChecksum(frameCopy, strlen(frameCopy)-1), (unsigned char)frameCopy[strlen(frameCopy)-1]);
		if(0)//(unsigned char)extractedFrame[totalExtracted-1][0] != getChecksum(rcvdFrame, strlen(rcvdFrame)-1))
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
	else if(totalExtracted == 6 && strncmp(extractedFrame[0], "SAY", strSize) == 0)
	{
		manageMsg(atoi(extractedFrame[1]), atoi(extractedFrame[2]), extractedFrame[3]);
		cmd = CMD_SAY;
	}
	else if(totalExtracted == 5 && strncmp(extractedFrame[0], "LEAVE", strSize) == 0)
	{
		result = CHANNEL_LeaveClient(atoi(extractedFrame[1]), atoi(extractedFrame[2]));
		cmd = CMD_LEAVE;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "DISCONNECT", strSize) == 0)
	{
		result = disconnectClient(atoi(extractedFrame[1]));
		cmd = CMD_DISCONNECT;
	}
	else if(totalExtracted == 6 && strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{
		analyzeAck(atoi(extractedFrame[1]), atoi(extractedFrame[2]));
		cmd = CMD_ACK;
	}else{
		result = -1;
		printf("%s: Incoming frame does not correspond to expected scheme : ACK will not be granted.\n", time2string());
	}

	if(cmd != CMD_ACK)
	{
		ack_frame(rcvdIdFrame, idClient, cmd, extractedFrame[0], addr_client_frame, result);
	}

}

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

  printf("%s: Server launched. Waiting for clients.\n", time2string());

  pthread_create(&threadTimeOut, NULL, threadTimeOutChecker, NULL);
  pthread_create(&threadAckThr, NULL, threadAckSystem, NULL);

  // Callback
  for (;;)
  {
    addr_len = sizeof(client_addr);
    memset(msgbuf,0,strlen(msgbuf));
    n = recvfrom(sd, msgbuf, MAX_MSG, 0, (struct sockaddr *)&client_addr, &addr_len);
    if (n == -1)
      perror("recvfrom");
    else {
    	printf("%s: received from %s: %s\n", time2string(), inet_ntoa(client_addr.sin_addr), msgbuf);
    	analyzeFrame(msgbuf, client_addr);

      //sendto(sd, "OK", sizeof("OK"), 0, (struct sockaddr *)&client_addr, addr_len);
    }
  }
  return 0;
}
//==========================================================================//

