/*
 * This file is part of hhod.
 *
 * hhod is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * hhod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * For a copy of the GNU General Public License
 * see <http://www.gnu.org/licenses/>.
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include "global.h"
#include "decode.h"


/**** system log ****/
#include <syslog.h>

/* Home Heartbeat On-line Daemon */
#define DAEMON_NAME "hhod"

/**** socket ****/

#include <sys/socket.h>
#include <netinet/in.h>

//#include "global.h"

#define SERVER_PORT     (1098)
#define MAXCLISOCKETS   (32)
#define MAXSOCKETS      (2+MAXCLISOCKETS)

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyUSB0"


static struct pollfd Clients[(3 * MAXSOCKETS)];
/* Client sockets */
static struct pollfd Clientsocks[MAXCLISOCKETS];

static size_t NClients;     /* # of valid entries in Clientsocks */
int PollTimeOut;


/*
* Like printf but print to socket without date/time stamp.
* Used to send back result of getstatus command.
*/
int statusprintf(int fd, const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int buflen;

	va_start(args, fmt);
	buflen = vsnprintf(buf, sizeof(buf)-2, fmt, args);
	va_end(args);
	return send(fd, buf, buflen, MSG_NOSIGNAL);
}


/*
* Like printf but prefix each line with date/time stamp.
* If fd == -1, send to all socket clients else send only to fd.
*/
int sockprintf(int fd, const char *fmt, ...)
{
	va_list args;
	char buf[1024];
	int buflen;
	int i;
	va_start(args, fmt);
	buflen = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (buflen > 1) {
		if (fd != -1) {

			return send(fd, buf, buflen, MSG_NOSIGNAL);
		}

		/* Send to all socket clients */
		for (i = 0; i < MAXCLISOCKETS; i++) {
			if ((fd = Clientsocks[i].fd) > 0) {
				send(fd, buf, buflen, MSG_NOSIGNAL);
			}
		}
	}
	return buflen;
}

static int Do_exit = 0;

static void init_client(void)
{
	int i;

	for (i = 0; i < MAXCLISOCKETS; i++) {
		Clientsocks[i].fd = -1;
	}
	NClients = 0;
}

/* Add new socket client */
static int add_client(int fd)
{
	int i;
	for (i = 0; i < MAXCLISOCKETS; i++) {
		if (Clientsocks[i].fd == -1) {
			Clientsocks[i].fd = fd;
			Clientsocks[i].events = POLLIN;
			Clientsocks[i].revents = 0;
			NClients++;
			return 0;
		}
	}
	//max clients exceeded
	return -1;
}

/* Delete socket client */
int del_client(int fd)
{
	int i;
	for (i = 0; i < MAXCLISOCKETS; i++) {
		if (Clientsocks[i].fd == fd) {
			Clientsocks[i].fd = -1;
			NClients--;
			return 0;
		}
	}
	return -1;
}

/* Copy socket client records to array */
static int copy_clients(struct pollfd *Clients)
{
	int i;

	for (i = 0; i < MAXCLISOCKETS; i++) {
		if (Clientsocks[i].fd != -1) {
			*Clients++ = Clientsocks[i];
		}
	}

	return NClients;
}

static void sighandler(int signum)
{
	Do_exit = 1;
}

int raw_data = 0;
static int mydaemon(void)
{
	//home heartbeat file descriptor
	int hh_fd, res;
	fd_set readfs;    /* file descriptor set */
	struct timeval Timeout;

	struct termios oldtio, newtio;

	hh_fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY);
	if (hh_fd < 0) { perror(MODEMDEVICE); exit(-1); }

	tcgetattr(hh_fd, &oldtio); /* save current serial port settings */
	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */

	/* set new port settings for canonical input processing */
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = ICANON;
	newtio.c_cc[VTIME] = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN] = 1;     /* blocking read until 1 character arrives */
	tcflush(hh_fd, TCIFLUSH);
	tcsetattr(hh_fd, TCSANOW, &newtio);

	int nready, i;

	/**** sockets ****/
	socklen_t clilen;
	int clifd, listenfd;
	char buf[1024];
	int bytesIn;
	struct sockaddr_in cliaddr, servaddr;
	static const int optval = 1;

	struct sigaction sigact;
	int r = 1;

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);

	int nusbfds = 1;        /* Skip over listen fd at [0] */
	Clients[nusbfds].fd = hh_fd;
	Clients[nusbfds].events = POLLIN;
	Clients[nusbfds].revents = 0;

	/**** sockets ****/
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	listen(listenfd, 128);

	init_client();
	Clients[0].fd = listenfd;
	Clients[0].events = POLLIN;

	PollTimeOut = -1;

	while (!Do_exit) {
		int nsockclients;
		int npollfds;

		/* Start appending records for socket clients to Clients array after
		* listen
		*/
		nsockclients = copy_clients(&Clients[2]);
		/* 1 for listen socket, 1 for nusbfds for hhb, nsockclients for socket clients
		*/
		npollfds = 2 + nsockclients;
		nready = poll(Clients, npollfds, PollTimeOut);

		/**** Time out ****/
		if (nready != 0) {
			/**** Poll HHB ****/
			FD_SET(hh_fd, &readfs);  /* set testing for source 1 */
			Timeout.tv_usec = 0;  /* milliseconds */
			Timeout.tv_sec = 1;  /* seconds */
			res = select(hh_fd + 1, &readfs, NULL, NULL, &Timeout);
			if (res)
			{
				/* number of file descriptors with input = 0, timeout occurred. */
				if (FD_ISSET(hh_fd, &readfs))  {       /* input from source 1 available */
					if ((res = read(hh_fd, buf, 1024)) > 1)
					{
						buf[res] = 0;             /* set end of string, so we can printf */
						decode_state(buf);
						if (--nready <= 0) continue;
					}
				}
			}

			/**** listen sockets ****/
			if (Clients[0].revents & POLLIN) {
				/* new client connection */
				clilen = sizeof(cliaddr);
				clifd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
				r = add_client(clifd);
				if (--nready <= 0) continue;
			}

			for (i = 2; i < npollfds; i++) {
				if ((clifd = Clients[i].fd) >= 0) {
					if (Clients[i].revents & (POLLIN | POLLERR)) {
						if ((bytesIn = read(clifd, buf, sizeof(buf))) < 0) {

							close(clifd);
							del_client(clifd);
						}
						else if (bytesIn == 0) {
							close(clifd);
							del_client(clifd);
						}
						else {
							buf[bytesIn] = 0;

							if (strcmp(buf, "raw=on\n")==0)
							{
								raw_data = 1;
							}

							if (strcmp(buf, "raw=off\n")==0)
							{
								raw_data = 0;
							}

							if (strcmp(buf, "exit\n")==0||
								strcmp(buf, "quit\n")==0
								)
							{
								Do_exit = 1;
							}

							if (strcmp(buf, "S\n")==0||strcmp(buf, "S\r\n")==0||
								strcmp(buf, "s\n")==0||
								strcmp(buf, "?\n")==0||
								strcmp(buf, "a\n")==0||
								strcmp(buf, "i\n")==0||
								strcmp(buf, "p\n")==0||
								strcmp(buf, "B\n")==0||
								strcmp(buf, "M\n")==0||
								strcmp(buf, "A\n")==0||
								strcmp(buf, "x\n")==0||
								strcmp(buf, "\n")==0
								)
							{
								write(hh_fd, &buf, (size_t)bytesIn);
							}
						}
						if (--nready <= 0) break;
					}
				}
			}
		}
	}

	if (Do_exit == 1)
		r = 0;
	else
		r = 1;
	/* restore old port settings */
	tcsetattr(hh_fd, TCSANOW, &oldtio);
	return r >= 0 ? r : -r;
}

static void printcopy(void)
{
	printf("\n");
	printf("This program comes with NO WARRANTY.\n");
	printf("You may redistribute copies of this program\n");
	printf("under the terms of the GNU General Public License.\n");
	printf("For more information about these matters, see the file named COPYING.\n");
	fflush(NULL);
}

int main(int argc, char *argv[])
{
	int rc, i;
	int foreground = 0;

	/* Initialize logging */
	openlog(DAEMON_NAME, LOG_PID, LOG_LOCAL5);
	syslog(LOG_NOTICE, "starting");

	/* Process command line args */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0)
			foreground = 1;
		else if (strcmp(argv[i], "--raw") == 0)
		raw_data = 1;
		else if (strcmp(argv[i], "--version") == 0) {
			printf("%s\n", "HHB Server BETA 0.0.2");
			printcopy();
			exit(0);
		}
		else {
			printf("unknown option %s\n", argv[i]);
			exit(-1);
		}
	}

	/* Daemonize */
	if (!foreground) {
		rc = daemon(0, 0);
	}

	/* Do real work */
	rc = mydaemon();

	/* Finish up */
	syslog(LOG_NOTICE, "terminated");
	closelog();
	return rc;
}
