#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "server.h"
#define BUFF_SIZE 1024

int main(int argc, const char *argv[])
{
	connected_users=0;
	int user_number=0;
	user_number=atoi(argv[1]);
	//printf("%i\n",user_number);
	users_list=construct(user_number);
	int i = 0;
	char buff[BUFF_SIZE];
	//memset(buff,0,BUFF_SIZE);
	ssize_t msg_len = 0;
	ssize_t res_len = 0;
	int srv_fd = -1;
	int cli_fd = -1;
	int epoll_fd = -1;
	ssize_t readb=0;
	struct sockaddr_in srv_addr;
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len=0;
	struct epoll_event e, es[user_number];

	memset(&srv_addr, 0, sizeof(srv_addr));
	memset(&cli_addr, 0, sizeof(cli_addr));
	memset(&e, 0, sizeof(e));

	srv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (srv_fd < 0) {
		printf("Cannot create socket\n");
		return 1;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(5556);
	if (bind(srv_fd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
		printf("Cannot bind socket\n");
		close(srv_fd);
		return 1;
	}

	if (listen(srv_fd, 1) < 0) {
		printf("Cannot listen\n");
		close(srv_fd);
		return 1;
	}

	epoll_fd = epoll_create(11);
	if (epoll_fd < 0) {
		printf("Cannot create epoll\n");
		close(srv_fd);
		return 1;
	}

	e.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	e.data.fd = srv_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, srv_fd, &e) < 0) {
		printf("Cannot add server socket to epoll\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}
	////////////////////////////////////////////////////////////////////////////////
	for(;;) {
		i = epoll_wait(epoll_fd, es, 11, -1);

		if (i < 0) {
			printf("Cannot wait for events\n");
			close(epoll_fd);
			close(srv_fd);
			return 1;
		}

		for (--i; i > -1; --i) {
			printf("Event %x\n", es[i].events);
			if (es[i].data.fd == srv_fd) {
				cli_fd = accept(srv_fd, (struct sockaddr*) &cli_addr, &cli_addr_len);
				if (cli_fd < 0) {
					printf("Cannot accept client1\n");
					close(epoll_fd);
					close(srv_fd);
					return 1;
				}

				e.data.fd = cli_fd;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_fd, &e) < 0) {
					printf("Cannot accept client2\n");
					close(epoll_fd);
					close(srv_fd);
					return 1;
				}
			} else {
				cli_fd=es[i].data.fd;
				if (es[i].events & EPOLLERR || es[i].events & EPOLLHUP || es[i].events & EPOLLRDHUP) {
					//TODO bad event on client fd - clean client:
					//		* remove asociated user from the list
					//		* remove socket from epoll
					//		* close socket
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cli_fd, &e);
					close(cli_fd);
					

				}
				else if ( es[i].events & EPOLLIN ){
					// Odczytywanie msg od klienta
					read(cli_fd, &msg_len, sizeof(msg_len));
					readb=read(cli_fd, buff, msg_len);
					printf("Odczytane dane : %zu\n",msg_len);

					if (readb > 0) {
						respond(cli_fd,buff);
						res_len=strlen(buff);
						write(cli_fd, &res_len, sizeof(res_len));
						write(cli_fd, buff, res_len);
						memset(buff,0,BUFF_SIZE);
						printf("Wyslano wiadomosc\n");

					}
					
				}
				else {
                                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cli_fd, &e);
					close(cli_fd);
				     }
			}
		}
	}

	clear(users_list);
	return 0;
}
