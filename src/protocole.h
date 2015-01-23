#ifndef __PROTOCOL_H
#define __PROTOCOL_H

//========================== DEFINES =======================================//
#define clrscr() printf("\033[H\033[2J")
#define couleur(param) printf("\033[%sm",param)

//Config server
#define SERVER_PORT 1500
#define MAX_MSG 1500
#define MAX_CLIENTS 100
#define MAX_CLIENTS_digits 3
#define MAX_CHANNELS 10
#define FRAME_HIST_LEN 500

//Cmd id
#define CMD_CONNECT 1
#define CMD_JOIN 2
#define CMD_SAY 3
#define CMD_LEAVE 4
#define CMD_DISCONNECT 5
#define CMD_ACK 6
#define CMD_TRANSMIT 7
#define CMD_ALIVE 8

#define TRANSMIT_ALL -123
#define MSG_CLIENTS 0
#define MSG_TEXT 1
#define MSG_HIST 2
#define MAX_INPUT 300

#define NB_ENABLE 1
#define NB_DISABLE 2
//==========================================================================//

#endif
