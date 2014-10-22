#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h> 
#include <time.h>


#define MAXBUFLENGTH 1472
#define FRAMESIZE 4

//Prototypes
void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, long long int bytesToTransfer);


int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);
	
	reliablyTransfer(argv[1], argv[2], argv[3], numBytes);
} 

/* -------------------------------------------------------------------------------- */
// socket initialization
/* -------------------------------------------------------------------------------- */
int create_socket(char * hostname, char* hostUDPport, struct addrinfo ** p_ptr)
{
	struct addrinfo *p;
	struct addrinfo hints, *servinfo;
	int sockfd, rv;
	
	memset(&hints, 0, sizeof hints); 
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_DGRAM; 

	if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) { 
	    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));         
	    return -1; 
	} 
	// loop through all the results and make a socket 
	for(p = servinfo; p != NULL; p = p->ai_next) { 
	    if ((sockfd = socket(p->ai_family, p->ai_socktype, 
	            p->ai_protocol)) == -1) { 
	           perror("talker: socket"); 
	        continue; 
	    }  
	    break; 
	} 

	if (p == NULL) { 
	    fprintf(stderr, "talker: failed to bind socket\n"); 
	    return -1; 
	} 
	
	*p_ptr = p;

	freeaddrinfo(servinfo); 

	return sockfd;
}



/* -------------------------------------------------------------------------------- */
// Packet sending and receiving 
/* -------------------------------------------------------------------------------- */
void send_packet(const char* buf, int len, int sockfd, struct addrinfo *p)
{    
    int numbytes;

    //printf("send packet %d\n", *buf);
    if ((numbytes = sendto(sockfd, buf, len, 0, 
             p->ai_addr, p->ai_addrlen)) == -1) { 
        perror("talker: sendto"); 
        exit(1); 
    }
    else    printf("talker: sent %d bytes\n", len); 

}

int receive_packet(char* buf, int buf_size, int sockfd)
{
    int numbytes;

    printf("listener: waiting to recvfrom...\n"); 

    if ((numbytes = recv(sockfd, buf, buf_size , 0)) == -1) { 
        perror("recv"); 
        exit(1); 
    } 
    buf[numbytes] = '\0'; 

	printf("listener: received %d bytes!\n", numbytes);

	return numbytes;
}

/* -------------------------------------------------------------------------------- */
// Payload Management
/* -------------------------------------------------------------------------------- */

char * create_payload(FILE * fp, int start_byte, int size,  int seq)
{
	if(size > MAXBUFLENGTH - FRAMESIZE)
	{
		printf("pay: cannot create payload that large within constraints\n");
		return NULL;
	}
	if(fseek(fp, start_byte, SEEK_SET) != 0)
	{
			//Handle seek fail
	}
	
	char * buf = (char *)malloc(FRAMESIZE + size);
	memset(buf,seq, sizeof(seq));
	fread(buf+FRAMESIZE, 1, size, fp);
	
	return buf;
}


/* -------------------------------------------------------------------------------- */
// Core functionality
/* -------------------------------------------------------------------------------- */

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, long long int bytesToTransfer)
{
	
	FILE * pFile;
	struct addrinfo *p; // address of recipient
	int i = 0;
	
	int total_packet_ct = bytesToTransfer / (MAXBUFLENGTH - FRAMESIZE) + 1;
	int bytes_left = bytesToTransfer;
	int last_packet_sent = -1;
	int last_packet_acked = -1;

	// Check if we can open the file
    pFile = fopen ( filename , "rb" );
    if (pFile==NULL) {
        fputs ("File error",stderr); 
        return;
    }
	
	
	int sockfd = create_socket(hostname, hostUDPport, &p);
	if (sockfd == -1) {
		printf("Failed to open socket!");
		exit(1);
	}
	
	struct timeval tv;
    int timeout = 100000;

    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0)
        printf("setsockopt failed\n");

	
	
	for(i = 0; i < 1; i++) {
		
		int next_packet_size;
		if(bytesToTransfer > (MAXBUFLENGTH-FRAMESIZE)) // Multiple packets left
			next_packet_size = MAXBUFLENGTH-FRAMESIZE;
		else
			next_packet_size = bytesToTransfer;
			
		char * buf = create_payload(pFile, (MAXBUFLENGTH-FRAMESIZE)*i, next_packet_size, 0);
		if(buf == NULL)
		{
			printf("ERROR: could not create a payload\n");
			exit(1);
		}
		
		char recv_buf[MAXBUFLENGTH];
		
		send_packet(buf, next_packet_size , sockfd, p);
		last_packet_sent++;
		
		int numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd);
		if(numbytes == 0)
		{
			printf("listener: received timeout");
		}
		last_packet_acked++;
		printf("%s\n", recv_buf);
		
		bytesToTransfer -= MAXBUFLENGTH; // successfully sent these packets
	}
	
	
	char FIN[] = "-1";
	send_packet(FIN, sizeof(FIN), sockfd, p);
	
	//clean up file
	fclose(pFile);
	
	//clean up socket
	close(sockfd);
	
	
}

