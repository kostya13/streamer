/*
    Simple udp client
*/
#include <thread>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <random>
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <unistd.h>
#include <list>
#include <mutex>
#include <map>
#include <vector>
#include <array>
#include <algorithm>

 
const char* SERVER = "127.0.0.1";
const unsigned int BUFLEN =  1028;  //Max length of buffer
const unsigned int PORT = 8888;   //The port on which to listen for incoming data
const unsigned int CACHE_SIZE = 1024;  


class Container 
{
     std::mutex _lock;
     std::list<uint32_t> _elements;
public:
     void push(uint32_t element)
	 {
	   _lock.lock();
	   _elements.push_back(element);
	   _lock.unlock();
	 }

     uint32_t pop()
	 {
	   uint32_t e;
	   _lock.lock();
	   e = _elements.front();
	   _elements.pop_front();
	   _lock.unlock();
	   return e;
	 }

	 bool empty()
	 {
	   return _elements.empty();
	 }

};


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


class SkipCheck
{
  std::random_device random_device; // Источник энтропии.
  std::mt19937 generator; // Генератор случайных чисел.
  std::uniform_int_distribution<> distribution; // Равномерное распределение [10, 20]

  public:
	SkipCheck(): 
	  generator(random_device()), 
	  distribution(0, 10) {};

	bool skip()
	{
	  return (distribution(generator) == 2);
	}
};


class Cache
{
  public:
	Cache()
	{
	  index.fill(0);
	}
  typedef  char buf_item[BUFLEN];
	std::array<uint32_t, CACHE_SIZE> index;
	std::array<buf_item, CACHE_SIZE> elements;
	int position = 0;
	void add(uint32_t num, char* data)
	{
	  std::cout<<"Added: "<<position<<" "<<ntohl(reinterpret_cast<uint32_t&>(*data)) <<std::endl;
	  index[position] = num;
	  std::memcpy(elements[position], data, sizeof(buf_item));
	  position++;
	  if( position == CACHE_SIZE)
	  {
		position = 0;
	  }
	}

	void get(uint32_t num, char* data)
	{
	  auto res = std::find(index.cbegin(), index.cend(), num);
	  if(res == index.cend())
	  {
		std::memset(data, 0, sizeof(buf_item));
		uint32_t net_num = htonl(num);
		std::memcpy(data, &net_num, sizeof(num));
		std::cout<<"Index new: "<<ntohl(net_num)<<std::endl;
	  }
	  else
	  {
		auto i = res - index.cbegin();
		std::cout<<"Index old: "<<num<<" "<<i<<" "<<ntohl(reinterpret_cast<uint32_t&>(*elements[i])) <<std::endl;
		std::memcpy(data, elements[i], sizeof(buf_item));
	  //std::cout<<"Restored: "<<ntohl(reinterpret_cast<uint32_t&>(*data)) <<std::endl;
	  }
	}
};

Container skipped;

void send_stream(int s)
{
  struct sockaddr_in si_other;
  socklen_t slen = sizeof(si_other);
  char buf[BUFLEN];
  char filebuf[BUFLEN - 4];
  uint32_t counter = 0;
  uint32_t last_counter = 0;
  SkipCheck checker;
  Cache cache;

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
	if(skipped.empty())
	{
	  fin.read(filebuf, BUFLEN - 4);
	  uint32_t packet = htonl(counter);
	  std::memcpy(buf, &packet, sizeof(packet));
	  std::memcpy(buf + sizeof(packet), filebuf, BUFLEN - 4);
	  std::chrono::microseconds w( 50000 );
	  std::this_thread::sleep_for( w );
	  if(!checker.skip())
	  {
		//std::cout<<"Send: "<<counter<<std::endl;
		if (sendto(s, buf, BUFLEN, 0 , (struct sockaddr *) &si_other, slen)==-1)
		{
		  die("sendto()");
		}
	  }
	  cache.add(counter, buf);
	  counter++;
	}
	else
	{
	  uint32_t e = skipped.pop();
	  std::cout<<"Resend: "<<e<<std::endl;
	  cache.get(e, buf);
	  //std::cout<<"From buf: "<<ntohl(reinterpret_cast<uint32_t&>(*buf)) <<std::endl;
	  if (sendto(s, buf, BUFLEN, 0 , (struct sockaddr *) &si_other, slen)==-1)
	  {
		die("sendto()");
	  }
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds> 
						  (std::chrono::steady_clock::now() - start);
	if(duration.count() == 1000)
	{
	  std::cout<<"Bitrate: "<<(counter - last_counter)/1024<<" Mb/сек"<<std::endl;
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
    uint32_t required = ntohl(packet);
	skipped.push(required);
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
