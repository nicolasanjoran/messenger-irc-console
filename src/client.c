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
//==========================================================================//

//=========================== GLOBAL VARs & Types ==========================//
struct channel {
	int idChannel;
	char* name;
};

struct channels channel[MAX_CHANNELS];
int idClient, sd;
struct sockaddr_in client_addr, serv_addr;
//==========================================================================//


//============================ Fonctions ===================================//
//TODO: implement connect
void  SERVER_Connect();

void  SERVER_Disconnect();

// Return the idChannel
int CHANNEL_Connect(char* channel);

void CHANNEL_Disconnect(int channel);

int sendMessage(char *s);


void printInterface();
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


int sendMessage(char *s)
{
	int n, retval;
	char *msgbuf;
	struct timeval tv;

	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;

	fd_set rfds; //Ensemble des descripteurs des fichiers

	//Ajout du socket
	FD_ZERO(&rfds);
	FD_SET(sd, &rfds);

	socklen_t addr_len = sizeof(serv_addr);
	if (sendto(sd, *s, strlen(*s) + 1, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
	{
		perror("sendto");
		return 1;
	}
	else
	{
		retval = select(sd + 1, &rfds, NULL, NULL, &tv);
		if(retval == -1)
		{
			perror("select()");
		}
		else if(retval)
		{
			if(FD_ISSET(0, &rfds)){
				//TODO: lire
			}
		}
		else
		{
			//TODO: send again
			//printf("Aucune donnée disponible depuis %i secondes", tv.tv_sec);
		}

	}

}
