
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>

#include <enet/enet.h>

volatile sig_atomic_t stop = 0;
void sigint_handle(int signum)
{
	if (signum == SIGINT)
		stop = 1;
	return;
}

int main(int argc, char **argv)
{
	signal(SIGINT, sigint_handle);
	
	ENetHost *client;
	client = enet_host_create(NULL, 1, 2, 0, 0);
	if (client == NULL) {
		fprintf(stderr, "Error while trying to create an ENet client host.\n");
		exit(EXIT_FAILURE);
	}
	ENetAddress address;
	ENetEvent event;
	ENetPeer *peer;
	
	enet_address_set_host(&address, "127.0.0.1");
	if (argc >= 2)
		address.port = atoi(argv[1]);
	else
		return -1;		

	peer = enet_host_connect(client, &address, 2, 0);
	if (peer == NULL) {
		fprintf(stderr, "no available peers for initializing an ENet connection.\n");
		exit(EXIT_FAILURE);
	}
	
	int check;
	while(!stop && check >= 0) {
		check = enet_host_service(client, &event, 3000);
		if (check > 0) {
			switch(event.type) {
				case ENET_EVENT_TYPE_CONNECT:
					puts("Connection succeeded.");
					
					const char *message = argv[2] ? argv[2] : "that's a packet sent by the client";
					ENetPacket *packet = enet_packet_create(message, strlen(message) + 1, ENET_PACKET_FLAG_RELIABLE);
					enet_peer_send(peer, 0, packet);
					break;
				case ENET_EVENT_TYPE_RECEIVE:
					if (event.packet)
						printf("Message received says '%s'\n", event.packet->data);
						enet_packet_destroy(event.packet);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					puts("Disconnection succeeded.");
					enet_host_destroy(client);
					break;
			}
		}
	}
	enet_peer_reset(peer);	
	return -1;
}
