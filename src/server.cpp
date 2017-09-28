/*
    Simple udp server
*/
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <unordered_set>
#include<arpa/inet.h>
#include<cstdlib> 
#include<cstring> 
#include<sys/socket.h>
 
const unsigned int BUFLEN =  1028;
const unsigned int PORT = 8888;
const unsigned int SEND_BUF_LEN = 2;
 
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
     
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
     
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
     
    if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        die("bind");
    }
     
	uint32_t last_counter = 0;
	std::unordered_set<uint32_t> missed_packets;

	if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
	{
	  die("Recieve first packet");
	}
	uint32_t counter = ntohl(reinterpret_cast<uint32_t&>(buf));
	std::cout<<"First packet: "<<counter<<std::endl;
	last_counter = counter;
    while(1)
	{
	  if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
	  {
		std::cout<<"Recieve error"<<std::endl;
		continue;
	  }
	  counter = ntohl(reinterpret_cast<uint32_t&>(buf));

	  auto missed = std::find(missed_packets.begin(), missed_packets.end(), counter);
	  bool new_packet = (missed == missed_packets.end());
	  if(new_packet)
	  {
		bool has_missed_packets = (counter - last_counter > 1);
		if(has_missed_packets)
		{
		  uint32_t out_buf[SEND_BUF_LEN];
		  out_buf[0] = ntohl(last_counter + 1);
		  out_buf[1] = ntohl(counter);
		  if (sendto(s,  out_buf, sizeof(uint32_t) * SEND_BUF_LEN , 0, (struct sockaddr*) &si_other, slen) == -1)
		  {
			std::cout<<"Send error"<<std::endl;
			continue;
		  }
		  for(uint32_t i = last_counter + 1; i < counter; i++)
		  {
			  std::cout<<"Skipped: "<<i<<std::endl;
			  missed_packets.insert(i);
		  } 
		}
		last_counter = counter;
	  }
	  else
	  {
		std::cout<<"Get missed packet: "<<*missed<<std::endl;
		missed_packets.erase(missed);
	  }
	}
	return 0;
}
