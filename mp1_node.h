/**********************
*
* Progam Name: MP1. Membership Protocol.
* 
* Code authors: Chaitanya Datye
*
* Current file: mp2_node.h
* About this file: Header file.
* 
***********************/

#ifndef _NODE_H_
#define _NODE_H_
#define TFAIL 10                 /* TFail */
#define TCLEANUP 30              /* TCleanup */

#include "stdincludes.h"
#include "params.h"
#include "queue.h"
#include "requests.h"
#include "emulnet.h"

/* Configuration Parameters */
char JOINADDR[30];                    /* address for introduction into the group. */
extern char *DEF_SERVADDR;            /* server address. */
extern short PORTNUM;                /* standard portnum of server to contact. */


/* Miscellaneous Parameters */
extern char *STDSTRING;

/* Structure for the membershiplist */
typedef struct membershiplist {
    struct address addr;                //address of the node in the list
    long heartbeat;                     //last value heartbeat counter updated in the list for the node
    int time;                           //time at which the heartbeat was received
    int status_failed;                  //status whether the node is up or suspected as failed
} membershiplist;

typedef struct member{            
        struct address addr;            // my address
        int inited;                     // boolean indicating if this member is up
        int ingroup;                    // boolean indiciating if this member is in the group

        queue inmsgq;                   // queue for incoming messages

        int bfailed;                    // boolean indicating if this member has failed
        struct membershiplist * membershiplist;  // membership list of the current node
        int current_membershiplist_size;       //how many nodes currently in the membershiplist
} member;

/* Message types */
/* Meaning of different message types
  JOINREQ - request to join the group
  JOINREP - replyto JOINREQ
  GOSSIP - gossip about the membershiplist to a random node
*/
enum Msgtypes{
		JOINREQ,			
		JOINREP,
        GOSSIP,
		DUMMYLASTMSGTYPE
};

/* Generic message template. */
typedef struct messagehdr{ 	
	enum Msgtypes msgtype;
} messagehdr;


/* Functions in mp2_node.c */

/* Message processing routines. */
STDCLLBKRET Process_joinreq STDCLLBKARGS;
STDCLLBKRET Process_joinrep STDCLLBKARGS;
STDCLLBKRET Process_gossip STDCLLBKARGS;

/*
int recv_callback(void *env, char *data, int size);
int init_thisnode(member *thisnode, address *joinaddr);
*/

/*
Other routines.
*/

void nodestart(member *node, char *servaddrstr, short servport);
void nodeloop(member *node);
int recvloop(member *node);
int finishup_thisnode(member *node);

#endif /* _NODE_H_ */

