/*
    Simple udp client
*/
#include <thread>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <unistd.h>

 
const char* SERVER = "127.0.0.1";
const unsigned int BUFLEN =  1028;  //Max length of buffer
const unsigned int PORT = 8888;   //The port on which to listen for incoming data


void die(const char *s)
{
    perror(s);
    exit(1);
}
 
int prepare_socket()
{
    int s; 
 
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
 
	//struct timeval read_timeout;
	//read_timeout.tv_sec = 0;
	//read_timeout.tv_usec = 1;
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	return s;
}

void send_stream(int s)
{
  struct sockaddr_in si_other;
  socklen_t slen = sizeof(si_other);
  char buf[BUFLEN];
  char filebuf[BUFLEN - 4];
  uint32_t counter = 0;
  uint32_t last_counter = 0;

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(PORT);

  if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
  {
	fprintf(stderr, "inet_aton() failed\n");
	exit(1);
  }
  auto start = std::chrono::steady_clock::now();
  std::ifstream fin("/dev/urandom", std::ios::in | std::ios::binary);
  if(! fin)
  {
	die("random()");
  }
  while(1)
  {
	fin.read(filebuf, BUFLEN - 4);
    uint32_t packet = htonl(counter);
	std::memcpy(buf, &packet, sizeof(packet));
	std::memcpy(buf + sizeof(packet), filebuf, BUFLEN - 4);
	if (sendto(s, buf, BUFLEN, 0 , (struct sockaddr *) &si_other, slen)==-1)
	{
	  die("sendto()");
	}
	counter++;
	//if((counter % 1024) == 0)
	//{
	  //auto duration = std::chrono::duration_cast<std::chrono::milliseconds> 
                            //(std::chrono::steady_clock::now() - start);
	  //std::cout<<"Time: "<<duration.count()<<std::endl;
	  //start = std::chrono::steady_clock::now();
	//}
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds> 
						  (std::chrono::steady_clock::now() - start);
	if(duration.count() == 1000)
	{
	  std::cout<<"Bitrate: "<<(counter - last_counter)/1024<<"Mb/сек"<<std::endl;
	  start = std::chrono::steady_clock::now();
	  last_counter = counter;
	}
  }
}

void read_stream(int s)
{
  while(1)
  {
    uint32_t packet;
	if (recvfrom(s, &packet, sizeof(packet), 0, nullptr, 0) == -1)
	{
	  std::cout<<"Recieve error"<<std::endl;
	  continue;
	}
    uint32_t counter = ntohl(packet);
	std::cout<<counter<<std::endl;
  }
}

int main(void)
{
  int s = prepare_socket();
  std::thread t1(send_stream, s);
  std::thread t2(read_stream, s);
  t1.join();
  t2.join();
  return 0;
}
