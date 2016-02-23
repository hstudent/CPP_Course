#include <iostream>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

using namespace std;

#define MAX_ARG	1024
#define SERVER "Server: Dynamic Thread\n"
#define CONTENT "Content-Type: text/html\r\n"


char ip[MAX_ARG] = {0}, port[MAX_ARG] = {0}, directory[MAX_ARG] = {0};
bool daemon_mode = true;

void serve(int sock, char *path);  //Serve the file
void notFound(int sock); //Send HTTP header when a file is not found (404)
void outputFile(int sock, FILE *html); //Reads input from file and sends it to client

void error(const char *msg)
{
 perror(msg);
 exit(1);
}

void parseHttpReq(const char *buffer, int bufLen, int sock)
{

    	/*FILE* pFile = fopen("/tmp/dump.txt", "w");
    	fprintf(pFile, "%s", buffer);
    	fclose(pFile);*/

	  char method[MAX_ARG];
	  char path[MAX_ARG];
	  char version[MAX_ARG];

	  //parse the HTTP method, path to the file and the HTTP version
	  int sres = sscanf(buffer, "%s %s %s", method, path, version);
	  if (sres == 3 && strcmp(method, "GET") == 0
		  && (strcmp(version, "HTTP/1.0") || strcmp(version, "HTTP/1.1")))
	  {
		   serve(sock, path);  //Send file
	  }
	  else
	  {
	  }
}

void goodRequest(int sock, int size)
{
	char buffer[1024];
	//Send HHTP Response line by line
	strcpy(buffer, "HTTP/1.0 200 Ok\r\n");
	send(sock, buffer, strlen(buffer), 0);
	strcpy(buffer, SERVER);
	send(sock, buffer, strlen(buffer), 0);
	// sending content length
	sprintf(buffer, "Content-Length: %i\r\n", size);
	send(sock, buffer, strlen(buffer), 0);
	strcpy(buffer, CONTENT);
	send(sock, buffer, strlen(buffer), 0);
	strcpy(buffer, "\r\n");
}

void notFound(int sock)
{
	char buffer[1024];
	  //Send HTTP Response line by line
	  strcpy(buffer, "HTTP/1.0 404 Not Found\r\n");
	  write(sock, buffer, strlen(buffer));
	  strcpy(buffer, CONTENT);
	  write(sock, buffer, strlen(buffer));

	  const char* pNotFoundPage = "<html>\n<head>\n<title>Not Found</title>\n</head>\r\n<body>\n<p>404 Request file not found.</p>\n</body>\n</html>\r\n";
	  int size = strlen(pNotFoundPage);

	  sprintf(buffer, "Content-length: %i\n",  size);
      write(sock, buffer, strlen(buffer));
      write(sock, "Connection: close\r\n", strlen("Connection: close\r\n"));
	  strcpy(buffer, "\r\n");

	  write(sock, buffer, strlen(buffer));
  	  strcpy(buffer, "<html>\n<head>\n<title>Not Found</title>\n</head>\r\n");
  	  write(sock, buffer, strlen(buffer));
  	  strcpy(buffer, "<body>\n<p>404 Request file not found.</p>\n</body>\n</html>\r\n");
  	  write(sock, buffer, strlen(buffer));
}//End of notFound

void outputFile(int sock, FILE *html)
{
	fseek (html , 0 , SEEK_END);
	unsigned int lSize = ftell (html);
	rewind (html);
	char* buffer = (char*) malloc (sizeof(char)*lSize);
	unsigned int result = fread(buffer,1,lSize,html);
	if (result == lSize)
	{
		write(sock, buffer, lSize);
	}
	free(buffer);
}

void serve(int sock, char *path)
{
	char targetFile[MAX_ARG];

	bool bOpen = true;
	//determine file name
	if(strcmp(path, "/") == 0)
	{
		//bOpen = false;
		strcpy(targetFile, directory);
		strcat(targetFile, "/index.html");
	}
	else
	{
		// additional path processing - extract file name
		char* qPos = strchr(path, '?');
		if (qPos != NULL)
		{
			path[qPos - path] = 0;
		}
		strcpy(targetFile, directory);
		//int len = strlen(directory);
		strcat(targetFile, path);
	}

	//Open file
	FILE *html = NULL;
	if (bOpen)
	{
			html = fopen(targetFile, "r");
	}
	if(html == NULL)
	{
		notFound(sock);
	}
	else
	{
		fseek (html , 0 , SEEK_END);
		int size = ftell (html);
		rewind (html);

		write(sock, "HTTP/1.0 200 OK\n", 16);
		char buffer[1024];
		sprintf(buffer, "Content-length: %i\n",  size);
		write(sock, buffer, strlen(buffer));
		write(sock, "Connection: close\n", strlen("Connection: close\n"));
		write(sock, "Content-Type: text/html\n\n", 25);

	     outputFile(sock, html);
		 fclose(html);
	}
}

void proc_exit(int param)
{
		while (true)
		{
			int wstat;
			pid_t pid = wait3 (&wstat, WNOHANG, (struct rusage *)NULL );
			if (pid == 0 || pid == -1)
				return;
			else
			{
				if (!daemon_mode)
				{
					cout << "child exit. pid = " << pid << " code = " << wstat << endl;
				}
			}
		}
}

int deamoncode()
{
	// create server socket
	int servsock = socket(AF_INET, SOCK_STREAM, 0);
	if (servsock < 0)
	  error("ERROR opening server socket");

	int enable = 1;

	if (setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	    error("setsockopt(SO_REUSEADDR) failed");

	if (setsockopt(servsock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
	    error("setsockopt(SO_REUSEPORT) failed");

	sockaddr_in serv_addr;
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	int nPort = atoi(port);
	serv_addr.sin_port = htons(nPort);
	serv_addr.sin_addr.s_addr = inet_addr(ip);

	if (bind(servsock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	  error("ERROR on binding");

	listen(servsock,5);

	if (daemon_mode)
	{
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	sigignore(SIGINT);
	signal (SIGCHLD, proc_exit);

	while (1)
	{
		sockaddr cli_addr;
		socklen_t clilen = sizeof(cli_addr);
		int newsockfd = accept(servsock, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
		  error("ERROR on accept");

		if (fork() == 0)
		{
			//while(true)
			{
				char buf[4096];
				ssize_t bytes_read = recv(newsockfd, buf, sizeof(buf), 0);
				buf[bytes_read] = 0;
				parseHttpReq(buf, bytes_read, newsockfd);
			}
			close(newsockfd);
			exit(0);
		}
	}
	close(servsock);

	return 0;
}

int main(int argc, char* argv[])
{
	int rez = 0;
	while ( (rez = getopt(argc,argv,"h:p:d:t")) != -1)
	{
			switch (rez)
			{
				case 'h': strncpy(ip, optarg, MAX_ARG); break;
				case 'p': strncpy(port, optarg, MAX_ARG); break;
				case 'd': strncpy(directory, optarg, MAX_ARG); break;
				case 't': daemon_mode = false; break;
	        };
	};

	cout << "Parameters: ip=" << ip << ", port=" << port << ", directory=" << directory << ", deamon=" << daemon_mode << endl;

	if (chdir(directory) != 0)
	{
		error("UNABLE to change dir.");
	}

	if (daemon_mode)
	{
		// create daemon
		if (fork() == 0)
		{
			return deamoncode();
		}
	}
	else
	{
		deamoncode();
	}

	return 0;
}
