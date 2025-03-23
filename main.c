#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>

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

void signal_handler(int);

volatile int keep_running = 1;

int main(const int argc, const char **argv)
{
    struct sigaction act;
    act.sa_handler = signal_handler;
    sigaction(SIGINT, &act, NULL);

    if (argc != 4)
    {
        char* tmp = malloc(48 + strlen(argv[0]) + 1);
        sprintf(tmp, "Usage: %s <server-name> <port> <email-server-port>", argv[0]);

        error(tmp);

        free(tmp);
    }

    char buffer[1024];

    struct addrinfo hints, *server_addr, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, argv[2], &hints, &server_addr) != 0)
    {
        error("getaddrinfo error");
    }

    int sock;
    for (p = server_addr; p != NULL; p = p->ai_next)
    {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sock);
            continue;
        }

        break;
    }

    if (p == NULL)
        error("failed to bind");

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
            const int client_sock = accept(sock, p->ai_addr, &p->ai_addrlen);
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

            write(curr_client.sock, "ATSIUSKVARDA\n", 13);
            printf("Client connected. Socket fd: %d\n", curr_client.sock);
        }

        if (poll(client_pollfd, connected_clients, 0))
        {
            for (int i = 0; i < connected_clients; i++)
            {
                if (client_pollfd[i].revents & POLLHUP)
                {
                    printf("Client with socket fd %d and username %s disconnected\n", clients[i].sock, clients[i].username);

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
                    bzero(buffer, sizeof(buffer));
                    read(clients[i].sock, buffer, sizeof(buffer) - 1);

                    if (clients[i].username == NULL)
                    {
                        if (buffer[strlen(buffer) - 2] == '\r')
                        {
                            buffer[strlen(buffer) - 2] = '\0';
                        }
                        else
                        {
                            buffer[strlen(buffer) - 1] = '\0';
                        }

                        if (strlen(buffer) < 3)
                        {
                            write(clients[i].sock, "BLOGAS VARDAS\n", 14);
                            printf("Received bad username from client with sock id %d: %s\n", clients[i].sock, buffer);
                        }
                        else
                        {
                            clients[i].username = malloc(strlen(buffer) + 1);
                            strcpy(clients[i].username, buffer);
                            printf("Received username from client with sock id %d: %s\n", clients[i].sock, buffer);
                            write(clients[i].sock, "VARDASOK\n", 9);
                        }

                        continue;
                    }
                    for (int j = 0; j < connected_clients; j++)
                    {
                        if (i == j)
                        {
                            if (buffer[strlen(buffer) - 1] == '\n')
                                printf("Received message from %s: %s", clients[i].username, buffer);
                            else
                                printf("Received message from %s: %s\n", clients[i].username, buffer);

                            continue;
                        }

                        char* tmp = malloc(15 + strlen(clients[i].username) + strlen(buffer) + 1);

                        sprintf(tmp, "PRANESIMAS [%s]: %s", clients[i].username, buffer);
                        write(clients[j].sock, tmp, strlen(tmp));
                        if (buffer[strlen(buffer) - 1] != '\n')
                            write(clients[j].sock, "\n", 1);

                        free(tmp);
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

void signal_handler(int)
{
    keep_running = 0;
    printf("\nServer closing...\n");

    exit(0);
}
