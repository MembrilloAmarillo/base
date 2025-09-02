/**
 * @author Sascha Paez
 * @date   25 July 2025
 * This library is developed for the purpose of receiving commands from the UCA Mission Control
 * System. The way this commanding is designed serves the sole purpose of easy commanding, with
 * little to no overhead.
 */
#ifndef _COMMANDING_H_
#define _COMMANDING_H_

#include "types.h"
#include "net.h"

typedef enum command_id command_id;
enum command_id {
	FTP_DOWNLOAD,
};

typedef struct ucf_command ucf_command;
struct ucf_command {
	command_id ID;
	union {
		U8* Data;
		struct {
			U8* Info;
			U32 Idx;
		};
	};
};

typedef struct udp_command udp_command;
struct udp_command {
	udp_socket   Socket;
	ucf_command* Commands;
};

void InitCommander( udp_command* CommandServer );

#endif // _COMMANDING_H_

#ifdef COMMANDING_IMPL

void InitCommander( udp_command* CommandServer ) {
   
}

#endif // COMMANDING_IMPL