/*
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#define BACKLOG 10

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

void *handle_conn(void* p_socket_client);

int main()
{

	printf("Configuring local address...\n");
	struct addrinfo hints; /* A struct that can contain all necessary information*/
	memset(&hints, 0, sizeof(hints)); /* Fill hints with 0's */

	hints.ai_family = AF_UNSPEC; 					//IPv4
	hints.ai_socktype = SOCK_STREAM; 				//TCP
	hints.ai_flags = AI_PASSIVE; 					//Localhost

	struct addrinfo *bind_address;

	getaddrinfo(0, "8080", &hints, &bind_address);

	printf("Creating socket...\n");
	SOCKET socket_listen;							// Just an int

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
		fprintf(stderr, "bind() failed. (%d)\n", errno);
		return 1;
	}

	//House keeping
	freeaddrinfo(bind_address);

	/*
	 * INFO
	 * Vi har forbundet til porten og ip, nu skal vi til at vente på om der er nogen som ringer
	 * At lytte er step 1
	 */

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

		SOCKET socket_client = accept(socket_listen,
				(struct sockaddr*) &client_address, &client_len);
		if (!ISVALIDSOCKET(socket_client))
		{
			fprintf(stderr, "accept() failed. (%d)\n", errno);
			return 1;
		}

		printf("Client is connected... ");

		//handle_conn(&socket_client);

		pthread_t t;
		int *p_sock_cli = malloc(sizeof(int));
		*p_sock_cli = socket_client;
		pthread_create(&t, NULL, handle_conn, p_sock_cli);

	}


	printf("Closing listening socket...\n");
	CLOSESOCKET(socket_listen);

	printf("Finished.\n");

	return 0;
}

void *handle_conn(void* p_socket_client)
{
	int socket_client = *(SOCKET*)p_socket_client;
	free(p_socket_client);
	printf("Reading request...\n");
	char request[1024];

	int bytes_received = recv(socket_client, request, 1024, 0);

	while (request != 0)
	{

		printf("Received %d bytes.\n", bytes_received);
		printf("%.*s", bytes_received, request);

		printf("Echoing transmission...\n");

		int bytes_sent = send(socket_client, request, strlen(request), 0);
		printf("Sent %d of %d bytes.\n", bytes_sent, (int) strlen(request));

		printf("Reading request...\n");
		bytes_received = recv(socket_client, request, 1024, 0);
		request[bytes_received] = 0;

	}

	printf("Closing connection...\n");
	CLOSESOCKET(socket_client);

	return NULL;
}
