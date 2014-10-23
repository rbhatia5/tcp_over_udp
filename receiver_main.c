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

#define MAXBUFLENGTH 1472
#define FRAMESIZE 4

#define DEBUG 1

//Prototypes
void reliablyReceive(char* myUDPport, char* destinationFile);
int create_receiver_socket();

struct current_state {
	int last_inorder_packet;
	
} state;


int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[1]);
	
	reliablyReceive(argv[1], argv[2]);
}


void send_packet(const char* buf, int len, int sockfd, struct sockaddr_storage * their_addr)
{    
    int numbytes;

    if ((numbytes = sendto(sockfd, buf, len, 0, 
             (struct sockaddr*) their_addr, sizeof(struct sockaddr_storage))) == -1) { 
        perror("talker: sendto"); 
        exit(1); 
    }
    //else    printf("talker: sent %d bytes\n", len); 

}


int receive_packet(char* buf, int sockfd, struct sockaddr_storage * their_addr)
{
    int numbytes;
    //struct sockaddr_storage their_addr; 
    socklen_t addr_len; 

    //printf("listener: waiting to recvfrom...\n"); 

    addr_len = sizeof (struct sockaddr_storage); 
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLENGTH , 0, 
        (struct sockaddr *)their_addr, &addr_len)) == -1) { 
        perror("recvfrom"); 
        exit(1); 
    } 
    buf[numbytes] = '\0'; 

	return numbytes;
}

void reliablyReceive(char* myUDPport, char* destinationFile)
{
	
	FILE * pFile;
	state.last_inorder_packet = 0;

	// Check if we can open the file
    pFile = fopen ( destinationFile , "w" );
    if (pFile==NULL) {
        fputs ("File error",stderr); 
        return;
    }

	int sockfd = create_receiver_socket(myUDPport);
	if (sockfd == -1) {
		printf("Failed to open socket!");
		exit(1);
	}

	char * buf = (char *)malloc(MAXBUFLENGTH);
	
	
	while(1) {
		struct sockaddr_storage their_addr;
		int byte_ct = receive_packet(buf, sockfd, &their_addr);
	
		//printf("received a packet!: \n");
		
		
		int packet_num;
		memcpy(&packet_num,buf,4);
		printf("packet number %d had size of %d\n", packet_num, byte_ct);
		if(packet_num == -1){
			printf("send ack %d\n", (int)*buf);
			send_packet(buf, sizeof(int), sockfd, &their_addr);
			break;

		}
			
		//printf("%s", (buf+sizeof(int)));
		//printf("\n---NEW PACKET ---\n");
		char resp[5];
		
		//int packet_num;
		//memcpy(&packet_num, buf, 4);

		
		if(packet_num != state.last_inorder_packet+1)
		{
			printf("here!\n");
			
			//memcpy(resp,&state.last_inorder_packet, sizeof(state.last_inorder_packet));
			resp[0]=state.last_inorder_packet;
			resp[1]=state.last_inorder_packet>>8;
			resp[2]=state.last_inorder_packet>>16;
			resp[3]=state.last_inorder_packet>>24;
			printf("sending ack%d\n",state.last_inorder_packet);
			send_packet(resp, sizeof(int), sockfd, &their_addr);
		}
		else // Packet was the next in ack sequence
		{
			int res = fputs(buf+FRAMESIZE, pFile);
			if(res == EOF)
			{
				//Handle file write error here
			}
			printf("ack=%d\n", state.last_inorder_packet);
			state.last_inorder_packet++;
			resp[0]=state.last_inorder_packet;
			resp[1]=state.last_inorder_packet>>8;
			resp[2]=state.last_inorder_packet>>16;
			resp[3]=state.last_inorder_packet>>24;
			printf("Sending ack%d\n", *resp);
			send_packet(resp, sizeof(int) , sockfd, &their_addr);
			
		}
		
	
		
	}

	free(buf);
	//clean up file
	fclose(pFile);
	
	//clean up socket
	close(sockfd);
	
	
}

int create_receiver_socket(char * myUDPport)
{
	
	struct addrinfo hints, *servinfo, *p;
	int sockfd, rv;
	
	memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4 
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = AI_PASSIVE; // use my IP 

    if ((rv = getaddrinfo(NULL, myUDPport, &hints, &servinfo)) != 0) { 
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); 
        return -1; 
    } 

    // loop through all the results and bind to the first we can 
    for(p = servinfo; p != NULL; p = p->ai_next) { 
        if ((sockfd = socket(p->ai_family, p->ai_socktype, 
                p->ai_protocol)) == -1) { 
            perror("listener: socket"); 
            continue; 
        } 

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { 
            close(sockfd); 
            perror("listener: bind"); 
            continue; 
        } 
        break; 
    } 

    if (p == NULL) { 
        fprintf(stderr, "listener: failed to bind socket\n"); 
        return -1; 
    } 

    freeaddrinfo(servinfo);

	return sockfd;
}