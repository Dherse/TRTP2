#include <sys/types.h>          
#include <sys/socket.h>
#include "../headers/packet.h"

//TODO1: gérer errno, 
//TODO2: potentiellement passer le socket en argument pour le réutiliser ou autre
//TODO3: avoir un buffer uint8_t permanent pour réceptionner les messages sans allocations
//TODO4: rajouter un paramètre payload pour le passer en argument à unpack
//TODO5: rename function =p
/**
 *  ## Use :
 *
 * creates a socket, listens and unpacks a message.
 * 
 * ## Arguments :
 *
 * - `src_addr` - a pointer to the address you want to listen to
 * - `addrlen` - the size of the address contained in 'src_addr'
 * - `packet` - pointer to a packet that has been allocated
 * - `sender_addr` - pointer to a buffer in which the source of the packet will be stored
 * - `sender_addr_len` - before the call : points to the size of the buffer sender_addr
 *                     - after the call : points to the size of the address stored in sender_addr
 * 
 * ## Return value:
 *
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 */
 int recv_and_handle_message(const struct sockaddr *src_addr, socklen_t addrlen, packet_t* packet, struct sockaddr * sender_addr, socklen_t* sender_addr_len);