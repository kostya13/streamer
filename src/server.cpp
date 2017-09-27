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
#include <vector>
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
	bool get_first = false;
	std::vector<uint32_t> absent;
	std::list<uint32_t> wait;
    while(1)
	{

	  if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
	  {
		die("recvfrom()");
	  }
	  uint32_t *packet = (uint32_t*)buf;
	  uint32_t counter = ntohl(*packet);
	  if(!get_first)
	  {
		std::cout<<"First packet: "<<counter<<std::endl;
		get_first = true;
		last_counter = counter;
	  }
	  std::cout<<"Get: "<<counter<<std::endl;

	  auto res = std::find(wait.begin(), wait.end(), counter);
	  if(res == wait.end())
	  {
		if(counter - last_counter > 1)
		{
		  for(uint32_t i = last_counter + 1; i < counter; i++)
		  {
			std::cout<<"Skipped: "<<i<<std::endl;
			absent.push_back(i);
			wait.push_back(i);
		  } 
		}
		last_counter = counter;
	  }
	  else
	  {
		wait.erase(res);
	  }

	  if (!absent.empty())
	  {
		uint32_t out_buf[256];
		std::cout<<"Absent size: "<<absent.size()<<std::endl;
		for(int i=0; i<absent.size(); i++)
		{
			//std::cout<<"Rerquired: "<<absent[i]<<std::endl;
		    out_buf[i] = ntohl(absent[i]);
		}
		if (sendto(s,  out_buf, sizeof(uint32_t)*absent.size() , 0, (struct sockaddr*) &si_other, slen) == -1)
		{
			std::cout<<"Send error"<<std::endl;
			continue;
		}
		absent.clear();
	  }
	}
	close(s);
	return 0;
}
