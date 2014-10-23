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
char * create_payload(char * pay, FILE * fp, int start_byte, int size,  int seq);
int SS=1;

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


	struct timeval tv;
    int timeout=20000;

    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
                sizeof(tv)) < 0)
        perror("setsockopt failed\n");

	freeaddrinfo(servinfo); 

	return sockfd;
}



/* -------------------------------------------------------------------------------- */
// Packet sending and receiving 
/* -------------------------------------------------------------------------------- */
int send_packet(const char* buf, int len, int sockfd, struct addrinfo *p)
{    
    int numbytes;
    int * ptr;
    ptr=malloc(sizeof(int));
    memcpy(ptr,buf,4);
    //printf("send packet %d\n",(int)*ptr);
    if ((numbytes = sendto(sockfd, buf, len, 0, 
             p->ai_addr, p->ai_addrlen)) == -1) { 
        perror("talker: sendto"); 
        //exit(1); 
    }
    //else    printf("talker: sent %d bytes\n", len); 
    return numbytes;
}
void write_header(char* buf, int packet_num)
{
    int* head=(int*)malloc(sizeof(int));
    *head=packet_num;

    memcpy(buf, head, 4);
}

int send_multiple_packet(FILE* pFile, int from, int to, int sockfd, struct addrinfo *p, int last_packet_len, char* pay)
{    
    int number_of_packets=(to-from+1);
    int i;
    //char* buf=(char*)malloc(1472*sizeof(char)); // buffer to read files  /*****why can't I do this!?******/
    int result;

    rewind(pFile);

    //read to the right place
    /*for(i=0;i<from;i++){
        result = fread (buf+4,1,(MAXBUFLENGTH-4), pFile);
        if (result != (MAXBUFLENGTH-4)) {fputs ("Reading error",stderr); exit (3);}
    }*/
    
    for(i=0;i<number_of_packets;i++){
        //check if is the last packet
        if((i+1)==number_of_packets&&last_packet_len!=0){
            //write_header(buf, (int)(from+i));
            create_payload(pay, pFile, (MAXBUFLENGTH-sizeof(int))*(from-1+i), last_packet_len, (from+i));
            //result = fread (buf+4,1,last_packet_len, pFile);
            //if (result != last_packet_len) {fputs ("Reading error",stderr); exit (3);}
            //buf[last_packet_len+4]='\0';
        	if(send_packet(pay, last_packet_len+sizeof(int), sockfd, p)==-1) return -1;
        }
        else{
        	create_payload(pay, pFile, (MAXBUFLENGTH-sizeof(int))*(from-1+i), (MAXBUFLENGTH-sizeof(int)), (from+i));
            //write_header(buf, (int)(from+i));
            //result = fread (buf+4,1,(MAXBUFLENGTH-4), pFile);
            //if (result != (MAXBUFLENGTH-4)) {fputs ("Reading error",stderr); exit (3);}
            //buf[MAXBUFLENGTH]='\0';
        	if(send_packet(pay, MAXBUFLENGTH, sockfd, p)==-1) return -1;
        }


        //send_packet(pay, result+4, sockfd, p);
    }

    return 0; 

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

char * create_payload(char * pay, FILE * fp, int start_byte, int size,  int seq)
{
	int i = 0;
	//printf("Payload: byte %d to %d\n", start_byte, size+start_byte-1);
	if(size > MAXBUFLENGTH - sizeof(int))
	{
		printf("pay: cannot create payload that large within constraints\n");
		return NULL;
	}
	if(fseek(fp, start_byte, SEEK_SET) != 0)
	{
			//Handle seek fail
	} 
	pay[0]=seq;
	pay[1]=(seq>>8);
	pay[2]=(seq>>16);
	pay[3]=(seq>>24);

	//memset(pay, seq, sizeof(seq));
	fread(pay+sizeof(int), 1, size, fp);
	//printf("\n----END PACKET ----\n");
	return pay;
}


/* -------------------------------------------------------------------------------- */
// Core functionality
/* -------------------------------------------------------------------------------- */

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, long long int bytesToTransfer)
{
	
	FILE * pFile;
	struct addrinfo *p; // address of recipient
	int i = 0;
	int j = 0;
	
	int total_packet_ct = (bytesToTransfer / (MAXBUFLENGTH - sizeof(int))) + 1;
	long long int bytes_left = bytesToTransfer;
	int last_packet_sent = -1;
	int last_packet_acked = 0;
	int window_size=1;
	int pre_window;
	int ack_record[3]; //recore dupack
	int dupack_count=0;
	int timeout_count=0;
	char final_buf[4];

    for(j=0;j<3;j++){  // hi eric <3333 - prajit
        ack_record[i]=0;
        }

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
	
    printf("total_packet_ct: %d\n", total_packet_ct);
	
	while( last_packet_acked < total_packet_ct) {
		
		printf("I have %lld bytes left to transfer\n", bytes_left);
		
		int next_packet_size;
		if(bytes_left > (MAXBUFLENGTH-sizeof(int))) // Multiple packets left
			next_packet_size = MAXBUFLENGTH-sizeof(int);
		else // last packet
			next_packet_size = bytes_left;
		

		char pay[MAXBUFLENGTH];

//*****eric code start*******send pack
		printf("**************window_size:  %d *****************\n", window_size);
		if((last_packet_acked+1+window_size)<total_packet_ct){   
			printf("send packet %d to %d\n", last_packet_acked+1, last_packet_acked+window_size);
            if(send_multiple_packet(pFile, last_packet_acked+1, last_packet_acked+window_size, sockfd, p, next_packet_size,pay)==-1){
            	SS=1;
            	printf("here!\n");
            	sockfd = create_socket(hostname, hostUDPport, &p);
            }
            	
            pre_window=window_size;
            last_packet_sent+=window_size;
        }
        else{
        	printf("******************send packet %d to %d\n", last_packet_acked+1, total_packet_ct);
            if(send_multiple_packet(pFile, last_packet_acked+1, total_packet_ct, sockfd, p, bytes_left%(MAXBUFLENGTH-sizeof(int)),pay)==-1){
            	SS=1;
            	printf("*************************here!\n");
            	sockfd = create_socket(hostname, hostUDPport, &p);
            }
            	
            pre_window=total_packet_ct-last_packet_acked;
            //last_packet_sent+=total_packet_ct-last_packet_acked;
        }

//*****eric code end*******send pack

			
	/*	create_payload(pay, pFile, (MAXBUFLENGTH-sizeof(int))*i, next_packet_size, i);
		if(pay == NULL)
		{
			printf("ERROR: could not create a payload\n");
			exit(1);
		}
	
		send_packet(pay, next_packet_size+sizeof(int) , sockfd, p);
		last_packet_sent++;
	*/	


//*****eric code start*******recv ack
		//printf("start recv!!!\n");
		for(j=0;j<pre_window;j++){
            char recv_buf[MAXBUFLENGTH];
			int numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd);
			int recv_ack;
			memcpy(&recv_ack,recv_buf,4);
			if(numbytes == -1)
			{
				//last_packet_sent = last_packet_acked;
				//i = last_packet_acked+1;
				printf("listener: received timeout\n");
				
				sockfd = create_socket(hostname, hostUDPport, &p);
				dupack_count=0;
				timeout_count++;
				if(timeout_count==3) 
				{
					SS=1;
					timeout_count=0;
					break;
				}
					
			}
			else if(recv_ack <= last_packet_acked){
				dupack_count++;
				timeout_count=0;
				printf("here\n");
				for(j=0;j<(last_packet_acked-recv_ack);j++){
					if(numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd)==-1)
        			{
        				sockfd = create_socket(hostname, hostUDPport, &p);
        				break;
        			}
        			memcpy(&recv_ack,recv_buf,4);
        			if(recv_ack>last_packet_acked) last_packet_acked=recv_ack;
				}

				if (dupack_count==3)
				{
					printf("dup ack %d received\n", recv_ack);
					SS=0;
        			window_size/=2;

        			if (window_size==0)
      		  		{
            			window_size=1;
        			}
        			dupack_count=0;
        			//exit(1);

        			ack_record[0]=recv_ack;
        			while(recv_ack<=last_packet_acked){
        				printf("keep recv!!!!!!!!!!!\n");
        				if(numbytes = receive_packet(recv_buf, MAXBUFLENGTH , sockfd)==-1)
        				{
        					sockfd = create_socket(hostname, hostUDPport, &p);
        					break;
        				}
        					
						memcpy(&recv_ack,recv_buf,4);

        			}
        			break;
				}

        		ack_record[2]=ack_record[1];
        		ack_record[1]=ack_record[0];
        		ack_record[0]=last_packet_acked;
			}
			else {
				if(SS==1||(j+1)==pre_window)  
					window_size++;
				
				//last_packet_acked++;
				last_packet_acked=recv_ack;
				//printf("ack %d received\n", recv_ack);
				bytes_left =bytesToTransfer- (last_packet_acked*(MAXBUFLENGTH-sizeof(int))); // successfully sent these packets
				dupack_count=0;
				timeout_count=0;
				//i++;
				//i = last_packet_acked;
			}	
			

			/*if (ack_record[0]!=0&&ack_record[1]==ack_record[2]&&ack_record[0]==ack_record[1])
    		{
    			printf("dup ack %d received\n", recv_ack);
        		/* dup_ack received */
        		/*SS=0;
        		window_size/=2;
        		if (window_size==0)
      		  	{
            		window_size=1;
        		}
        		break;
    		}*/


        }


        //update ack_record
        
    	
//*****eric code end*******recv ack
		/*char recv_buf[MAXBUFLENGTH];
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
		}*/
		
	}
	
	char fin[10];
	int fin_num = -1;
	memcpy(fin, &fin_num, sizeof(int));
	do{
		printf("send fin\n");
		send_packet(fin, 50, sockfd, p);
	}while(receive_packet(final_buf, MAXBUFLENGTH , sockfd)==-1);

	//clean up file
	fclose(pFile);
	
	//clean up socket
	close(sockfd);
	
	
}

