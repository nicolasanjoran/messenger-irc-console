#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//========================== DEFINES =======================================//
#define SERVER_PORT 1500
#define MAX_MSG 100
#define MAX_CHANNELS 10
#define TIMEOUT 10

#define CMD_CONNECT 1
#define CMD_JOIN 2
#define CMD_ACK 3
#define CMD_TRANSMIT 4
//==========================================================================//

//=========================== GLOBAL VARs & Types ==========================//
struct channel {
	int idChannel;
	char* name;
	int file;

};

struct channel channels[MAX_CHANNELS];
int idClient, sd, numTrame;
struct sockaddr_in client_addr, serv_addr;
//==========================================================================//


//============================ Fonctions ===================================//
//TODO: implement connect
void  SERVER_Connect();

void  SERVER_Disconnect();

// Return the idChannel
void CHANNEL_Connect(char* channel);

void CHANNEL_Disconnect(int idChannel);

int send2Server(char *s);

void printInterface();

unsigned char getChecksum(char*s, int stringSize);

void analyzeFrame(char* rcvdFrame, struct sockaddr_in addr_client_frame);
//==========================================================================//


//============================= MAIN =======================================//
int main (int argc, char *argv[])
{

	int i;
	char msgbuf[MAX_MSG];

	// Command-line error check
	if (argc < 3)
		return 1;
	printf("%s: trying to send to %s\n", argv[0], argv[1]);

	// Create socket
	if ((sd = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
	{
		perror(argv[0]);
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
	serv_addr.sin_family = AF_INET;
	if (inet_aton(argv[1], &(serv_addr.sin_addr)) == 0)
	{
		printf("Invalid IP address format <%s>\n", argv[1]);
		return 1;
	}
	else
	{
		printf("Connecté au serveur <%s>\n", argv[1]);

	}
	serv_addr.sin_port = htons(SERVER_PORT);

	//TODO : changer nom msgbuf
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

int send2Server(char* msg)
{
	unsigned char chkSum = getChecksum(msg, strlen(msg));
	int result;
	char finalMsg[MAX_MSG];

	fd_set rfds;
	struct timeval tv;
	int retval;
	FD_ZERO(&rfds);
	FD_SET(sd,&rfds);
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	sprintf(finalMsg,"%s%c%c", msg, 0x01, chkSum);

	if (sendto(sd, finalMsg, strlen(finalMsg) + 1, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
	{
		perror("sendto");
		result = -1;
	}
	else
	{
		retval = select(sd+1,&rfds, NULL,NULL,&tv);
		if(retval == -1)
		{
			perror("select()");
			result = -2;
		}
		else if(retval)
		{
			//TODO: look for acknowledgment
		}
		else
		{
			printf("send2Server timeout");
			result = -3;
		}
		return result;
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

void analyzeFrame(char* rcvdFrame, struct sockaddr_in addr_client_frame)
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
		idClient = result;
		cmd = CMD_CONNECT;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "TRANSMIT", strSize) == 0)
	{
		result = transmit(atoi(extractedFrame[1]));
		cmd = CMD_TRANSMIT;
	}
	else if(totalExtracted == 4 && strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{
		//TODO ACK system.
		cmd = CMD_ACK;
	}else{
		result = -1;
		printf("%s: Incoming frame does not correspond to expected scheme : ACK will not be granted.\n", time2string());
	}

	//TODO Produce response (ACK).
	if(cmd != CMD_ACK)
	{
		ack_frame(rcvdIdFrame, idClient, cmd, extractedFrame[0], addr_client_frame, result);
	}

}

int transmit(int idChannel, char* message){

	int result;

	if((result = write(channels[idChannel].file, message, strlen(message))) < 0)
	{
			perror("write");
	}

}

void CHANNEL_Connect(char* channel){

	char finalMsg[MAX_MSG];

	sprintf(finalMsg,"JOIN%c%d%c%s", 0x01, idClient, 0x01, channel);

	send2Server(finalMsg);
}

void CHANNEL_Disconnect(int idChannel){

	char finalMsg[MAX_MSG];
	int result;

	sprintf(finalMsg,"LEAVE%c%d%c%d", 0x01, idClient, 0x01, idChannel);

	result = send2Server(finalMsg);
}

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

