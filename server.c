
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#include <enet/enet.h>

#define LISTEN_PORT 42424
#define MAX_CLIENTS 5

typedef struct {
	char hostname[1024];
	enet_uint16 port;
} ServerInfo;

typedef struct {
	ENetSocket listen;
	ENetHost *host;
} ENetLANServer;

void listen_for_clients(ENetLANServer *server);
void send_string(ENetHost *host, char *string);

void stop_server(ENetLANServer *server);
bool init_server(ENetLANServer *server);

void sigint_handle(int signum);

volatile sig_atomic_t stop = 0;

void sigint_handle(int signum)
{
	if (signum == SIGINT)
		stop = 1;
	return;
}

bool init_server(ENetLANServer *server)
{
	if (enet_initialize() != 0) {
		fprintf(stderr, "Error occurred while initializing ENet.\n");
		return false;
	}
	server->listen = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
	if (server->listen == ENET_SOCKET_NULL) {
		fprintf(stderr, "Failed to created socket.\n");
		return false;
	}
	if (enet_socket_set_option(server->listen, ENET_SOCKOPT_REUSEADDR, 1) != 0) {
		fprintf(stderr, "Failed to enable reuse address.\n");
		return false;
	}
	ENetAddress listen_address;
	listen_address.host = ENET_HOST_ANY;
	listen_address.port = LISTEN_PORT;
	if (enet_socket_bind(server->listen, &listen_address) != 0) {
		fprintf(stderr, "Failed  to bind listen socket.\n");
		return false;
	}
	if (enet_socket_get_address(server->listen, &listen_address) != 0) {
		fprintf(stderr, "Cannot get listen socket address.\n");
		return false;
	}
	printf("Listening for scans on port %hu.\n", listen_address.port);
	 
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = ENET_PORT_ANY;
	server->host = enet_host_create(&address, MAX_CLIENTS, 2, 0, 0);
	if (server->host == NULL) {
		fprintf(stderr, "Failed to open ENet host.\n");
		return false;
	}
	printf("ENet host started on port %d.\n", server->host->address.port);
	return true;
}

void listen_for_clients(ENetLANServer *server)
{
	ENetSocketSet set;
	ENET_SOCKETSET_EMPTY(set);
	ENET_SOCKETSET_ADD(set, server->listen);
	if (enet_socketset_select(server->listen, &set, NULL, 0) <= 0)
		return;
	char buffer; ENetBuffer recv_buffer;
	ENetAddress recv_address;
	
	recv_buffer.data = &buffer;
	recv_buffer.dataLength = 1;
	const int recv_length = enet_socket_receive(server->listen, &recv_address, &recv_buffer, 1);
	if (recv_length <= 0)
		return;
	char address_buffer[256];
	enet_address_get_host_ip(&recv_address, address_buffer, sizeof(address_buffer));
	printf("Listen port %d from %s:%d\n", *(char *)recv_buffer.data, address_buffer, recv_address.port);
	ServerInfo information;
	if (enet_address_get_host(&server->host->address, information.hostname, sizeof(information.hostname)) != 0) {
		fprintf(stderr, "Failed to get hostname.\n"); return;
	}
	information.port = server->host->address.port;
	recv_buffer.dataLength = sizeof(information);
	recv_buffer.data = &information;
	if (enet_socket_send(server->listen, &recv_address, &recv_buffer, 1) != (int)recv_buffer.dataLength)
		fprintf(stderr, "Failed to reply to scanned.\n");
}

void send_string(ENetHost *host, char *string) {
	ENetPacket *packet = enet_packet_create(string, strlen(string) + 1, ENET_PACKET_FLAG_RELIABLE);
	enet_host_broadcast(host, 0, packet);
	return;
}

void stop_server(ENetLANServer *server) {
	printf("Server closing.\n");
	if (enet_socket_shutdown(server->listen, ENET_SOCKET_SHUTDOWN_READ_WRITE) != 0)
		fprintf(stderr, "Failed to shutdown listen socket.\n");
	enet_socket_destroy(server->listen);
	enet_host_destroy(server->host);
	enet_deinitialize();
	return;
}

int main(void)
{
	signal(SIGINT, sigint_handle);
	
	ENetLANServer server;
	if (!init_server(&server)) {
		return -1;
	}
	int check;
	while (!stop && check >= 0) {
		listen_for_clients(&server); ENetEvent event;
		check = enet_host_service(server.host, &event, 0);
		if (check > 0) {
			char buffer[256];
			switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT:
					sprintf(buffer, "New client connected %d.\n", event.peer->incomingPeerID);
					send_string(server.host, buffer);
					printf("%s\n", buffer);
					break;
				case ENET_EVENT_TYPE_RECEIVE:
					sprintf(buffer, "Client %d says '%s'", event.peer->incomingPeerID, event.packet->data);
					send_string(server.host, buffer);
					send_string(event.peer->host, "Server received your packet");
					printf("%s\n", buffer);
					break;
				case ENET_EVENT_TYPE_DISCONNECT:
					sprintf(buffer, "Client %d disconnected", event.peer->incomingPeerID);
					send_string(server.host, buffer);
					printf("%s\n", buffer);
					break;
				default:
					break;
			}
		}
		else if (check < 0) {
			fprintf(stderr, "Error servicing host.\n");
		}
		sleep(1);
	}
	stop_server(&server);
	return 0;
}

