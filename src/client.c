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

//========================== DEFINES =======================================//
#define SERVER_PORT 1500
#define MAX_MSG 100
#define MAX_CHANNELS 10
#define TIMEOUT 10

#define CMD_CONNECT 1
#define CMD_JOIN 2
#define CMD_ACK 3
#define CMD_TRANSMIT 4

#define FRAME_HIST_LEN 500
//==========================================================================//

//=========================== GLOBAL VARs & Types ==========================//
struct channel {
	int idChannel;
	char* name;
	int file;

};

struct frameStruct{
	char active;
	char* msg;
	char serverAcked; // 0: Client is not concerned or already acked
	// 1: Client has not acked yet, wait 1s more.
	// 2: Server has waited for 1s, re-try to send frame.
};

struct channel channels[MAX_CHANNELS];
struct frameStruct framesHist[FRAME_HIST_LEN];
int idClient, sd, frameCounter;
struct sockaddr_in client_addr, serv_addr;
char nickname[50];
char addr[20];
pthread_t pthreadReceiver;
socklen_t addr_len;
//==========================================================================//


//============================ Fonctions ===================================//
//TODO: implement connect
int  SERVER_Connect();

void  SERVER_Disconnect();

// Return the idChannel
void CHANNEL_Join(char* channel);

void CHANNEL_Leave(int idChannel);

int send2Server(char *s);

void printInterface();

unsigned char getChecksum(char*s, int stringSize);

int analyzeFrame(char* rcvdFrame);

void transmit(int idChannel, char* message);

void ack_frame(int idFrame, int cmd_i, char* cmd_s, int result);
//==========================================================================//


//============================= MAIN =======================================//
int main (int argc, char *argv[])
{

	int i;
	char msgbuf[MAX_MSG];

	//
	printf("Choisissez un pseudo : \n");
	scanf("%s",nickname);
	while(SERVER_Connect() < 0)
	{
	}

	//callback
	while(1)
	{
		//TODO: Implement printInterface()
		scanf("%s",msgbuf);
	}


	close(sd);
	return 0;
}
//==========================================================================//

int incFrameCounter()
{
	frameCounter = (frameCounter + 1) %FRAME_HIST_LEN;
	return frameCounter;
}

void * threadReceiver(void * arg)
{
	int n;
	char msgbuf[MAX_MSG];

	for (;;)
	{
		addr_len = sizeof(serv_addr);
		n = recvfrom(sd, msgbuf, MAX_MSG, 0, (struct sockaddr *)&serv_addr, &addr_len);
		if (n == -1)
			perror("recvfrom");
		else {
			printf("received from %s: %s\n", inet_ntoa(serv_addr.sin_addr), msgbuf);
			analyzeFrame(msgbuf);
		}
	}
}

void * threadAckSystem(void * arg)
{
	int i, j;
	while(1)
	{
		sleep(1);
		for(i=0 ; i<FRAME_HIST_LEN ; i++)
		{


			if(framesHist[i].serverAcked == 1)
			{
				framesHist[i].serverAcked++;

			}
			else if(framesHist[i].serverAcked == 2)
			{
				framesHist[i].serverAcked = 1;
				sendto(sd, framesHist[i].msg, strlen(framesHist[i].msg), 0, (struct sockaddr *)&serv_addr, addr_len);

			}

			if(framesHist[i].serverAcked == 0)
			{
				free(framesHist[i].msg);
			}

		}
		return NULL;
	}
}

void analyzeAck( int idFrame)
{
	if(idFrame >= 0 && idFrame < FRAME_HIST_LEN)
	{
		framesHist[idFrame].serverAcked = 0;
	}
}


int  SERVER_Connect(){


	char finalMsg[MAX_MSG];
	int result;
	// Create socket
	if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("Socket can't be created\n");
		return 1;
	}

	// Bind socket
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	client_addr.sin_port        = htons(0);

	if (bind(sd,(struct sockaddr *)&client_addr, sizeof client_addr) == -1)
	{
		perror("bind");
		return 1;
	}


	// Fill server address structure

	printf("Adresse du serveur : \n");
	scanf("%s",addr);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(addr);

	if ( serv_addr.sin_addr.s_addr == ( in_addr_t)(-1))
	{
		printf("Invalid IP address format <%s>\n", addr);
		return -1;
	}
	serv_addr.sin_port = htons(SERVER_PORT);

	sprintf(finalMsg,"CONNECT%c%s", 0x01, nickname );
	result = send2Server(finalMsg);
	idClient = result;
	return result;
}


void SERVER_Disconnect()
{
	char finalMsg[MAX_MSG];

	sprintf(finalMsg,"DISCONNECT%c%d", 0x01, idClient );
	send2Server(finalMsg);

}


int send2Server(char* msg)
{
	unsigned char chkSum = getChecksum(msg, strlen(msg));
	int result,n,numTrame;
	char finalMsg[MAX_MSG];
	char msgbuf[MAX_MSG];

	numTrame = incFrameCounter();
	sprintf(finalMsg,"%s%c%d%c%c", msg,0x01,numTrame,0x01,chkSum);

	framesHist[frameCounter].msg = finalMsg;
	framesHist[frameCounter].serverAcked = 1;

	if (sendto(sd, finalMsg, strlen(finalMsg) + 1, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
	{
		perror("sendto");
	}
	return numTrame;

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

int analyzeFrame(char* rcvdFrame)
{
	/*
	 * This is used for extracting fields from the received frame.
	 */
	char** extractedFrame;
	int cnt = 0;
	//char*rcvdFrame = malloc(strlen_int(frame));
	//memcpy(rcvdFrame, frame, strlen_int(frame));
	//rcvdFrame = frame;
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

	/*
	 * Extract idFrame
	 */
	if(totalExtracted > 3)
	{
		/*
		 * Checksum processing
		 */
		printf("Processed checksum: %x    ::  Actual:%x\n",getChecksum(rcvdFrame, strlen(rcvdFrame)-1), (unsigned char)extractedFrame[totalExtracted-1][0]);
		//TODO Uncomment checksum checkers !!!
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
		//TODO : Connect
		cmd = CMD_CONNECT;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "TRANSMIT", strSize) == 0)
	{
		transmit(atoi(extractedFrame[1]),extractedFrame[4]);
		cmd = CMD_TRANSMIT;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{
		//TODO ACK system.
		cmd = CMD_ACK;
		result = atoi(extractedFrame[1]);
	}
	else
	{
		result = -1;
		printf("Incoming frame does not correspond to expected scheme : ACK will not be granted.\n");
	}

	//TODO Produce response (ACK).
	if(cmd != CMD_ACK)
	{
		ack_frame(rcvdIdFrame, cmd, extractedFrame[0], result);
	}

	return result;
}

void transmit(int idChannel, char* message){

	int result;
	if(channels[idChannel].idChannel != -1)
	{
		if((result = write(channels[idChannel].file, message, strlen(message))) < 0)
		{
			perror("write");
		}
		else
		{
			//TODO: ACK
		}
	}
	else
	{
		//channel not joined
	}

}

void CHANNEL_Join(char* channel){

	char finalMsg[MAX_MSG];
	int idChannel;

	sprintf(finalMsg,"JOIN%c%d%c%s", 0x01, idClient, 0x01, channel);

	idChannel = send2Server(finalMsg);

	if(idChannel < 0)
	{
		printf("Can't join %s server",channel);
	}
	else
	{
		//fill channel information and open/create file
		char*idChannel_s = malloc(strlen(channel)+1);
		char * filename = malloc(strlen(idChannel_s)+15);
		strcpy(idChannel_s, channel);

		channels[idChannel].idChannel = idChannel;
		channels[idChannel].name = idChannel_s;
		channels[idChannel].file = open(filename, O_CREAT | O_RDWR, 0666);
	}
}

void CHANNEL_Leave(int idChannel){

	char finalMsg[MAX_MSG];
	int result;

	sprintf(finalMsg,"LEAVE%c%d%c%d", 0x01, idClient, 0x01, idChannel);
	result = send2Server(finalMsg);

	channels[idChannel].idChannel = -1;
}

void ack_frame(int idFrame, int cmd_i, char* cmd_s, int result)
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

	send2Server(rsp);

}


