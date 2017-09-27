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
#include <list>
#include <algorithm>
 
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
     
	uint32_t last_counter = 0;
	std::list<uint32_t> absent;
    while(1)
	{

	  if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
	  {
		die("recvfrom()");
	  }
	  uint32_t *packet = (uint32_t*)buf;
	  uint32_t counter = ntohl(*packet);
	  std::cout<<"Get: "<<counter<<std::endl;

	  auto res = std::find(absent.begin(), absent.end(), counter);
	  if(res == absent.end())
	  {
		if(counter - last_counter > 1)
		{
		  for(uint32_t i = last_counter + 1; i < counter; i++)
		  {
			std::cout<<"Skipped: "<<i<<std::endl;
			absent.push_back(i);
		  } 
		}
		last_counter = counter;
	  }
	  else
	  {
		absent.erase(res);
	  }

	  if (!absent.empty())
	  {
		uint32_t required = ntohl(absent.front());
		std::cout<<"Rerquired: "<<absent.front()<<std::endl;
		if (sendto(s,  &required, sizeof(required), 0, (struct sockaddr*) &si_other, slen) == -1)
		{
		  die("sendto()");
		}
	  }
	}
	close(s);
	return 0;
}
