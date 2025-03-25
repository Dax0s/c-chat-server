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

int starts_with(const char* str, const char* prefix)
{
    return strncmp(str, prefix, strlen(prefix));
}

void signal_handler(int);

volatile int keep_running = 1;

void parse_server(const char* str, char* server_name);
void parse_user(const char* str, char* user_name);
void parse_msg(const char* str, char* msg);

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

    struct sockaddr_in email_server_addr;
    const int email_server_port = atoi(argv[3]);

    const int email_server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (email_server_socket_fd < 0)
        error("ERROR opening socket");

    struct hostent *server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    email_server_addr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *)&email_server_addr.sin_addr.s_addr, server->h_length);
    email_server_addr.sin_port = htons(email_server_port);

    if (connect(email_server_socket_fd, (struct sockaddr *) &email_server_addr, sizeof(email_server_addr)) < 0)
        error("ERROR connecting to email server");

    write(email_server_socket_fd, argv[1], strlen(argv[1]));

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

    int sock = -1;
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

    if (p == NULL || sock == -1)
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
                    connected_clients--;
                    for (int j = i; j < connected_clients; j++)
                    {
                        client_pollfd[j] = client_pollfd[j + 1];
                        clients[j] = clients[j + 1];
                    }

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
                    if (starts_with(buffer, "@get") == 0)
                    {
                        char get_buffer[1024] = "";
                        sprintf(get_buffer, "@get <%s>", clients[i].username);
                        write(email_server_socket_fd, get_buffer, strlen(get_buffer));

                        char email_buffer[1024] = "";
                        while (1)
                        {
                            read(email_server_socket_fd, email_buffer, 1023);
                            if (starts_with(email_buffer, "@end") == 0)
                            {
                                break;
                            }

                            char tmp_buffer[1024] = "";
                            sprintf(tmp_buffer, "PRANESIMAS %s", email_buffer);
                            write(clients[i].sock, tmp_buffer, strlen(tmp_buffer));
                            bzero(tmp_buffer, sizeof(tmp_buffer));
                            bzero(email_buffer, sizeof(email_buffer));
                            write(email_server_socket_fd, "@received\n", 10);
                        }

                        continue;
                    }

                    if (starts_with(buffer, "@send") == 0)
                    {
                        char server_name[1024] = "";
                        char user_name[1024] = "";
                        char msg[1024] = "";

                        parse_user(buffer, user_name);
                        parse_server(buffer, server_name);
                        parse_msg(buffer, msg);

                        char send_buffer[1024] = "";

                        sprintf(send_buffer, "@send <%s> <%s> <%s>: %s", clients[i].username, server_name, user_name, msg);

                        write(email_server_socket_fd, send_buffer, strlen(send_buffer));

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

void parse_server(const char* str, char* server_name)
{
    bzero(server_name, sizeof(server_name));

    int write = 0;
    int len = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '<')
        {
            write++;
            continue;
        }
        if (str[i] == '>')
        {
            break;
        }
        if (write == 1)
        {
            server_name[len++] = str[i];
        }
    }
}

void parse_user(const char* str, char* user_name)
{
    bzero(user_name, sizeof(user_name));

    int write = 0;
    int len = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '<')
        {
            write++;
            continue;
        }
        if (str[i] == '>' && write == 2)
        {
            break;
        }
        if (write == 2)
        {
            user_name[len++] = str[i];
        }
    }
}

void parse_msg(const char* str, char* msg)
{
    bzero(msg, sizeof(msg));

    int write = 0;
    int len = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '>')
        {
            write++;
            continue;
        }
        if (write == 3)
        {
            msg[len++] = str[i];
        }
        if (write == 2)
            write++;
    }
}

void signal_handler(int)
{
    keep_running = 0;
    printf("\nServer closing...\n");

    exit(0);
}
