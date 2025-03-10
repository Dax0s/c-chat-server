#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <signal.h>

typedef struct client
{
    int sock;
    char* username;
} client;

void error(const char *msg)
{
    fprintf(stderr, "[ERROR]: %s", msg);
    exit(1);
}

void signal_handler(int sig);

volatile int keep_running = 1;

int main(const int argc, const char **argv)
{
    struct sigaction act;
    act.sa_handler = signal_handler;
    sigaction(SIGINT, &act, NULL);

    if (argc != 2)
    {
        error("Usage: ./server <port>");
    }

    char buffer[1024];

    struct sockaddr_in server_addr;

    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error("error creating a socket");

    const int port = (int) strtol(argv[1], NULL, 10);
    // Address Family - internet; supports IPv4 addresses
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    socklen_t server_addr_size = sizeof(server_addr);
    if (bind(sock, (const struct sockaddr *) &server_addr, server_addr_size) < 0)
        error("error on binding");

    listen(sock, 5);
    struct pollfd sock_pollfd;
    sock_pollfd.fd = sock;
    sock_pollfd.events = POLLIN;

    int connected_clients = 0;
    client* clients = NULL;
    struct pollfd* client_pollfd = NULL;

    while (keep_running)
    {
        bzero(buffer, sizeof(buffer));

        if (poll(&sock_pollfd, 1, 0))
        {
            const int client_sock = accept(sock, (struct sockaddr *) &server_addr, &server_addr_size);
            if (client_sock < 0)
                error("error on accept");

            connected_clients++;
            client* tmp = realloc(clients, connected_clients * sizeof(client));
            if (tmp == NULL)
            {
                error("failed to allocate memory");
            }
            clients = tmp;
            client curr_client;
            curr_client.sock = client_sock;
            curr_client.username = NULL;
            clients[connected_clients - 1] = curr_client;

            struct pollfd* tmp_client_pollfd = realloc(client_pollfd, sizeof(struct pollfd) * connected_clients);
            if (tmp_client_pollfd == NULL)
            {
                error("failed to allocate memory");
            }
            client_pollfd = tmp_client_pollfd;

            struct pollfd tmp_poll;
            tmp_poll.fd = client_sock;
            tmp_poll.events = POLLIN;

            client_pollfd[connected_clients - 1] = tmp_poll;

            write(curr_client.sock, "Enter your username: ", 21);
            printf("Client connected\n");
        }

        if (poll(client_pollfd, connected_clients, 0))
        {
            for (int i = 0; i < connected_clients; i++)
            {
                if (client_pollfd[i].revents & POLLHUP)
                {
                    printf("Client disconnected\n");

                    close(client_pollfd[i].fd);
                    for (int j = i; j < connected_clients - 1; j++)
                    {
                        client_pollfd[j] = client_pollfd[j + 1];
                        clients[j] = clients[j + 1];

                        connected_clients--;

                        struct pollfd* tmp = realloc(client_pollfd, sizeof(struct pollfd) * connected_clients);
                        if (tmp == NULL)
                        {
                            error("failed to allocate memory");
                        }
                        client_pollfd = tmp;

                        client* tmp1 = realloc(clients, sizeof(client) * connected_clients);
                        if (tmp1 == NULL)
                        {
                            error("failed to allocate memory");
                        }
                        clients = tmp1;
                    }
                }
                else if (client_pollfd[i].revents & POLLIN)
                {
                    read(clients[i].sock, buffer, sizeof(buffer) - 1);
                    printf("Received: %s", buffer);

                    if (clients[i].username == NULL)
                    {
                        buffer[strlen(buffer) - 2] = '\0';
                        printf(buffer);
                        clients[i].username = malloc(strlen(buffer) + 1);
                        strcpy(clients[i].username, buffer);
                        continue;
                    }
                    for (int j = 0; j < connected_clients; j++)
                    {
                        if (i == j)
                            continue;

                        char* tmp = malloc(2 + strlen(clients[j].username) + 1);

                        sprintf(tmp, "[%s]: ", clients[i].username);
                        write(clients[j].sock, tmp, strlen(tmp));
                        write(clients[j].sock, buffer, sizeof(buffer));
                    }
                }
            }
        }
    }

    for (int i = 0; i < connected_clients; i++)
    {
        close(clients[i].sock);
    }

    close(sock);
    free(clients);
    free(client_pollfd);

    return 0;
}

void signal_handler(const int sig)
{
    printf("\nServer closing...\n");

    exit(0);
}
