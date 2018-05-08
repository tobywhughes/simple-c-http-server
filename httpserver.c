#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define MAXLINE	516


void *client_request(void *arg);
char *reply_to_request();
int parse_get(char * buf);
int parse_host(char * buf);
int parse_user_agent(char * buf);

const int backlog = 4;
char filename[128];
char version_name[128];
char hostname[128];
char user_agent[128];

bool started = false;

int main(int argc, char *argv[])
{

    int	    listenfd, connfd;
    pthread_t pid;
    int     clilen;
    struct  sockaddr_in cliaddr, servaddr;

    if (argc != 3) {
	printf("Usage: tcpserver <address> <port> \n");
	return EXIT_FAILURE;
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
	perror("socket error");
	return EXIT_FAILURE;
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family        = AF_INET;
    servaddr.sin_addr.s_addr   = inet_addr(argv[1]);
    servaddr.sin_port          = htons(atoi(argv[2]));

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
	perror("bind error");
        return EXIT_FAILURE;

    }
	
    if (listen(listenfd, backlog) == -1) {
	perror("listen error");
	return EXIT_FAILURE;
    }

    while (1) {
	clilen = sizeof(cliaddr);
	if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen)) < 0 ) {
		if (errno == EINTR)
			continue;
		else {
			perror("aceppt error");
			return EXIT_FAILURE;
		}
	}
	pthread_create(&pid, NULL, client_request, (void *)&connfd);

	//close(connfd);
    }

}

void * client_request(void *arg)
{
	int connfd;
	connfd = *(int *)arg;
	int	n;
	int error;
	time_t  tnow;
        char    *ptime;
	char	buf[512];
	char * token;
	char * reply;
	char * lines[32];
	uint8_t line_count = 0;
	
	while (1) {
		line_count = 0;
		if ((n = read(connfd, buf, MAXLINE)) == 0)
			return;
		printf("FULL MESSAGE: %s\n\n", buf);
		if (buf[0] == '\r' && buf[1] == '\n')
		{
			token = NULL;
			error = parse_line(token);
		}
		else
		{
			token = strtok(buf, "\r\n");
			lines[line_count] = token;
			line_count++;
			while (token != NULL)
			{
				token = strtok(NULL, "\r\n");
				lines[line_count] = token;
				line_count++;
				printf("#Token: %s\n", token);
			}
			for(int i = 0; i < line_count; i++)
			{
				error = parse_line(lines[i]);
				if(error != 200 && error != -1)
				{
				 	parse_line(NULL);
					break;
				}
			}
		}
		printf("ERROR: %d\n", error);
		if(error == -1)
		{
			reply = reply_to_request();
			write(connfd, reply, strlen(reply));
		}
		if(started == true)
		{
			write(connfd, "Line Recieved\n", 14);  
		}
	}
}

int parse_line(char * buf)
{
	int error = 0;
	printf("PARSING LINE: %s\n", buf);
	char passbuf[256];
	if(buf == NULL)
	{
		error = 1;
		if(started == true)
		{
			error = -1;
		}
		started = false;
		printf("REQUEST ENDED\n");
		return error;
	}
	strcpy(passbuf, buf);
	char * token;
	token = strtok(buf, " ");
	if(started == false){
		if (strcmp(token, "GET") == 0)
		{
			error = parse_get(passbuf);
			if (error == 200)
			{
				started = true;
			}
			return error;
		}
	}
	else
	{
		if (strcmp(token, "Host:") == 0)
		{
			error = parse_host(passbuf);
			return error;	
		}
		else if (strcmp(token, "User-Agent:") == 0)
		{
			error = parse_user_agent(passbuf);
			return error;
		}
	}
	return error;
}

int parse_get(char * buf)
{
	int error = 200;
	printf("FULL COMMAND: %s\n", buf);
	printf("COMMAND: %s\n", strtok(buf, " "));
	strcpy(filename, strtok(NULL, " "));
	printf("FILENAME: %s\n", filename);
	strcpy(version_name, strtok(NULL, "\\"));
	printf("VERSION NAME: %s\n", version_name);
	if(strcmp(version_name, "HTTP/1.0") != 0)
	{
		printf("Invalid version name. Please use HTTP/1.0.\n");
		error = 505;
	}
	return error;
}

char * reply_to_request()
{
	return "<b>TEMP REPLY</b>";
}

int parse_host(char * buf)
{
	printf("$PARSING HOST~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(hostname, strtok(NULL, ""));
	printf("HOSTNAME: %s\n", hostname);
	return 200;
}

int parse_user_agent(char * buf)
{
	printf("$PARSING USER AGENT~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(user_agent, strtok(NULL, ""));
	printf("USER AGENT: %s\n", user_agent);
	return 200;
}
