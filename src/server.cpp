/*
    UDP сервер.
	Принимает поток пакетов
	Ведет подсчет принятым пакетам.
	Если обнаруживает пропущенные пакеты,
	посылает запрос к клиенту на повторную передачу пакетов

	В данном примере полезная нагрузка пакета никак не обрабатывается.

	Константин Ильяшенко
	Сентябрь 2017

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
const socklen_t SLEN = sizeof(sockaddr_in);
typedef std::unordered_set<uint32_t> Missed;

void die(const char *s)
{
    perror(s);
    exit(1);
}
 
void handle_new_packets(uint32_t counter, uint32_t last_counter, int socket, sockaddr_in* si_other,
					    Missed& missed_packets)
{
  bool has_missed_packets = (counter - last_counter > 1);
  uint32_t out_buf[SEND_BUF_LEN];
  if(has_missed_packets)
  {
	out_buf[0] = ntohl(last_counter + 1);
	out_buf[1] = ntohl(counter);
	if (sendto(socket, out_buf, sizeof(out_buf), 0, (sockaddr*) si_other, SLEN) == -1)
	{
	  std::cout<<"Send error"<<std::endl;
	  return;
	}
	for(auto i = last_counter + 1; i < counter; i++)
	{
	  std::cout<<"Skipped: "<<i<<std::endl;
	  missed_packets.insert(i);
	} 
  }
}


int setup_socket()
{
  int s;
  sockaddr_in si_me;
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
	die("socket");
  }

  memset((char*)&si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  if( bind(s, (sockaddr*)&si_me, sizeof(si_me) ) == -1)
  {
	die("bind");
  }
  return s;
}

int main(void)
{
    sockaddr_in si_other;
    socklen_t slen = sizeof(sockaddr_in);
	int socket = setup_socket();
    char buf[BUFLEN];
	uint32_t last_counter = 0;
	Missed missed_packets;
     
	if (recvfrom(socket, buf, BUFLEN, 0, (sockaddr*)&si_other, &slen) == -1)
	{
	  die("Recieve first packet");
	}
	uint32_t counter = ntohl(reinterpret_cast<uint32_t&>(buf));
	std::cout<<"First packet: "<<counter<<std::endl;
	last_counter = counter;

    while(1)
	{
	  if (recvfrom(socket, buf, BUFLEN, 0, (sockaddr*)&si_other, &slen) == -1)
	  {
		std::cout<<"Recieve error"<<std::endl;
		continue;
	  }
	  counter = ntohl(reinterpret_cast<uint32_t&>(buf));

	  auto missed = std::find(missed_packets.begin(), missed_packets.end(), counter);
	  bool new_packet = (missed == missed_packets.end());
	  if(new_packet)
	  {
		handle_new_packets(counter, last_counter, socket, &si_other, missed_packets);
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
