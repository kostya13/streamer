/*
    Simple udp server
*/
#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <unistd.h>
#include <iostream>
 
const unsigned int BUFLEN =  1028;  //Max length of buffer
const unsigned int PORT = 8888;   //The port on which to listen for incoming data
 
void die(const char *s)
{
    perror(s);
    exit(1);
}
 
int main(void)
{
    struct sockaddr_in si_me, si_other;
     
    int s, i, recv_len;
	socklen_t slen = sizeof(si_other);
    char buf[BUFLEN];
     
    //create a UDP socket
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
     
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));
     
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
     
    //bind socket to port
    if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        die("bind");
    }
     
	printf("Waiting for data...");
	fflush(stdout);
	uint32_t last_counter = 0;
    while(1)
	{

	  if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
	  {
		die("recvfrom()");
	  }
	  uint32_t *packet = (uint32_t*)buf;
	  uint32_t counter = ntohl(*packet);

	  if(counter - last_counter > 1)
	  {
		if (sendto(s,  packet, sizeof(*packet), 0, (struct sockaddr*) &si_other, slen) == -1)
		{
		  die("sendto()");
		}
	  }
	  last_counter = counter;
	  //std::cout<<"Counter: "<<counter<<" "<<last_counter<<std::endl;

	}

	close(s);
	return 0;
}
