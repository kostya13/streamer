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
#include "constants.h"
 
const unsigned int RECV_BUF_LEN = HEADER_LEN + PAYLOAD_LEN_MAX;
const unsigned int SEND_BUF_LEN = 2;
const socklen_t SLEN = sizeof(sockaddr_in);
typedef std::unordered_set<uint32_t> Missed;


void die(const char *s)
{
    perror(s);
    exit(1);
}
 

void check_missed(uint32_t counter, uint32_t last_counter, 
	                   int socket, sockaddr_in* si_other,
                       Missed& missed_packets)
{
  uint32_t out_buf[SEND_BUF_LEN];
  bool has_missed_packets = ((counter - last_counter) > 1);
  if(has_missed_packets)
  {
	out_buf[0] = ntohl(last_counter + 1);
	out_buf[1] = ntohl(counter);
	if (sendto(socket, out_buf, sizeof(out_buf), 0, (sockaddr*) si_other, SLEN) == SOCKET_ERROR)
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


int prepare_socket(unsigned int port)
{
  int s;
  sockaddr_in si_me;
  if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
  {
	die("socket");
  }

  memset((char*)&si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  if( bind(s, (sockaddr*)&si_me, sizeof(si_me) ) == SOCKET_ERROR)
  {
	die("bind");
  }
  return s;
}


void check_options(unsigned int port)
{
  if(port < MIN_PORT)
  {
	std::cout<<"Port: "<<port<<std::endl;
	die("Incorrect port");
  }
}

bool is_good_packet(size_t length)
{
  return (length > HEADER_LEN) and (length <= HEADER_LEN + PAYLOAD_LEN_MAX);
}


bool use_payload(size_t length, char* buf)
{
  if (length == SOCKET_ERROR)
  {
	return false;
  }
  else if (is_good_packet(length))
  {
	// Здесь обрабатывается пакет
	return true;
  }
  return false;
}


int main(int argc, char *argv[])
{
  sockaddr_in si_other;
  socklen_t slen = sizeof(sockaddr_in);
  unsigned int port = DEFAULT_PORT;
  char buf[RECV_BUF_LEN];
  uint32_t last_counter = 0;
  Missed missed_packets;
  size_t recieved;
  uint32_t counter;

  if(argc == 1)
  {
	std::cout<<"Use default port: "<<port<<std::endl;
  }
  else
  {
	const char *opts = "p:";
	int opt;
	while((opt = getopt(argc, argv, opts)) != -1) 
	{
	  switch(opt)
	  {
		case 'p': 
		  port = atoi(optarg);
		  break;
		default:
		  break;
	  }
	}
	check_options(port);
	std::cout<<"Use port: "<<port<<std::endl;
  }

  int socket = prepare_socket(port);
  recieved = recvfrom(socket, buf, RECV_BUF_LEN, 0, (sockaddr*)&si_other, &slen);
  if(use_payload(recieved, buf))
  {
	counter = ntohl(reinterpret_cast<uint32_t&>(buf));
	std::cout<<"First packet: "<<counter<<std::endl;
	last_counter = counter;
  }
  else
  {
	die("Can't recieve packets");
  }

  while(FOREVER)
  {
	recieved = recvfrom(socket, buf, RECV_BUF_LEN, 0, (sockaddr*)&si_other, &slen);
	if(!use_payload(recieved, buf))
	{
	  continue;
	}
	counter = ntohl(reinterpret_cast<uint32_t&>(buf));

	auto missed = std::find(missed_packets.begin(), missed_packets.end(), counter);
	bool new_packet = (missed == missed_packets.end());
	if(new_packet)
	{
	  check_missed(counter, last_counter, socket, &si_other, missed_packets);
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
