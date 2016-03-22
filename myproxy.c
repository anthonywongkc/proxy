#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>	

# define THREADNUM 100
# define REQUEST_SIZE 8192

char ** splitString(char *line, int type){
    char * temp;
    //here 64 can change to other number, in this program its minimum should be 4
    char cutter0[] = "\r\n";
    char cutter1[] = " ";
    char * cutter;
    char ** buf0 = malloc(sizeof(char *) * 32);
    char ** buf1 = malloc(sizeof(char *) * 8);
    char ** tokens;
    if (type == 0 ){
        tokens = buf0;
        cutter = cutter0;
        free(buf1);
    }
    else {
        tokens = buf1;
        cutter = cutter1;
        free(buf0);
    }
     
    temp = strtok(line, cutter);
    if (!tokens){
        fprintf(stderr, "tokens: allocation error\n");
        exit(EXIT_FAILURE);
      }
    int i = 0;
    while(temp != NULL){
        tokens[i] = temp;
        i++;
        temp = strtok(NULL, cutter);
    }
    tokens[i] = NULL;
    return tokens;
}

void *process_th(void * args){

	int browsersd = *((int*) args);
	int proxysd = *((int*) args + 1);

	char request[REQUEST_SIZE];
	char req_copy[REQUEST_SIZE];

	while(1){
		//receive http request
		int result;
		int i = 0;
		memset(request, 0, REQUEST_SIZE);
		while((result = recv(browsersd, &request[i], sizeof(char), 0))) {
			if (i > 4 && strstr(request, "\r\n\r\n") != NULL){
				break;
			}
			i++;
		}
		request[i+1] = '\0';
		if (result < 0){
			close(browsersd);
			perror("Received fail!\n");
			pthread_exit(NULL);
		}
		result = i;
		//print http request
		printf("RECEIVED HTTP REQUEST: \n");
		if(strlen(request)!=0)printf("%s\n",request);
		printf("Receive requests...(%d Bytes)\n", result);

		memset(req_copy, 0, REQUEST_SIZE);
    	strcpy(req_copy, request);

    	int check = strncmp(request, "GET", 3);
		if (check != 0) {
			close(browsersd);
        	printf("Only GET request is supported!\n");
        	pthread_exit(NULL);
		}

		//check the existence of IMS in http request
        int haveIMS = 0;
        //check the existence of no-cache in http request
        int haveCache = 0;
        //check the existence of keep-alive in http request
        int haveKeepAlive = 0;

		char **firstLine;
        char **hostLine;
        char **cacheLine;
        char **imsLine;
        char requestType[10]; // get or post
        char url[256];
        char hostname[256];
        char IMS[256];// If-modified-since
        int supportedFileType = 0;
        
        //split the http request
        char ** lines = splitString(request, 0);

        int n = 0;
        while(lines[i] != NULL){
            //get the first line of http request, get the request url
            if (n == 0){
                firstLine = splitString(lines[n], 1);
                //requested object will be in firstLine[1]
                memset(requestType, 0, 10);
                strcpy(requestType, firstLine[0]);
                memset(url, 0, 256);
                strcpy(url, firstLine[1]);
                if(strstr(firstLine[1], "html") != NULL || strstr(firstLine[1], "jpg") != NULL || 
                    strstr(firstLine[1], "gif") != NULL || strstr(firstLine[1], "txt") != NULL || 
                    strstr(firstLine[1], "pdf") != NULL || strstr(firstLine[1], "jpeg") != NULL) {
                    supportedFileType = 1;
                }
            }
            //get the host name from http request
            if (strstr(lines[n], "Host") != NULL){
                hostLine = splitString(lines[n], 1);
                memset(hostname, 0, 256);
                strcpy(hostname, hostLine[1]);
            }
            //check whether http request has a cache-control
            if (strstr(lines[n], "Cache-Control") != NULL){
                if (strstr(lines[n], "no-cache") != NULL){
                    haveCache = 1;
                }
            }
            //check whether http request has a If-Modified-Since
            if (strstr(lines[n], "If-Modified-Since") != NULL){
                printf("IMS: %s\n", lines[n]);
                haveIMS = 1;
                imsLine = splitString(lines[n], 1);
                memset(IMS, 0, 256);
                strcpy(IMS, imsLine[1]);
            }
            //check whether http request has a proxy-connection
            if (strstr(lines[n], "Proxy-Connection") != NULL){
                if (strstr(lines[n], "keep-alive") != NULL){
                    haveKeepAlive = 1;
                }
            }
            n++;
        }
        printf("host address: %s\n", hostname);

		/**
		char *req_start;
		char *req_next;
		char *req_end;
		char *content_len;
		char *content_len_last;

		//find host address
    	req_start = strstr(request,"Host:");
		req_next = req_start + 6;
		req_end = strstr(req_start,"\r\n");
		int size = (int)req_end - (int)req_next;
		printf("host address len: %d\n", size);
		char t[256];
		strncpy(t,req_next,size);
		t[size]='\0';
		printf("host address: %s\n", t);

		//get host address from domain name
		int status;
		struct addrinfo hints;
		struct addrinfo *servinfo; // 將指向結果
		memset(&hints, 0, sizeof hints); // 確保 struct 為空
		hints.ai_family = AF_INET; 
		hints.ai_socktype = SOCK_STREAM; 
		if ((status = getaddrinfo(t, "80", &hints, &servinfo)) != 0) {
  			fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
  			exit(1);
		}

		int len;
		
		//print host IP address
		struct sockaddr_in *addr;
    	addr = (struct sockaddr_in *)servinfo->ai_addr; 
		// printf("inet_ntoa(in_addr)sin = %s\n",inet_ntoa((struct in_addr)addr->sin_addr));
		
		int sd = socket(AF_INET,SOCK_STREAM,0);
		if(connect(sd,(struct sockaddr *)addr,sizeof(struct sockaddr))<0){
			printf("connection error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}

		if((len = send(sd,request,strlen(request),0))<0){
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		printf("len of request sending to server: %d\n", len);

		char buff[16000] ="";
		int a =0;
		while(1){
			recv(sd, buff + a, sizeof(char), 0);
			a++;
			//printf("%s\n",request);
			if (strstr(buff, "\r\n\r\n") != NULL){
				break;
			}
		}
		content_len = strstr(buff,"Content-Length");
		content_len_last = strstr(content_len, "\r\n");
		content_len = content_len + 16;
		char value[10];
		memcpy(value, content_len, (int)(content_len_last - content_len));
		value[(int)(content_len_last - content_len)] = '\0';
		int count = atoi(value);
		printf("content len: %d\n", count);
		while(1){
			recv(sd, buff + a, sizeof(char), 0);
			a++;
			count--;
			if (count == 0){
				break;
			}
		}
		buff[a] = '\0';
		printf("RECEIVED INFO: \n");
		if(strlen(buff)!=0)printf("%s\n",buff);
		printf("len of response: %d\n", a);

		if((len = send(browsersd, buff,a,0))<0){
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		printf("len of response sending to client: %d\n", len);
		printf("sent\n");
		close(sd);
		freeaddrinfo(servinfo);**/
    	printf("Connection closed\n");
	}
    close(browsersd);
    pthread_exit(NULL);

}

int main(int argc, char** argv){
	//user input
	if(argc != 2){
		printf("Usage: %s [port]\n", argv[0]);
		exit(0);
	}

	//open socket
	int servsd;
	if ((servsd = socket(AF_INET,SOCK_STREAM,0)) == -1){
		perror("socket()");
     	exit(0);
	}

	//resuing server port
	long val = 1;
    if(setsockopt(servsd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1){
        perror("setsockopt()");
        exit(1);
    }

	struct sockaddr_in proxyaddr;
	struct sockaddr_in browseraddr;

	memset(&proxyaddr, 0, sizeof(proxyaddr));
	proxyaddr.sin_family = AF_INET;
	proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxyaddr.sin_port = htons(atoi(argv[1]));

	if(bind(servsd,(struct sockaddr *) &proxyaddr, sizeof(proxyaddr))<0){
		printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	if(listen(servsd,3)<0){
		printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}

	unsigned int addr_len = sizeof(browseraddr);
	int clisd, i;
	pthread_t thread[THREADNUM];
	
	while (1) {
		printf("start thread [%d]\n", i);
		if((clisd = accept(servsd, (struct sockaddr *)&browseraddr, &addr_len)) == -1){
			printf("accpet error: %s (Errno: %d)\n", strerror(errno), errno);
      		close(clisd);
      		exit(0);
		}; 

		int sd[2];
		sd[0] = clisd;
		sd[1] = servsd;

		if((pthread_create(&thread[i], NULL, process_th, &sd)) != 0){
			printf("thread error: %s (Errno: %d)\n", strerror(errno), errno);
        	exit(0);
		}; 
		pthread_detach(thread[i]);
		//pthread_join(client_thread,NULL);
		i++;
		
		
	}
	close(servsd);
	return 0;
}