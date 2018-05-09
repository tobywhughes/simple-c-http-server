#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define MAXLINE	516


void *client_request(void *arg);
bool buffer_is_valid(char * buf);
bool double_return(char * buf);
void reply_to_request(int error);
int parse_line(char * buf);
int parse_post(char * buf);
int parse_get(char * buf);
int parse_head(char * buf);
int parse_host(char * buf);
int parse_user_agent(char * buf);
int parse_accept(char * buf);
int parse_content_type(char * buf);
int parse_languages(char * buf);
int parse_connection_param(char * buf);
int parse_referer(char * buf);
int parse_content_length(char * buf);

const int backlog = 4;
char filename[128];
char version_name[128];
char hostname[128];
char user_agent[128];
char referer[128];
char reply[256];
char * types[20];
char * content_types[20];
char * languages[10];
char connection[64] = "keep-alive";
bool started = false;
char request_type[16];
char content_length[16] = "0";
bool post_body_flag = false;
char post_body[MAXLINE];

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
	char * lines[32];
	uint8_t line_count = 0;
	bool buffer_flag = false;
	char line_buffer[512] = {'\0'};
	char test_buffer[512] = {'\0'};
	char holder[512];
	while (1) {
		strcpy(test_buffer, line_buffer);
		memset(buf, '\0', MAXLINE);
		line_count = 0;
		if ((n = read(connfd, buf, MAXLINE)) == 0)
			return;
		strcpy(line_buffer, test_buffer);
		strcat(line_buffer, buf);
		if(buffer_is_valid(line_buffer) == true)
		{
			strcpy(buf, line_buffer);
			printf("FULL MESSAGE: %s\n\n", buf);
			if (buf[0] == '\r' && buf[1] == '\n')
			{
				token = NULL;
				error = parse_line(token);
			}
			else
			{
				int post_increment = 0;
				char temp_[512];
				strcpy(temp_, buf);
				if(temp_[0] == 'P' && temp_[1] == 'O' && temp_[2] == 'S' && temp_[3] == 'T')
				{
					printf("~~~POST~~~");
					post_increment++;
				}
				token = strtok(buf, "\r\n");
				lines[line_count] = token;
				line_count++;
				char * temp2;
				while (token != NULL || post_increment >= 0)
				{
					temp2 = strtok(NULL, "");
					if (temp2 == NULL)
					{
						lines[line_count] = NULL;
						token = NULL;
						post_increment--;
					}
					else if(double_return(temp2) == true)
					{
						//printf("#########TEST");
						lines[line_count] = NULL;
						token = NULL;
						post_increment--;
						strtok(temp2, "\n");
					}
					else
				       	{
						token = strtok(temp2, "\r\n");
						lines[line_count] = token;
					}
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
			reply_to_request(error);
			started = false;
			write(connfd, reply, strlen(reply));
			memset(line_buffer, '\0', MAXLINE);
		}	
		else
		{
			printf("##INCOMPLETE REQUEST. BUFFERING\n");
			fflush(stdout);
		}
	}
}

bool buffer_is_valid(char * buf)
{
	char buf_use[MAXLINE];
	strcpy(buf_use, buf);
	if(strstr(buf_use, "\r\n\r\n") != NULL)
	{
		return true;
	}
	return false;
}

bool double_return(char * buf)
{
	char buf_test[MAXLINE];
	strcpy(buf_test, buf);
//	printf("@@%s\n", buf_test);
//	printf("%d", buf_test[0]);
//	printf("%d", buf_test[1]);
	if(buf_test[0] == '\n' && buf_test[1] == '\r' && buf_test[2] == '\n')
	{
		return true;
	}
	return false;
}

void reply_to_request(int error)
{
	printf("\n#######\nRESPONSE\n#######\n");
	if(error == -1)
	{
		if(filename[0] == '/')
		{
			int i = 0;
			while(filename[i] != '\0')
			{
				filename[i] = filename[i+1];
				i++;
			}
		}
		if(access(filename, F_OK) == -1 && (strcmp(request_type, "GET") == 0 || strcmp(request_type, "HEAD") == 0))
		{
			error = 404;
		}
	}
	if(error == -1 && started == true && (strcmp(request_type, "GET") == 0 || strcmp(request_type, "HEAD") == 0))
	{
		char html[512];
		FILE *file = fopen(filename, "rb");
	       	fseek(file, 0, SEEK_END);
		int len = ftell(file);
		printf("###%d", len);
		rewind(file);
		fread(html, 512, 1, file);
		html[len] = '\0';
		fclose(file);
		if(strcmp(request_type, "GET") == 0)
		{
			sprintf(reply, "HTTP/1.0 200 OK\r\nServer: Simple-C-Server/0.1\r\nContent-Type: text/html\r\nContent-Language: en-US\r\nConnection: %s\r\nContent-Length: %d\r\n\r\n%s", connection, strlen(html), html);
		}
		else if(strcmp(request_type, "HEAD") == 0)
		{
			sprintf(reply, "HTTP/1.0 200 OK\r\nServer: Simple-C-Server/0.1\r\nContent-Type: text/html\r\nContent-Language: en-US\r\nConnection: %s\r\nContent-Length: %d\r\n\r\n", connection, strlen(html));
		}
	}
	else if(error == -1 && started == true && strcmp(request_type, "POST") == 0)
	{
		char input_string[20] = "<b>Post Data: </b>";
		char link_back[100] = "<br><a href=\"/index.html\">Back to the main page!</a>";
		FILE *file = fopen(filename, "wb");
		fwrite(input_string, strlen(input_string), 1, file);
		fwrite(post_body, strlen(post_body), 1, file);
		fwrite(link_back, strlen(link_back), 1, file);
		fclose(file);
		char html[512];
		FILE *filer = fopen(filename, "rb");
	       	fseek(filer, 0, SEEK_END);
		int len = ftell(filer);
		printf("###%d", len);
		rewind(filer);
		fread(html, 512, 1, filer);
		html[len] = '\0';
		fclose(filer);	
		sprintf(reply, "HTTP/1.0 200 OK\r\nServer: Simple-C-Server/0.1\r\nContent-Type: text/html\r\nContent-Language: en-US\r\nConnection: %s\r\nContent-Length: %d\r\n\r\n%s", connection, strlen(html), html);
	}
	else if (error == 404)
	{
		char html[128] = "<h1>404 ERROR</h1><br><h3>We regret to inform you that your file does not exist. What a shame.</h3>";
		sprintf(reply, "HTTP/1.0 404 NOT FOUND\r\nContent-Length: %ld\r\n\r\n%s", strlen(html), html);
	}
	else if(error == 400)
	{	
		sprintf(reply, "HTTP/1.0 400 BAD REQUEST\r\nContent-Length: 0\r\n\r\n");
	}
	else if(error == 406)
	{
		sprintf(reply, "HTTP/1.0 406 NOT ACCEPTABLE\r\nContent-Length: 0\r\n\r\n");
	}
	else if(error == 505)
	{
		sprintf(reply, "HTTP/1.0 505 HTTP VERSION NOT SUPPORTED\r\nContent-Length: 0\r\n\r\n");
	}
	else
	{
		sprintf(reply, "HTTP/1.0 400 BAD REQUEST\r\nContent-Length: 0\r\n\r\n");
	}
	printf("%s\n", reply);
}

int parse_line(char * buf)
{
	int error = 0;
	if(buf != NULL && post_body_flag == true)
	{
		printf("GRABBING POST BODY\n");
		strcpy(post_body, buf);
		return 200;
	}
	printf("PARSING LINE: %s\n", buf);
	char passbuf[256];
	if(buf == NULL)
	{
		if(post_body_flag == false && strcmp(request_type, "POST") == 0)
		{
			post_body_flag = true;
			return 200;		
		}
		error = -1;
		post_body_flag = false;
		printf("REQUEST ENDED WITH ERROR: %d\n", error);
		return error;
	}
	strcpy(passbuf, buf);
	char * token;
	token = strtok(buf, " ");
	if (strcmp(token, "GET") == 0)
	{
		if (started == true)
		{
			return 400;
		}
		started = true;
		error = parse_get(passbuf);
		return error;
	}
	if (strcmp(token, "HEAD") == 0)
	{
		if (started == true)
		{
			return 400;
		}
		started = true;
		error = parse_head(passbuf);
		return error;
	}
	if (strcmp(token, "POST") == 0)
	{
		if (started == true)
		{
			return 400;
		}
		started = true;
		error = parse_post(passbuf);
		return error;
	}
	else if (strcmp(token, "Host:") == 0)
	{
		return parse_host(passbuf);
	}
	else if (strcmp(token, "Referer:") == 0)
	{
		return  parse_referer(passbuf);
	}
	else if (strcmp(token, "User-Agent:") == 0)
	{
		return parse_user_agent(passbuf);
	}
	else if (strcmp(token, "Accept:") == 0)
	{
		return parse_accept(passbuf);
	}
	else if (strcmp(token, "Content-Type:") == 0)
	{
		return parse_content_type(passbuf);
		
	}
	else if (strcmp(token, "Accept-Language:") == 0)
	{
		return parse_languages(passbuf);
	}
	else if (strcmp(token, "Content-Length:") == 0)
	{
		return parse_content_length(passbuf);
	}
	else if (strcmp(token, "Accept-Encoding:") == 0)
	{
		printf("This server does not currently accept encoding. Encoding options ignored.");
		return 200;
	}
	else if (strcmp(token, "Connection:") == 0)
	{
		return parse_connection_param(passbuf);
	}
	else if (strcmp(token, "Upgrade-Insecure-Requests:")==0)
	{
		printf("This server does not currently support security upgrade requests. Request ignored.");
		return 200;		
	}
	else if (strcmp(token, "Cache-Control:")==0)
	{
		printf("This server does not currently support cache-control. Request ignored.");
		return 200;		
	}
	return 400;
}

int parse_get(char * buf)
{
	int error = 200;
	printf("FULL COMMAND: %s\n", buf);
	strcpy(request_type, "GET");
	printf("COMMAND: %s\n", strtok(buf, " "));
	strcpy(filename, strtok(NULL, " "));
	printf("FILENAME: %s\n", filename);
	strcpy(version_name, strtok(NULL, "\\"));
	printf("VERSION NAME: %s\n", version_name);
	if(strcmp(version_name, "HTTP/1.0") != 0)
	{
		if(strcmp(version_name, "HTTP/1.1") == 0)
		{
			printf("1.1 Requested. This server only supports 1.0. Converted to 1.0");
			strcpy(version_name, "HTTP/1.0");
		}
		else 
		{
			printf("Invalid version name. Please use HTTP/1.0.\n");
			error = 505;
		}
	}
	return error;
}

int parse_head(char * buf)
{
	int error = 200;
	printf("FULL COMMAND: %s\n", buf);
	strcpy(request_type, "HEAD");
	printf("COMMAND: %s\n", strtok(buf, " "));
	strcpy(filename, strtok(NULL, " "));
	printf("FILENAME: %s\n", filename);
	strcpy(version_name, strtok(NULL, "\\"));
	printf("VERSION NAME: %s\n", version_name);
	if(strcmp(version_name, "HTTP/1.0") != 0)
	{
		if(strcmp(version_name, "HTTP/1.1") == 0)
		{
			printf("1.1 Requested. This server only supports 1.0. Converted to 1.0");
			strcpy(version_name, "HTTP/1.0");
		}
		else 
		{
			printf("Invalid version name. Please use HTTP/1.0.\n");
			error = 505;
		}
	}
	return error;
}

int parse_post(char * buf)
{
	int error = 200;
	printf("FULL COMMAND: %s\n", buf);
	strcpy(request_type, "POST");
	printf("COMMAND: %s\n", strtok(buf, " "));
	strcpy(filename, strtok(NULL, " "));
	printf("FILENAME: %s\n", filename);
	strcpy(version_name, strtok(NULL, "\\"));
	printf("VERSION NAME: %s\n", version_name);
	if(strcmp(version_name, "HTTP/1.0") != 0)
	{
		if(strcmp(version_name, "HTTP/1.1") == 0)
		{
			printf("1.1 Requested. This server only supports 1.0. Converted to 1.0");
			strcpy(version_name, "HTTP/1.0");
		}
		else 
		{
			printf("Invalid version name. Please use HTTP/1.0.\n");
			error = 505;
		}
	}
	return error;
}

int parse_host(char * buf)
{
	printf("$PARSING HOST~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(hostname, strtok(NULL, ""));
	printf("HOSTNAME: %s\n", hostname);
	return 200;
}

int parse_referer(char * buf)
{
	printf("$PARSING REFERER~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(referer, strtok(NULL, ""));
	printf("REFERER: %s\n", referer);
	return 200;
}

int parse_content_length(char * buf)
{
	printf("$PARSING CONTENT LENGTH~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(content_length, strtok(NULL, ""));
	printf("CONTENT LENGTH: %s\n", content_length);
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

int parse_connection_param(char * buf)
{
	printf("$PARSING CONNECTION~~ %s\n", buf);
	strtok(buf, " ");
	strcpy(connection, strtok(NULL, ""));
	printf("CONNECTION: %s\n", connection);
	if (strcmp(connection, "keep-alive") != 0)
	{
		printf("This server currently only accepts keep-alive connection requests.");
		return 400;
	}
	return 200;
}

int parse_accept(char * buf)
{
	printf("$NEGOTIATING ACCEPTED CONTENT TYPES~~ %s\n", buf);
	strtok(buf, " ");
	int content_count = 0;
	char * holder;
	holder = strtok(NULL, ",");
	printf("CONTENT TOKEN: %s\n", holder);
	types[content_count] = holder;
	content_count++;
	while(holder != NULL)
	{
		holder = strtok(NULL, ",");
		printf("CONTENT TOKEN: %s\n", holder);
		types[content_count] = holder;
		content_count++;
	}
	bool flag = false;
	for(int i = 0; i < content_count - 1; i++)
	{
		char * mime_type = strtok(types[i], ";");
		if(strcmp(mime_type, "text/html") == 0)
		{
			flag = true;
		}
		else if (strcmp(mime_type, "*/*") == 0)
		{
			flag = true;
		}
	}
	if(flag == true)
	{
		printf("#Valid content type found.\n");
		return 200;
	}
	else
	{
		printf("This server only supports text/html or */*. Please add this to your accept header.\n");
		return 406;
	}
}

int parse_content_type(char * buf)
{
	printf("$NEGOTIATING CONTENT TYPES~~ %s\n", buf);
	strtok(buf, " ");
	int content_count = 0;
	char * holder;
	holder = strtok(NULL, ",");
	printf("CONTENT TOKEN: %s\n", holder);
	content_types[content_count] = holder;
	content_count++;
	while(holder != NULL)
	{
		holder = strtok(NULL, ",");
		printf("CONTENT TOKEN: %s\n", holder);
		content_types[content_count] = holder;
		content_count++;
	}
	bool flag = false;
	for(int i = 0; i < content_count - 1; i++)
	{
		char * mime_type = strtok(content_types[i], ";");
		if(strcmp(mime_type, "application/x-www-form-urlencoded") == 0)
		{
			flag = true;
		}
	}
	if(flag == true)
	{
		printf("#Valid content type found.\n");
		return 200;
	}
	else
	{
		printf("This server only supports application/x-ww-form-urlencoded for POSTing. Please add this to your accept header.\n");
		return 415;
	}
}

int parse_languages(char * buf)
{
	printf("$NEGOTIATING LANGUAGES~~ %s\n", buf);
	strtok(buf, " ");
	int language_count = 0;
	char * holder;
	holder = strtok(NULL, ",");
	printf("CONTENT TOKEN: %s\n", holder);
	languages[language_count] = holder;
	language_count++;
	while(holder != NULL)
	{
		holder = strtok(NULL, ",");
		printf("CONTENT TOKEN: %s\n", holder);
		languages[language_count] = holder;
		language_count++;
	}
	bool flag = false;
	for(int i = 0; i < language_count - 1; i++)
	{
		if(strcmp(languages[i], "en-US") == 0)
		{
			flag = true;
		}
	}
	if(flag == true)
	{
		printf("#Valid Language Found.\n");
		return 200;
	}
	else
	{
		printf("This server only supports en-US. Please add this to your Accept-Language header.\n");
		return 406;
	}
}


