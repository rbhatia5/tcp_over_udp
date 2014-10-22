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


struct state_machine{
	int cong_win_start;
	int cong_win_end;
	int curr_packet;
	int highest_acked_packet;
	enum state_name { CA, SS, FR } conn_state;
	int dupack_ct;
	
} state;

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

    printf("send packet %d\n", *buf);
    if ((numbytes = sendto(sockfd, buf, len, 0, 
             p->ai_addr, p->ai_addrlen)) == -1) { 
        perror("talker: sendto"); 
        exit(1); 
    }
    //else    printf("talker: sent %d bytes\n", len); 

}

int receive_packet(char* buf, int buf_size, int sockfd)
{
    int numbytes;

    //printf("listener: waiting to recvfrom...\n"); 

    if ((numbytes = recv(sockfd, buf, buf_size , 0)) == -1) { 
        perror("recv"); 
        //exit(1); 
    } 
    buf[numbytes] = '\0'; 

	//printf("listener: received %d bytes!\n", numbytes);

	return numbytes;
}

/* -------------------------------------------------------------------------------- */
// Payload Management
/* -------------------------------------------------------------------------------- */

int create_payload(char * pay, FILE * fp, int start_byte, int size,  int seq)
{
	int i = 0;
	//printf("Payload: byte %d to %d\n", start_byte, size+start_byte-1);
	if(size > MAXBUFLENGTH - sizeof(int))
	{
		printf("pay: cannot create payload that large within constraints\n");
		return -1;
	}
	if(fseek(fp, start_byte, SEEK_SET) != 0)
	{
			//Handle seek fail
	} 
	
	memset(pay, seq, sizeof(seq));
	fread(pay+sizeof(int), 1, size, fp);
	//printf("\n----END PACKET ----\n");
	return 0;
}


/* -------------------------------------------------------------------------------- */
// Core functionality
/* -------------------------------------------------------------------------------- */

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, long long int bytesToTransfer)
{
	
	FILE * pFile;
	struct addrinfo *p; // address of recipient
	int i = 0;
	
	//state machine
	state.cong_win_start = 0;
	state.cong_win_end = 1;
	state.curr_packet = 0;
	state.highest_acked_packet = -1;
	state.conn_state = SS;
	state.dupack_ct = 0;
	
	
	
	
	int total_packet_ct = (bytesToTransfer / (MAXBUFLENGTH - sizeof(int))) + 1;
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
    int timeout=100000;

    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
                sizeof(tv)) < 0)
        perror("setsockopt failed\n");
	
	
	
	int j = 0, bytes_sent = 0;
	
	
	while(state.highest_acked_packet+1 < total_packet_ct)
	{
		j = 0;
		//printf("Curr packet is : %d\n", state.curr_packet);
		printf("bytes_sent: %d\n", bytes_sent);
		int store = state.curr_packet;
		while(state.highest_acked_packet < store - 1)
		{
			printf("Respond State\n");
			char recv_buf[MAXBUFLENGTH];
			int numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd);
			
			if(numbytes == 0  )
			{
				//Timed out or received unexpected ack
				state.curr_packet = state.highest_acked_packet+1;
				state.cong_win_start = state.curr_packet;
				state.cong_win_end = state.curr_packet + j;
				bytes_sent-=(state.highest_acked_packet+1) * (MAXBUFLENGTH-sizeof(int));
				//printf("listener: received timeout\n");
				int sockfd = create_socket(hostname, hostUDPport, &p);
				break;

			}
			else if(*recv_buf < state.highest_acked_packet+1) 
			{
				// Ack is behind expectation
				state.curr_packet = state.highest_acked_packet+1;
				int window_size = state.cong_win_end - state.cong_win_start;
				state.cong_win_start = state.curr_packet;
				state.cong_win_end = state.curr_packet + window_size;
				bytes_sent-=(state.highest_acked_packet+1) * (MAXBUFLENGTH-sizeof(int));
				//printf("listener: out of order received ack%d\n", *recv_buf);
			 	sockfd = create_socket(hostname, hostUDPport, &p);
				break;
				
			}
			else if(*recv_buf > state.highest_acked_packet+1) 
			{
				// Ack is ahead of expectation
				state.highest_acked_packet = *recv_buf;
				state.curr_packet = state.highest_acked_packet+1;
				int window_size = state.cong_win_end - state.cong_win_start;
				state.cong_win_start = state.curr_packet;
				state.cong_win_end = state.curr_packet + window_size;
				bytes_sent-=(state.highest_acked_packet+1) * (MAXBUFLENGTH-sizeof(int));
				//printf("listener: out of order received ack%d\n", *recv_buf);
			 	sockfd = create_socket(hostname, hostUDPport, &p);
				break;
			}
			else {
				state.highest_acked_packet++;
				state.cong_win_start++;
				state.cong_win_end++;
				//printf("listener: in order ack %d received\n", *recv_buf);
				//printf("highest acked packet is now %d\n", state.highest_acked_packet);
				
				// Send a packet -----------------------------------
				int next_packet_size;
				if(state.curr_packet + 1 < total_packet_ct ){ // Multiple packets left
					next_packet_size = MAXBUFLENGTH-sizeof(int);
					//printf("multiple remaining\n");
				}
				else if(state.curr_packet + 1 == total_packet_ct){ // last packet
					next_packet_size = bytesToTransfer - bytes_sent;
					//printf("one remaining\n");
				}
				else {
					next_packet_size = 0;
					//printf("end of file\n");
					break;
				}
				char pay[MAXBUFLENGTH];
				int ret = create_payload(pay, pFile, (MAXBUFLENGTH - sizeof(int))*state.curr_packet, next_packet_size, state.curr_packet);
				if(ret == -1)
				{
					printf("ERROR: could not create a payload\n");
					exit(1);
				}
				send_packet(pay, next_packet_size+sizeof(int) , sockfd, p);
				state.curr_packet++;
				// Send a packet ------------------------------------
				
				bytes_sent += next_packet_size; // successfully sent these packets
			}
		
			printf("Ack %d\n",*recv_buf);
		
		}
		
		
		for(; state.curr_packet < state.cong_win_end && state.curr_packet < total_packet_ct; state.curr_packet++)
		{
			printf("Send State\n");
			int next_packet_size;
			if(state.curr_packet + 1 < total_packet_ct) // Multiple packets left
				next_packet_size = MAXBUFLENGTH-sizeof(int);
			else if(state.curr_packet +1 == total_packet_ct)
				next_packet_size = bytesToTransfer-bytes_sent;
			else
				break;

			char pay[MAXBUFLENGTH];
			int ret = create_payload(pay, pFile, (MAXBUFLENGTH-sizeof(int))*state.curr_packet, next_packet_size, state.curr_packet);
			if(ret == -1)
			{
				printf("ERROR: could not create a payload\n");
				exit(1);
			}

			send_packet(pay, next_packet_size+sizeof(int) , sockfd, p);
			bytes_sent += next_packet_size;
			
			j++;
		}
	
		state.cong_win_end += j;
		state.cong_win_start += j;
		
	}
	
	
	/*
	
	while( i < total_packet_ct) {
		
		printf("I have %lld bytes left to transfer\n", bytesToTransfer);
		
		int next_packet_size;
		if(bytesToTransfer > (MAXBUFLENGTH-sizeof(int))) // Multiple packets left
			next_packet_size = MAXBUFLENGTH-sizeof(int);
		else // last packet
			next_packet_size = bytesToTransfer;
		
		char pay[MAXBUFLENGTH];
		create_payload(pay, pFile, (MAXBUFLENGTH-sizeof(int))*i, next_packet_size, i);
		if(pay == NULL)
		{
			printf("ERROR: could not create a payload\n");
			exit(1);
		}
		
		send_packet(pay, next_packet_size+sizeof(int) , sockfd, p);
		last_packet_sent++;
		
		char recv_buf[MAXBUFLENGTH];
		int numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd);
		if(numbytes == 0 || *recv_buf != last_packet_acked+1)
		{
			//last_packet_sent = last_packet_acked;
			i = last_packet_acked+1;
			printf("listener: received timeout\n");
			int sockfd = create_socket(hostname, hostUDPport, &p);
			
		}
		else {
			last_packet_acked++;
			printf("ack %d received\n", *recv_buf);
			bytesToTransfer -= next_packet_size; // successfully sent these packets
			i++;
		}	
		
	}
	*/
	char fin[10];
	int fin_num = -1;
	memcpy(fin, &fin_num, sizeof(int));
	send_packet(fin, 50, sockfd, p);
	
	
	//clean up file
	fclose(pFile);
	
	//clean up socket
	close(sockfd);
	
	
}

