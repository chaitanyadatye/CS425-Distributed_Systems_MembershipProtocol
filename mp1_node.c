/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: Chaitanya Datye
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"


/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
int isnulladdr( address *addr){
    return (memcmp(addr, NULLADDR, 6)==0?1:0);
}

/* 
 *
 * This function compares two adresses
 * Returns 0 if they are same, else returns 1
 * 
 */
int compare_address(address a, address b) {
    if(a.addr[0] == b.addr[0] && a.addr[1] == b.addr[1] && a.addr[2] == b.addr[2] && a.addr[3] == b.addr[3] && a.addr[4] == b.addr[4]){
        return 0;
    }
    else
        return 1;
}

/* 
Return the address of the introducer member. 
*/
address getjoinaddr(void){

    address joinaddr;

    memset(&joinaddr, 0, sizeof(address));
    *(int *)(&joinaddr.addr)=1;
    *(short *)(&joinaddr.addr[4])=0;

    return joinaddr;
}

/* 
 * When any node joins, its membership list is initialized
 */

void initialize_membershiplist(void *env) {
    
    member * node = (member *)env;
    
    struct membershiplist initial_membershiplist;
    memset(&initial_membershiplist.addr, 0, sizeof(address));
    initial_membershiplist.heartbeat = 0;
    initial_membershiplist.status_failed = 0;
    initial_membershiplist.time = 0;
    
    for(int i=0; i<MAX_NNB; i++){
        node->membershiplist[i] = initial_membershiplist;
    }
    
}

/*
 *
 * When Introducer receives any new JOINREQ, it adds that node to its membership list.
 *
 */
void addNodeToMembershipList(void *env, address *addr)
{
    member *node = (member *)env;
    
    node->membershiplist[node->current_membershiplist_size].addr = *addr;
    node->membershiplist[node->current_membershiplist_size].time = getcurrtime();
    node->membershiplist[node->current_membershiplist_size].heartbeat++;
    node->membershiplist[node->current_membershiplist_size].status_failed = 0;
    
    return;
    
}


/*
 *
 * Message Processing routines.
 *
 */

/* 
Received a JOINREQ (joinrequest) message.
*/
void Process_joinreq(void *env, char *data, int size)
{
    char s[sizeof(membershiplist)*MAX_NNB + sizeof(int)]; //size of the membershiplist. Int used to mark end
    memset(&s, 0, sizeof(membershiplist)*MAX_NNB+sizeof(int));
    char s1[1024];
    int seeker=0;               //used while serializing the list into a single string for sending
    
    messagehdr *msg;
    member *node = (member *)env;
    
    //add the new node to introducer's list
    addNodeToMembershipList(node, (address *)data);
    node->current_membershiplist_size += 1;
#ifdef DEBUGLOG
    logNodeAdd(&node->addr, (address *)data);
#endif
    
    size_t msgsize = sizeof(messagehdr)+sizeof(membershiplist)*MAX_NNB+sizeof(int);
    msg=malloc(msgsize);
    
    /* create JOINREP message: format of data is {struct address myaddr} */
    msg->msgtype=JOINREP;
    
    /*serialize the membershiplist */
    for(int i=0; i<node->current_membershiplist_size; i++){
    memcpy(&s[seeker], &node->membershiplist[i], sizeof(membershiplist));
    seeker+=sizeof(membershiplist);
    }
    
    /* pack list in a message */
    memcpy((char *)(msg+1), s, sizeof(membershiplist)*MAX_NNB+sizeof(int));
    
#ifdef DEBUGLOG
    sprintf(s1, "Processing request of ..");
    LOG(&node->addr, s1);
#endif
    /* send JOINREP message along with the introducer's membershiplist to the node. */
    MPp2psend(&node->addr, (address *)data, (char *)msg, msgsize);
    
    free(msg);
    return;
}

/* 
Received a JOINREP (joinreply) message. 
*/
void Process_joinrep(void *env, char *data, int size)
{
    member *node = (member *)env;
    
    //on receiveing the JOINREP from introducer, change ingroup status to 1
    node->ingroup = 1;
    
    membershiplist * membershiplist_received = (membershiplist *)(data);
    
    for(int i=0; i<MAX_NNB; i++) {
        //break if the address in the membership list starts with 0. Indicates that all nodes from list are taken care of
        if(membershiplist_received[i].addr.addr[0]==0){
            break;
        }
        node->membershiplist[i] = membershiplist_received[i];
        node->membershiplist[i].time = getcurrtime();
#ifdef DEBUGLOG
        logNodeAdd(&node->addr, &membershiplist_received[i].addr);
#endif
        node->current_membershiplist_size++;
    }
    
#ifdef DEBUGLOG
    LOG(&node->addr, "Received JOINREP message\n");
#endif
    return;
}

/*
 * 
 * Function is called when a node receives a GOSSIP msg from some other node
 * Node updates its membership list based on the list received in the GOSSIP message
 *
 */
void Process_gossip(void *env, char *data, int size)
{
    member *node = (member *)env;
    int received_list_size = 0;
    membershiplist * membershiplist_received = (membershiplist *)(data);
    
    //printf("\n\nTHIS IS GOSSIP_REP_ADDRESS %d.%d.%d.%d:%d\n", node->addr.addr[0], node->addr.addr[1],node->addr.addr[2],node->addr.addr[3],node->addr.addr[4]);

    //get the number of nodes in the list received. Since, list is serialized, we need to find this
    for(int i=0; i<MAX_NNB; i++){
        if(isnulladdr(&membershiplist_received[i].addr))
            break;
        received_list_size++;
    }
    
    //update self's membership list based on the received list
    int i=0;
    for(i=0; i<received_list_size; i++) {

        int j=0;
        int found=0; // flag tells whether node is in your list or not
    
        //if entry corresponds to self, no need to update. ignore entry and continue
        if(compare_address(node->addr, membershiplist_received[i].addr) == 0){
            printf("This is me.. Don't update\n");
            continue;
        }
      
        //if some node in received list is marked as suspected failure, no need to update
        if(membershiplist_received[i].status_failed == 1){
            printf("Failed nodes don't need to update\n");
            continue;
        }
        
        //for all other nodes in received list, check if they are in self's list and update accordingly
        for(j=0; j<node->current_membershiplist_size; j++) {
            
            if(compare_address(node->membershiplist[j].addr, membershiplist_received[i].addr) == 0) {
                printf("Node found in my list\n");
                
                //if status in your list is suspected failed but the received list has an updated counter, change status to not failed and update heartbeat counter in your list
                if(node->membershiplist[j].status_failed == 1){
                    if(node->membershiplist[j].heartbeat < membershiplist_received[i].heartbeat) {
                        node->membershiplist[j].heartbeat = membershiplist_received[i].heartbeat;
                        node->membershiplist[j].time = getcurrtime();
                        node->membershiplist[j].status_failed = 0;
                    }
                    found =1;
                    break;
                }
                
                //if heartbeat received is greater than that in your list, update
                if(node->membershiplist[j].heartbeat < membershiplist_received[i].heartbeat) {
                    node->membershiplist[j].heartbeat = membershiplist_received[i].heartbeat;
                    node->membershiplist[j].time = getcurrtime();
                    found = 1;
                    break;
                }
                //else just say found and break out of the loop
                found = 1;
                break;
    
            }
        }
        
            //if member is not in your list, add it to your list
            if(found == 0 && node->current_membershiplist_size<MAX_NNB){
                //printf(" %d *** ADDING TO LIST NEW NODE\n", j);
                
                node->current_membershiplist_size++;
                node->membershiplist[node->current_membershiplist_size-1] = membershiplist_received[i];
                node->membershiplist[node->current_membershiplist_size-1].heartbeat = membershiplist_received[i].heartbeat;
                node->membershiplist[node->current_membershiplist_size-1].time = getcurrtime();
                
                #ifdef DEBUGLOG
                    logNodeAdd(&node->addr, &node->membershiplist[node->current_membershiplist_size-1].addr);
                #endif
            }
    }
    
#ifdef DEBUGLOG
    LOG(&node->addr, "Received gossip message\n");
#endif
    return;
}

/*
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep,
    Process_gossip
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (char *)(msghdr+1);
    
    if(size < sizeof(messagehdr)){
#ifdef DEBUGLOG
        LOG(&((member *)env)->addr, "Faulty packet received - ignoring");
#endif
        return -1;
    }

#ifdef DEBUGLOG
    LOG(&((member *)env)->addr, "Received msg type %d with %d B payload", msghdr->msgtype, size - sizeof(messagehdr));
#endif

    if((node->ingroup && msghdr->msgtype >= 0 && msghdr->msgtype <= DUMMYLASTMSGTYPE)
        || (!node->ingroup && msghdr->msgtype==JOINREP))            
            /* if not yet in group, accept only JOINREPs */
        MsgHandler[msghdr->msgtype](env, pktdata, size-sizeof(messagehdr));
    /* else ignore (garbled message) */
    free(data);

    return 0;

}

/*
 *
 * Initialization and cleanup routines.
 *
 */

/* 
Find out who I am, and start up. 
*/
int init_thisnode(member *thisnode, address *joinaddr){
    
    if(MPinit(&thisnode->addr, PORTNUM, (char *)joinaddr)== NULL){ /* Calls ENInit */
#ifdef DEBUGLOG
        LOG(&thisnode->addr, "MPInit failed");
#endif
        exit(1);
    }
#ifdef DEBUGLOG
    else LOG(&thisnode->addr, "MPInit succeeded. Hello.");
#endif

    thisnode->bfailed=0;
    thisnode->inited=1;
    thisnode->ingroup=0;
    thisnode->membershiplist = (membershiplist *)malloc(MAX_NNB * sizeof(membershiplist));
    initialize_membershiplist(thisnode);
    thisnode->current_membershiplist_size = 0;
    /* node is up! */

    return 0;
}


/* 
Clean up this node. 
*/
int finishup_thisnode(member *node){
 
    free(node->membershiplist);
    return 0;
}


/* 
 *
 * Main code for a node 
 *
 */

/* 
Introduce self to group. 
*/
int introduceselftogroup(member *node, address *joinaddr){
    
    messagehdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if(memcmp(&node->addr, joinaddr, 4*sizeof(char)) == 0){
        /* I am the group booter (first process to join the group). Boot up the group. */
#ifdef DEBUGLOG
        LOG(&node->addr, "Starting up group...");
#endif
        
        addNodeToMembershipList(node, &node->addr);
        node->current_membershiplist_size += 1;
#ifdef DEBUGLOG
        logNodeAdd(&node->addr, &node->addr);
#endif
        node->ingroup = 1;
    }
    else{
        size_t msgsize = sizeof(messagehdr) + sizeof(address);
        msg=malloc(msgsize);

    /* create JOINREQ message: format of data is {struct address myaddr} */
        msg->msgtype=JOINREQ;
        memcpy((char *)(msg+1), &node->addr, sizeof(address));
        
#ifdef DEBUGLOG
        
        sprintf(s, "Trying to join...");
        LOG(&node->addr, s);
#endif

    /* send JOINREQ message to introducer member. */
        MPp2psend(&node->addr, joinaddr, (char *)msg, msgsize);
        
        free(msg);
    }

    return 1;

}

/* 
Called from nodeloop(). 
*/
void checkmsgs(member *node){
    void *data;
    int size;

    /* Dequeue waiting messages from node->inmsgq and process them. */
	
    while((data = dequeue(&node->inmsgq, &size)) != NULL) {
        recv_callback((void *)node, data, size);
    }
    return;
}


/*
Executed periodically for each member. 
Performs necessary periodic operations. 
Called by nodeloop(). 
 * Increment heartbeat counter
 * Cleanup failed nodes based on TCLEANUP
 * Mark nodes as suspected failures based on TFAIL
 * Gossip membershiplist to one random node
*/
void nodeloopops(member *node){

    //update heartbeat counter of self
    int cur_size = node->current_membershiplist_size;
    printf("CURR SIZE %d\n", cur_size);
    printf("Curr TIME %d\n", getcurrtime());
    for(int i=0; i<cur_size; i++){
        if(compare_address(node->membershiplist[i].addr, node->addr) ==0) {
            node->membershiplist[i].heartbeat++;
            node->membershiplist[i].time = getcurrtime();
            break;
        }
    }
    
    // Cleanup failed nodes
    for(int i=0; i<cur_size; i++){
        if((node->membershiplist[i].status_failed==1) && (getcurrtime() - node->membershiplist[i].time >= TCLEANUP)) {
            //printf("Cleaning up at time %d\n" ,getcurrtime());
            
            /* clean the node and swap it with the last node in the membership list.
             * decrement size of membership list by one
             * decrement i since you need to consider the swapped node
             */
            
            struct membershiplist cleanup; //cleanup node initialized to 0
            memset(&cleanup.addr, 0, sizeof(address));
            cleanup.heartbeat = 0;
            cleanup.status_failed = 0;
            cleanup.time = 0;
#ifdef DEBUGLOG
          logNodeRemove(&node->addr, &node->membershiplist[i].addr);
#endif
            //swap node
            node->membershiplist[i] = node->membershiplist[cur_size-1];
            node->membershiplist[cur_size-1] = cleanup;
            node->current_membershiplist_size--;
            cur_size--;
            i--;
        }
    }
 
    //mark nodes as failed
    for(int i=0; i<cur_size; i++){
        if(getcurrtime() - node->membershiplist[i].time >= TFAIL) {
            printf("Mark for failure\n");
            node->membershiplist[i].status_failed=1;
        }
    }
 
//choose random person and gossip
 
// choose when to gossip

    int gossip_flag = 0;
    int failed_nodes = 0;
    
    //if all nodes in my list are marked as suspected failures, I will not gossip
    for(int i=0; i<node->current_membershiplist_size; i++){
        if(node->membershiplist[i].status_failed == 1)
            failed_nodes++;
    }
    
    //if I am the only person in my list, I will not gossip
    if(node->current_membershiplist_size > 1 && failed_nodes < node->current_membershiplist_size-1)
        gossip_flag = 1;
    
   if(gossip_flag == 1){
        int random_node = rand() % cur_size;
        address gossip_address = node->membershiplist[random_node].addr;
        //printf("Random node : %d Curr size %d\n", random_node, cur_size);
    
       //if random number generated my address or address of suspected failed node, choose another node to gossip
        while(compare_address(gossip_address, node->addr)==0 || node->membershiplist[random_node].status_failed == 1) {
            //not to gossip to failed node
    
            random_node = rand() % cur_size;
            gossip_address = node->membershiplist[random_node].addr;
        }
       
        messagehdr *msg;
        size_t msgsize = sizeof(messagehdr)+sizeof(membershiplist)*MAX_NNB+sizeof(int);
        msg=malloc(msgsize);
    
        /* create GOSSIP message */
        msg->msgtype=GOSSIP;
        char s[sizeof(membershiplist)*MAX_NNB + sizeof(int)];
        memset(s, 0, sizeof(membershiplist)*MAX_NNB + sizeof(int));
        int seeker = 0;
       
        /*serialize the membershiplist */
        for(int i=0; i<cur_size; i++){
            memcpy(&s[seeker], &node->membershiplist[i], sizeof(membershiplist));
            seeker+=sizeof(membershiplist);
        }
       
        /* pack list in a message */
        memcpy((char *)(msg+1), s, sizeof(membershiplist)*MAX_NNB+sizeof(int));
       
        /* Send GOSSIP message with membership list to the random node*/
        MPp2psend(&node->addr, &gossip_address, (char *)msg, msgsize);

        free(msg);
    }
    return;
}

/* 
Executed periodically at each member. Called from app.c.
*/
void nodeloop(member *node){
    if (node->bfailed) return;

    checkmsgs(node);

    /* Wait until you're in the group... */
    if(!node->ingroup) return ;

    /* ...then jump in and share your responsibilites! */
    nodeloopops(node);
    
    return;
}

/* 
All initialization routines for a member. Called by app.c. 
*/
void nodestart(member *node, char *servaddrstr, short servport){

    address joinaddr=getjoinaddr();

    /* Self booting routines */
    if(init_thisnode(node, &joinaddr) == -1){

#ifdef DEBUGLOG
        LOG(&node->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if(!introduceselftogroup(node, &joinaddr)){
        finishup_thisnode(node);
#ifdef DEBUGLOG
        LOG(&node->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/* 
Enqueue a message (buff) onto the queue env. 
*/
int enqueue_wrppr(void *env, char *buff, int size){return enqueue((queue *)env, buff, size);}

/* 
Called by a member to receive messages currently waiting for it. 
*/
int recvloop(member *node){
    if (node->bfailed) return -1;
    else return MPrecv(&(node->addr), enqueue_wrppr, NULL, 1, &node->inmsgq); 
    /* Fourth parameter specifies number of times to 'loop'. */
}

