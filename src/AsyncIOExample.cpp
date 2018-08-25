//============================================================================
// Name        : AsyncIOExample.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : AsyncIOExample in C++, Ansi-style
//============================================================================

#include <unistd.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace std;

int const MAX_EVENTS = 1'000;
int const MAX_CONN = 1'000;

int epollfd;
epoll_event ev;

void panic(string err)
{
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

void handle_request(int sockfd)
{
	string request;
	char buffer[1024] = {0};
	int status;

	string route = "GET / HTTP/1.1";

	try
	{
		/********************************************************************************
		You may want to use `while` loop here
		to read from the socket until the end or EWOULDBLOCK,
		but for the sake of the example, I just read once and compare with the route.
		********************************************************************************/
		status = read(sockfd, buffer, 1024);
		if (status > 0)
		{
			request += buffer;
			cout << request << endl;

			string response =
				request.compare(0, route.length(), route) == 0
					? "HTTP/1.1 200 OK\r\n"
					  "Content-Type: text/html\r\n"
					  "\r\n"
					  "<html lang='en'>"
					  "<head>"
					  "	<title>AsyncIOExample</title>"
					  "</head>"
					  "<body>"
					  "	<h1>AsyncIOExample</h1>"
					  "	<p>Hello, World!</p>"
					  "</body>"
					  "</html>"

					: "HTTP/1.1 404 NOT FOUND\r\n"
					  "Content-Type: text/html\r\n"
					  "\r\n"
					  "<html lang='en'>"
					  "<head>"
					  "	<title>AsyncIOExample</title>"
					  "</head>"
					  "<body>"
					  "	<p>404 NOT FOUND</p>"
					  "</body>"
					  "</html>";

			int status = send(sockfd, response.c_str(), response.length(), 0);

			if (status > 0)
			{
				cout << response << endl;
				close(sockfd);
			}
			else if (errno & (EWOULDBLOCK | EAGAIN))
			{
				/********************************************
				If it returns EWOULDBLOCK for some reasons,
				we need to ask the kernel to notify us when 
				we can write to the socket (EPOLLOUT) 
				and handle it properly. but for this example, 
				I just close the socket.
				*********************************************/

				// ev.events = EPOLLOUT;
				// ev.data.fd = sockfd;

				// if (epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1)
				// 	panic("epoll_ctl failed");

				close(sockfd);
			}
			else
			{
				perror("send failed");
				close(sockfd);
			}
		}
		else
		{

			if (errno & (EWOULDBLOCK | EAGAIN))
			{
				return;
			}
			else if (status < 0)
			{
				perror("read failed");
			}

			close(sockfd);
		}
	}
	catch (const std::exception &e)
	{
		cerr << e.what() << endl;
		close(sockfd);
	}
}

int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		panic("socket failed");

	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
				   sizeof(opt)))
		panic("setsockopt failed");

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(8080);

	if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		panic("bind failed");

	if (listen(sockfd, MAX_CONN) < 0)
		panic("listen failed");

	cout << "Listening on http://localhost:8080" << endl;

	epollfd = epoll_create1(0);

	if (epollfd < 0)
		panic("epol_create1 failed");

	ev.events = EPOLLIN;
	ev.data.fd = sockfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) < 0)
		panic("epoll_ctl failed");

	struct epoll_event events[MAX_EVENTS];
	int nfds;
	int new_sockfd;
	int addrlen = sizeof(addr);

	for (;;)
	{
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1)
			panic("epoll_wait failed");

		for (int n = 0; n < nfds; ++n)
		{
			if (events[n].data.fd == sockfd)
			{
				new_sockfd = accept(sockfd, (struct sockaddr *)&addr,
									(socklen_t *)&addrlen);

				if (new_sockfd == -1)
					panic("accept failed");

				set_non_blocking(new_sockfd);

				ev.events = EPOLLIN;
				ev.data.fd = new_sockfd;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_sockfd, &ev) == -1)
					panic("epoll_ctl failed");
			}
			else
			{
				if (events[n].events & EPOLLIN)
				{
					/***********************************************
					We should keep the work in the loop minimal.
					The request should be handled in other threads.
					***********************************************/
					handle_request(events[n].data.fd);
				}
				else if (events[n].events & (EPOLLHUP | EPOLLERR))
				{
					close(events[n].data.fd);
				}
			}
		}
	}

	return 0;
}
