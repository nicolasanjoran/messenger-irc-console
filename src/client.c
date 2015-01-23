
#define _GNU_SOURCE
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
#include <termios.h>

//========================== DEFINES =======================================//

#define NB_ENABLE 1
#define NB_DISABLE 2
#define clrscr() printf("\033[H\033[2J")
#define couleur(param) printf("\033[%sm",param)

#define SERVER_PORT 1500
#define MAX_MSG 100
#define MAX_CHANNELS 10
#define TIMEOUT 10

#define CMD_CONNECT 1
#define CMD_JOIN 2
#define CMD_ACK 3
#define CMD_TRANSMIT 4
#define CMD_ALIVE 5

#define FRAME_HIST_LEN 500

#define MAX_INPUT 300


//==========================================================================//

//=========================== GLOBAL VARs & Types ==========================//
struct channel {
	int idChannel;
	char* name;
	int file;
	char**msgs;
	int idMsg;
};

struct frameStruct{
	char active;
	char* msg;
	int resp;
	char serverAcked; // 0: Client is not concerned or already acked
	// 1: Client has not acked yet, wait 1s more.
	// 2: Server has waited for 1s, re-try to send frame.
};

struct Gterm{
	int width;
	int height;
	int sidebar_width;
};

struct channel channels[MAX_CHANNELS];
struct frameStruct framesHist[FRAME_HIST_LEN];
time_t timestamp;
struct tm * t;
struct Gterm gterm;

int idClient, sd, frameCounter, currentChannel,alive;
struct sockaddr_in client_addr, serv_addr;
char nickname[50];
char addr[20];
pthread_t pthreadReceiver, pthreadAckSystem, pthreadAlive;
socklen_t addr_len;
char input[MAX_INPUT];
int inputIdx;

//==========================================================================//


//============================ Fonctions ===================================//
int  SERVER_Connect();

void  SERVER_Disconnect();

// Return the idChannel
void CHANNEL_Join(char* channel);

void CHANNEL_Leave();

int send2Server(char *s);

void printInterface();

unsigned char getChecksum(char*s, int stringSize);

int analyzeFrame(char* rcvdFrame);

void transmit(int idChannel, char* message);

void ack_frame(int idFrame, int cmd_i, char* cmd_s, int result);

void * threadAckSystem(void * arg);

void * threadReceiver(void * arg);

void GRAPH_init();

void GRAPH_print();

void GRAPH_PrintSeparator(char separator, char*msg);

char inputAvailable();

void nonblock(int state);

void incCurrentChannel();

void analyzeMessage(char* frame);
void say(char* message);
char* time2string();
//==========================================================================//

//============================= MAIN =======================================//
int main (int argc, char *argv[])
{

	int i,j,k;
	inputIdx = 0;
	char msgbuf[MAX_MSG];
	frameCounter = 0;
	currentChannel = -1;
	idClient = -1;

	for(j=0 ; j<MAX_CHANNELS ; j++)
	{
		channels[j].name = "";
		channels[j].idChannel = -1;
		channels[j].msgs = malloc(100*sizeof(char*));
		channels[j].idMsg = 0;
		for(k = 0 ; k<100 ; k++)
		{
			channels[j].msgs[k] = malloc(MAX_MSG*sizeof(char));
		}
	}

	printf("Choisissez un pseudo : \n");
	fgets(nickname,50,stdin);


	if(strlen(nickname)>0)
	{
		nickname[strlen(nickname)-1] = '\0';
	}

	SERVER_Connect();

	system ("stty -echo");
	nonblock(NB_ENABLE);
	GRAPH_init();
	GRAPH_print();
	//callback
	while(1)
	{
		
		//system("clear");
		
		usleep(100000);
		if(inputAvailable())
		{
			char c;
			c=fgetc(stdin);
			if(c==127)
			{
				inputIdx--;
				input[inputIdx]='\0';
			}
			else if(c=='\n')
			{
				char messageInput[MAX_INPUT];
				strcpy(messageInput, input);
				analyzeMessage(messageInput);
				for(i=0 ; i<MAX_INPUT ; i++)
				{
					input[i] = 0;
				}
				inputIdx='\0';
			}
			else if(c=='\t')
			{
				incCurrentChannel();
			}
			else if(inputIdx < MAX_INPUT)
			{
				input[inputIdx]=c;
				inputIdx++;
			}
			GRAPH_print();
			fflush(stdin);
		}
		//printf("%s\n", tab);
		//TODO: Implement printInterface()
		//scanf("%s",msgbuf);
	}
	 /*
	while(1)
	{
		fgets(msgbuf,MAX_MSG,stdin);
		msgbuf[strlen(msgbuf)-1] = '\0';
		analyzeMessage(msgbuf);
	}
*/
	close(sd);
	return 0;
}
//==========================================================================//

int incFrameCounter()
{
	frameCounter = (frameCounter + 1) %FRAME_HIST_LEN;
	return frameCounter;
}

char inputAvailable()  
{
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}

void nonblock(int state)
{
    struct termios ttystate;
 
    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);
 
    if (state==NB_ENABLE)
    {
        //turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    }
    else if (state==NB_DISABLE)
    {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }
    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
 
}

void * threadReceiver(void * arg)
{
	int n;
	char msgbuf[MAX_MSG];

	for (;;)
	{
		addr_len = sizeof(serv_addr);
		memset(msgbuf,0,strlen(msgbuf));
		n = recvfrom(sd, msgbuf, MAX_MSG, 0, (struct sockaddr *)&serv_addr, &addr_len);
		if (n == -1)
			perror("recvfrom");
		else {
			//printf("received from %s: %s\n", inet_ntoa(serv_addr.sin_addr), msgbuf);
			analyzeFrame(msgbuf);
		}
	}
	return NULL;
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
				//free(framesHist[i].msg);
			}

		}
		return NULL;
	}
}

void * threadAlive(void * arg)
{
	int n;
	int live = 1;
	char msgbuf[MAX_MSG];

	while(live)
	{
		sleep(10000);
		if(alive == 1)
		{
			alive =0;
		}
		else
		{
			//server disconnected
			printf("Server closed\n");
			live = 0;
			SERVER_Disconnect();
		}
	}
}

void analyzeAck( int idFrame, int resp)
{
	if(idFrame >= 0 && idFrame < FRAME_HIST_LEN)
	{
		//printf("ACK reçu.\n");
		framesHist[idFrame].resp = resp;
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
	fgets(addr,20,stdin);
	if(strlen(addr)>0)
	{
		nickname[strlen(addr)-1] = '\0';
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(addr);

	if ( serv_addr.sin_addr.s_addr == ( in_addr_t)(-1))
	{
		printf("Invalid IP address format <%s>\n", addr);
		return -1;
	}
	serv_addr.sin_port = htons(SERVER_PORT);
	pthread_create(&pthreadReceiver,NULL,threadReceiver,NULL);
	pthread_create(&pthreadAckSystem,NULL,threadAckSystem,NULL);
	//pthread_create(&pthreadAlive,NULL,threadAlive,NULL);

	sprintf(finalMsg,"CONNECT%c%s", 0x01, nickname );
	result = send2Server(finalMsg);

	printf("Trying to connect...\n");
	while(framesHist[result].serverAcked != 0);
	idClient = framesHist[result].resp;
	printf("Connected as userID: %d\n", idClient);


	return result;
}

void SERVER_Disconnect()
{
	char finalMsg[MAX_MSG];

	sprintf(finalMsg,"DISCONNECT%c%d", 0x01, idClient );
	send2Server(finalMsg);
	//printf("Déconnecté du serveur\n");
	exit(0);
}

void CHANNEL_Join(char* channel){

	char finalMsg[MAX_MSG];
	int idChannel;
	int numtrame;

	sprintf(finalMsg,"JOIN%c%d%c%s", 0x01, idClient, 0x01, channel);

	numtrame = send2Server(finalMsg);


	while(framesHist[numtrame].serverAcked != 0);
	idChannel = framesHist[numtrame].resp;

	//fill channel information and open/create file

	char*idChannel_s = malloc(strlen(channel)+1);
	strcpy(idChannel_s, channel);
	char * filename = malloc(strlen(idChannel_s)+15);
	sprintf(filename,"channels/%s.hist",idChannel_s);
	channels[idChannel].idChannel = idChannel;
	channels[idChannel].name = idChannel_s;
	channels[idChannel].file = open(filename,O_CREAT | O_RDWR, 0666);
	currentChannel = idChannel;
}

void CHANNEL_Leave(){

	char finalMsg[MAX_MSG];
	int result;

	if(currentChannel != -1)
	{
		sprintf(finalMsg,"LEAVE%c%d%c%d", 0x01, idClient, 0x01, currentChannel);
		send2Server(finalMsg);
		channels[currentChannel].idChannel = -1;

		currentChannel =-1; //Get the next channel
	}
	else
	{
		printf("Vous n'êtes pas dans un salon");
	}
}

int send2Server(char* msg)
{
	int result,n,numTrame,i;
	char finalMsg[MAX_MSG];
	char msgbuf[MAX_MSG];

	numTrame = incFrameCounter();

	sprintf(finalMsg,"%s%c%d%c", msg,0x01,numTrame,0x01);
	unsigned char chkSum = getChecksum(finalMsg, strlen(finalMsg));

	sprintf(finalMsg,"%s%c",finalMsg,chkSum);

	framesHist[frameCounter].msg = finalMsg;
	framesHist[frameCounter].serverAcked = 1;

	//printf("SEND=> %s\n", finalMsg);
	/*for(i=0 ; i<strlen(finalMsg) ; i++)
	{
		printf("%02x ", finalMsg[i]);
	}
	printf("\n");*/

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
	return (sum%10)+97;
}

int analyzeFrame(char* frame)
{
	/*
	 * This is used for extracting fields from the received frame.
	 */
	char** extractedFrame;
	int cnt = 0;
	//char*rcvdFrame = malloc(strlen_int(frame));
	//memcpy(rcvdFrame, frame, strlen_int(frame));
	char rcvdFrame[MAX_MSG];
	strcpy(rcvdFrame, frame);
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
		//printf("idx=%d, rcvd: %s\n", idx, extractedFrame[idx]);
		if(extractedFrame[idx]==NULL) break;

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
	if(totalExtracted >= 3)
	{
		/*
		 * Checksum processing
		 */
		//printf("Processed checksum: %x    ::  Actual:%x\n",getChecksum(rcvdFrame, strlen(rcvdFrame)-1), (unsigned char)extractedFrame[totalExtracted-1][0]);
		//TODO Uncomment checksum checkers !!!
		if(0)//(unsigned char)extractedFrame[totalExtracted-1][0] != getChecksum(rcvdFrame, strlen(rcvdFrame)-1))
		{
			totalExtracted = 0;
		}else
		{
			//printf("idFrame: %d\n", atoi(extractedFrame[totalExtracted-2]));
			rcvdIdFrame = atoi(extractedFrame[totalExtracted-2]);
		}
	}

	/*
	 * This is to identify which kind of frame it is.
	 */
	int strSize = sizeof(extractedFrame[0])/sizeof(char);
	if(totalExtracted == 4 && strncmp(extractedFrame[0], "CONNECT", strSize) == 0)
	{
		cmd = CMD_CONNECT;
	}
	else if(totalExtracted >= 4 && strncmp(extractedFrame[0], "TRANSMIT", strSize) == 0)
	{
		transmit(atoi(extractedFrame[1]),extractedFrame[3]);
		cmd = CMD_TRANSMIT;
	}
	else if(totalExtracted == 5 && strncmp(extractedFrame[0], "ACK", strSize) == 0)
	{
		cmd = CMD_ACK;
		analyzeAck(atoi(extractedFrame[1]), atoi(extractedFrame[2]));
		result = atoi(extractedFrame[1]);
	}
	else if(totalExtracted == 3 && strncmp(extractedFrame[0], "ALIVE", strSize) == 0)
	{
		cmd = CMD_ALIVE;
		result = atoi(extractedFrame[1]);
	}
	else
	{
		result = -1;
		//printf("Incoming frame does not correspond to expected scheme : ACK will not be granted.\n");
	}

	if(cmd != CMD_ACK)
	{
		ack_frame(rcvdIdFrame, cmd, extractedFrame[0], result);
	}

	return result;
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

	sprintf(rsp, "ACK%c%d%c%d%c%s%c", (char)0x01,idClient,0x01, idFrame, (char)0x01, rsp_value, 0x01);

	send2Server(rsp);
}

void analyzeMessage(char* frame)
{
	char cmdFrame[MAX_MSG];
	char parameter[MAX_MSG];
	alive = 1;

	strcpy(cmdFrame, frame);

	//Cherche si une commande a été tapée en premier
	if(strncmp(cmdFrame,"JOIN",strlen("JOIN")) == 0){

		if(strlen(cmdFrame) > strlen("JOIN")+1)
		{
			//Joint le salon précisé après la commande JOIN
			strncpy(parameter, cmdFrame + strlen("JOIN")+1,strlen(cmdFrame) - strlen("JOIN"));
			CHANNEL_Join(parameter);
		}
		else
		{
			printf("Paramètre non valide \n");
		}
	}
	else if(strncmp(cmdFrame,"LEAVE",strlen("LEAVE")) == 0)
	{
		CHANNEL_Leave();
	}
	else if(strncmp(cmdFrame,"DISCONNECT",strlen("DISCONNECT")) == 0)
	{
		SERVER_Disconnect();
	}
	else if(strncmp(cmdFrame,"NEXT",strlen("NEXT")) == 0)
	{
		if(strlen(cmdFrame) > strlen("NEXT")+1)
		{
			//next()
		}
		else
		{
			printf("Paramètre non valide \n");
		}
	}
	else
	{
		say(cmdFrame);
	}

}


//========================== CMD =======================================//

void say(char* message){
	char finalMsg[MAX_MSG];

	if(currentChannel >= 0)
	{
		//Contruit la chaine à envoyer
		sprintf(finalMsg,"SAY%c%d%c%d%c%s",0x01,idClient,0x01,currentChannel,0x01,message);

		send2Server(finalMsg);
		//Ecrit dans l'historique
		sprintf(finalMsg, "<<%s> -- %s> %s\n", nickname, time2string(), message);
		if(write(channels[currentChannel].file, finalMsg, strlen(finalMsg)) < 0)
		{
			perror("write");
		}

	}
	else
	{
		printf("Vous n'êtes pas dans un salon");
	}
}

void GRAPH_init()
{
	FILE *fp;
	char columns_nb[5];
	char lines_nb[5];
	fp = popen("tput cols", "r");
	if(fp == NULL)
	{
		perror("Cannot get terminal width.");
	}else{
		while(fgets(columns_nb, sizeof(columns_nb), fp) != NULL)
			gterm.width = atoi(columns_nb);
	}
	pclose(fp);

	fp = popen("tput lines", "r");
	if(fp == NULL)
	{
		perror("Cannot get terminal height.");
	}else{
		while(fgets(lines_nb, sizeof(lines_nb), fp) != NULL)
			gterm.height = atoi(lines_nb);
	}
	pclose(fp);

	gterm.sidebar_width=20;


}

void GRAPH_println(char*msg, int line)
{
	int i=0;
	printf("| ");
	while(i<strlen(msg) && i<gterm.width-gterm.sidebar_width-1)
	{
		printf("%c", msg[i]);
		i++;
	}
	while(i<gterm.width-gterm.sidebar_width-1)
	{
		printf(" ");
		i++;
	}
	printf("|");
	if(line==0)
	{
		for(i=0 ; i<(gterm.sidebar_width-8)/2 ; i++) printf(" ");
		printf("CHANNELS\n");
	}
	else if(line==1)
	{
		for(i=0 ; i<gterm.sidebar_width-2 ; i++) printf("-");
	}else{
		int j,k=1;
		char found=0;
		for(j=0 ; j<MAX_CHANNELS ; j++)
		{
			if(channels[j].idChannel != -1)
			{
				k++;
			}
			if(k==line)
			{
				found=1;
				break;
			}
		}
		if(found==1)
		{
			if(j==currentChannel) printf("\033[30m\033[47m");
			printf("%s\n", channels[j].name);
			couleur("0");
		}else{
			for(i=0 ; i<gterm.sidebar_width-2 ; i++) printf(" ");
		}
	}
}

void GRAPH_PrintSeparator(char separator, char* msg)
{
	int i;
	int msgSize = 0;
	if(msg!=NULL)
	{
		msgSize=strlen(msg);
	}

	for(i=0; i<(gterm.width-msgSize)/2 ; i++)
	{
		printf("%c", separator);
	}
	if(msgSize > 0)
	{
		printf("%s", msg);
	}
	for(; i+msgSize<gterm.width ; i++)
	{
		printf("%c", separator);
	}

}

void GRAPH_print()
{
	int i,j,start=-1;
	size_t len=MAX_MSG;

	GRAPH_init();
	int idx = 0;
	//char** messages;
	system("clear");
	GRAPH_PrintSeparator('-', "[MESSENGER 1.0]");
	/*for(j=0 ; j<=gterm.height-4 ; j++)
	{
		messages[j] = malloc(MAX_MSG*sizeof(char));
	}*/
	if(currentChannel!=-1)
	{
		//messages = channels[currentChannel].msgs;
		//printf("idx: %d", channels[currentChannel].idMsg);
		idx = channels[currentChannel].idMsg;
		/*const char delimiter = '\n';
		int nbread;
		FILE * filedesc;
		char * filename = malloc(strlen(channels[currentChannel].name)+15);
		sprintf(filename,"channels/%s.hist",channels[currentChannel].name);
		filedesc = fopen(filename, "r");
		//while(fscanf(filedesc, "%[^\n]", messages[idx])!=EOF)
		while(nbread=getline(&messages[idx], &len, filedesc) > 0)
		{
			
			messages[idx] = strtok(messages[idx], &delimiter);
			printf("strlen: %lu idx: %d, rd: %d %s\n",strlen(messages[idx]),idx, nbread, messages[idx]);
			idx = (idx+1)%(gterm.height-4);
			
			
		}*/
		int nbLignes = 0;
		/*for(j=0 ; j<=100 && nbLignes <= 100  ; j++)
		{
			if(messages[(idx-j+100)%100] != NULL)
			{
				nbLignes += strlen(messages[(idx-j+100)%100])/(gterm.width-gterm.sidebar_width-3);
			}
		}*/
		
		start = (idx-gterm.height+4+100)%100;
		int end = (idx);
		//printf("start: %d, end: %d\n", start, (start+gterm.height+100)%100);
		//printf("nblignes: %d\n", nbLignes);

		if(start==-1) start=0;
		int lines = 0;
		for (i = start; i != end; i=(i+1)%100)
		{
			//GRAPH_println("test!!", i);
			//printf("%s", channels[currentChannel].msgs[(start+i)%100]);
			if(channels[currentChannel].msgs[i] != NULL)
			{
				GRAPH_println(channels[currentChannel].msgs[i], lines);
				lines++;
				//printf("%s\n", messages[(start+i)%100]);
				//printf("t\n");
				//GRAPH_println("t", i);
			}
			else
			{
				//GRAPH_println("", i);
			}
			//printf("%s\n", messages[(start+i)%gterm.height-4]);
		}
	}else{
		for(i=0 ; i<=gterm.height-5 ; i++)
		{
			GRAPH_println("", i);
		}
	}



	
	GRAPH_PrintSeparator('-', NULL);
	printf(" >> %s\n", input);
	GRAPH_PrintSeparator('-', "[Press Tab: Switch channel]");
	fflush(stdout);



}

void incCurrentChannel()
{
	int i=0;
	int nbchannel = currentChannel;
	int result  = -1;
	if(nbchannel == -1) nbchannel = 0;
	for(; i<10 ; i++)
	{
		if(channels[(i+nbchannel)%MAX_CHANNELS].idChannel != -1)
		{
			result = channels[(i+nbchannel)%MAX_CHANNELS].idChannel;
			break;
		}
	}
	if(result != -1)
	{
		currentChannel = result;
	}
}

void transmit(int idChannel, char* message){

	int result;
	

	if(channels[idChannel].idChannel != -1)
	{
		strcpy(channels[idChannel].msgs[channels[idChannel].idMsg], message);
		printf("%s id:%d\n", channels[idChannel].msgs[channels[idChannel].idMsg],channels[idChannel].idMsg);
		channels[idChannel].idMsg = (channels[idChannel].idMsg + 1)%100;

		/*if((result = write(channels[idChannel].file, message, strlen(message))) < 0)
		{
			perror("write");
		}*/
	}
	else
	{
		//channel not joined
	}
	GRAPH_print();
}

char* time2string()
{
	char* stringTime = malloc(20);
	timestamp = time(NULL);
    t = localtime(&timestamp);
    sprintf(stringTime, "%02d-%02d-%04d %02d:%02d:%02d", t->tm_mday, t->tm_mon+1, t->tm_year+1900, t->tm_hour, t->tm_min, t->tm_sec);

    return stringTime;
}


