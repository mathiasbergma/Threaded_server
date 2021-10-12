/*
 * Socket.c
 *
 *  Created on: 05-10-2021
 *  	Author: Mathias Berglund Mathiesen
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define QUEUE_SIZE 12
#define ACTIVE_QUEUE 2
#define PORT "8080"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

pthread_mutex_t ctr_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ctr_cond = PTHREAD_COND_INITIALIZER;
volatile int active_ctr = 0;

pthread_mutex_t sock_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int sockets = 0;

void* handle_conn(void *p_socket_client);

int main()
{
	printf("Configuring local address...\n");
	struct addrinfo hints; //A struct that can contain all necessary information
	memset(&hints, 0, sizeof(hints)); // Fill hints with 0's

	hints.ai_family = AF_UNSPEC; 					//IPv4
	hints.ai_socktype = SOCK_STREAM; 				//TCP
	hints.ai_flags = AI_PASSIVE; 					//Localhost

	struct addrinfo *bind_address;

	getaddrinfo(0, PORT, &hints, &bind_address);

	printf("Creating socket...\n");
	SOCKET socket_listen;							// Just an int

	/* Call socket() to create a communications endpoint. Return value is
	 * a descriptor that refers to the endpoint
	 */
	socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
			bind_address->ai_protocol);
	if (!ISVALIDSOCKET(socket_listen))
	{
		fprintf(stderr, "socket() failed. (%d)\n", errno);
		return 1;
	}

	printf("Binding socket to local address...\n");
	if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
	{
		fprintf(stderr, "bind() failed. (%d)\nPort is most "
				"likely in use\n", errno);
		return 1;
	}

	//House keeping
	freeaddrinfo(bind_address);

	printf("Listening...\n");
	if (listen(socket_listen, 10) < 0)
	{
		fprintf(stderr, "listen() failed. (%d)\n", errno);
		return 1;
	}

	while (1)
	{
		printf("Waiting for connection...\n");
		struct sockaddr_storage client_address;
		socklen_t client_len = sizeof(client_address);

		// Binding function. Execution will halt until accept returns
		SOCKET socket_client = accept(socket_listen,
				(struct sockaddr*) &client_address, &client_len);
		if (!ISVALIDSOCKET(socket_client))
		{
			fprintf(stderr, "accept() failed. (%d)\n", errno);
			return 1;
		}

		// Create a new ptread variable
		pthread_t t;
		// Instantiate a pointer and allocate some memory for it
		int *p_sock_cli = malloc(sizeof(int));
		// Save socket client to the new variable
		*p_sock_cli = socket_client;

		// Only create a new thread if current count is less that QUEUE_SIZE
		if (sockets < QUEUE_SIZE)
		{
			printf("Client is connected... ");
			// Create a new tread
			pthread_create(&t, NULL, handle_conn, p_sock_cli);

			// Making sure that only 10 threads exists. Lock the mutex
			pthread_mutex_lock(&sock_mutex);
			// Update socket counter
			sockets++;
			pthread_mutex_unlock(&sock_mutex);

			printf("%d active connections\n", sockets);
		}
		else // QUEUE is full. Close connection immediately
		{
			CLOSESOCKET(socket_client);
			free(p_sock_cli);
		}

	}

	printf("Closing listening socket...\n");
	CLOSESOCKET(socket_listen);

	printf("Finished.\n");

	return 0;
}

void* handle_conn(void *p_socket_client)
{
	// Save the socket client in a local variable
	int socket_client = *(SOCKET*) p_socket_client;
	// Free the pointer
	free(p_socket_client);
	char request[1024];

	// Check if we already have 2 sockets that are ECHOing
	if (active_ctr >= ACTIVE_QUEUE)
	{
		char init_reply[] = "Connection is active - You are queued for "
				"active service....\n";
		int bytes_sent = send(socket_client, init_reply, strlen(init_reply), 0);
		printf("Send %d bytes to client in queue\n", bytes_sent);

		// We wait for the active counter to be less than ACTIVE_QUEUE.
		// In while loop to handle spurious wakeups
		while (active_ctr >= ACTIVE_QUEUE)
		{
			pthread_cond_wait(&ctr_cond, &ctr_mutex);
		}
		char ready[] = "You have been dequeued. Ready "
				"to ECHO.... CTRL^D to close connection\n";
		send(socket_client, ready, strlen(ready), 0);
	}
	else // Less than 2 sockets that are ECHOing
	{
		// Send a welcome message to the new connection
		char welcome[] = "Ready to ECHO..... CTRL^D to close connection\n";
		send(socket_client, welcome, strlen(welcome), 0);
	}

	// Update the variable
	active_ctr++;
	// Unlock the variable
	pthread_mutex_unlock(&ctr_mutex);

	printf("Reading request...\n");

	// First do a recv() so the while() loop can check for CTRL^D
	int bytes_received = recv(socket_client, request, 1024, 0);
	// Zero terminate the response so strlen() works
	request[bytes_received] = 0;

	while ((request[0] != 0x04) && (bytes_received != 0))
	{

		printf("Received %d bytes.\n", bytes_received);
		printf("%.*s", bytes_received, request);

		printf("Echoing transmission...\n");

		int bytes_sent = send(socket_client, request, strlen(request), 0);
		printf("Sent %d of %d bytes.\n", bytes_sent, (int) strlen(request));

		printf("Reading request...\n");
		bytes_received = recv(socket_client, request, 1024, 0);
		// Zero terminate the response so strlen() works
		request[bytes_received] = 0;

	}
	// User has closed connection. Say farewell
	char goodbye[] = "Closing connection\n";
	send(socket_client, goodbye, strlen(goodbye), 0);

	printf("Closing connection... ");
	CLOSESOCKET(socket_client);

	// Updating countervalue
	pthread_mutex_lock(&ctr_mutex);
	active_ctr--;
	pthread_cond_signal(&ctr_cond);
	pthread_mutex_unlock(&ctr_mutex);


	 // A socket has been closed. "sockets" must reflect this
	pthread_mutex_lock(&sock_mutex);
	sockets--;
	pthread_mutex_unlock(&sock_mutex);

	printf("%d active connections\n", sockets);

	return NULL;
}
