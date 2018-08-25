//============================================================================
// Name        : AsyncIOExample.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : AsyncIOExample in C++, Ansi-style
//============================================================================

#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <unistd.h>

#include <iostream>
using namespace std;

int const MAX_EVENTS = 1'000;
int const MAX_CONN = 1'000;

void panic(string err) {
	perror(err.c_str());
	exit(EXIT_FAILURE);
}

void set_non_blocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < -1)
        panic("fcntl failed");

    if ((fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)) < 0)
        panic("fcntl failed");
}

int main() {
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		panic("socket failed");

	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
			sizeof(opt)))
		panic("setsockopt failed");

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("localhost");
	addr.sin_port = htons(8080);

	if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		panic("bind failed");

	if (listen(sockfd, MAX_CONN) < 0)
		panic("listen failed");

	cout << "Listening on http://localhost:8080" << endl;

	int epollfd = epoll_create1(0);

	if (epollfd < 0)
		panic("epol_create1 failed");

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) < 0)
		panic("epoll_ctl failed");

	struct epoll_event events[MAX_EVENTS];
		int nfds;
		int new_sockfd;
		int addrlen = sizeof(addr);

		for (;;) {
			nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
			if (nfds == -1)
				panic("epoll_wait failed");

			for (int n = 0; n < nfds; ++n) {
				if (events[n].data.fd == sockfd) {
					new_sockfd = accept(sockfd, (struct sockaddr *) &addr,
							(socklen_t *) &addrlen);

					if (new_sockfd == -1)
						panic("accept failed");

					set_non_blocking(new_sockfd);

					ev.events = EPOLLIN;
					ev.data.fd = new_sockfd;
					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_sockfd, &ev) == -1)
						panic("epoll_ctl failed");

				} else {
					if (events[n].events & EPOLLIN) {
						handle_request(events[n].data.fd);
					} else if (events[n].events & EPOLLOUT) {
						handle_response(events[n].data.fd);
					} else if (events[n].events & (EPOLLHUP | EPOLLERR)) {
						tmp_requests.erase(events[n].data.fd);
						tmp_responses.erase(events[n].data.fd);
						close(events[n].data.fd);
					}
				}
			}
		}

	return 0;
}
