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


class Container 
{
     std::mutex _lock;
     std::list<uint32_t> _elements;
public:
     void push(uint32_t start, uint32_t end)
	 {
	   _lock.lock();
	   for(uint32_t i = start; i < end; i++)
	   {
		 _elements.push_back(i);
	   }
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
	   _lock.lock();
	   bool is_empty = _elements.empty();
	   _lock.unlock();
	   return is_empty;
	 }

	 size_t size()
	 {
	   _lock.lock();
	   size_t size = _elements.size();
	   _lock.unlock();
	   return size;
	 }
};

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
	  //std::cout<<"Random "<<distribution(generator)<<std::endl; 
	  return (distribution(generator) == 0);
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
	  //std::cout<<"Added: "<<position<<" "<<ntohl(reinterpret_cast<uint32_t&>(*data)) <<std::endl;
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
		//std::cout<<"Index new: "<<ntohl(net_num)<<std::endl;
	  }
	  else
	  {
		auto i = res - index.cbegin();
		//std::cout<<"Index old: "<<num<<" "<<i<<" "<<ntohl(reinterpret_cast<uint32_t&>(*elements[i])) <<std::endl;
		std::memcpy(data, elements[i], sizeof(buf_item));
	  //std::cout<<"Restored: "<<ntohl(reinterpret_cast<uint32_t&>(*data)) <<std::endl;
	  }
	}
};


class Producer
{
  std::ifstream fin;
  std::chrono::microseconds w;
  public:
	Producer(): 
	  fin("/dev/urandom", std::ios::in | std::ios::binary),
	  w(1)
	{
	  if(! fin)
	  {
		die("Producer()");
	  }
	}

	~Producer()
	{
	  fin.close();
	}

	void read(char* buf)
	{
	  //std::this_thread::sleep_for(w);
	  fin.read(buf, BUFLEN - sizeof(uint32_t));
	}
};



void send_stream(int s, Container& skipped)
{
  struct sockaddr_in si_other;
  socklen_t slen = sizeof(si_other);
  char buf[BUFLEN];
  char filebuf[BUFLEN - sizeof(uint32_t)];
  uint32_t counter = 0;
  uint32_t last_counter = 0;
  SkipCheck checker;
  Cache cache;
  Producer producer;

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(PORT);

  if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
  {
	fprintf(stderr, "inet_aton() failed\n");
	exit(1);
  }
  auto start = std::chrono::steady_clock::now();
  while(1)
  {
	//std::cout<<"Size: "<<skipped.size()<<std::endl;
	if(skipped.empty())
	{
	  producer.read(filebuf);
	  uint32_t packet = htonl(counter);
	  std::memcpy(buf, &packet, sizeof(packet));
	  std::memcpy(buf + sizeof(packet), filebuf, BUFLEN - sizeof(packet));
	  if(!checker.skip())
	  {
		//std::cout<<"+Send: "<<counter<<std::endl;
		if (sendto(s, buf, BUFLEN, 0 , (struct sockaddr *) &si_other, slen)==-1)
		{
		  die("sendto()");
		}
	  }
	  //else
	  //{
		//std::cout<<"-Skip: "<<counter<<std::endl;
	  //}
	  cache.add(counter, buf);
	  counter++;
	}
	else
	{
	  uint32_t e = skipped.pop();
	  //std::cout<<"Resend: "<<e<<std::endl;
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

void read_stream(int s, Container& skipped)
{
  while(1)
  {
	uint32_t buf[2];
	auto recieved = recvfrom(s, buf, sizeof(buf), 0, nullptr, 0);
	if (recieved == -1)
	{
	  std::cout<<"Recieve error"<<std::endl;
	  continue;
	}
	if (recieved == sizeof(uint32_t) * 2)
	{
	  skipped.push(ntohl(buf[0]), ntohl(buf[1]));
	}
	else
	{
	  std::cout<<"Incorrect data"<<std::endl;
	}
  }
}


int main(void)
{
  int s = prepare_socket();
  Container skipped;
  std::thread t1(send_stream, s, std::ref(skipped));
  std::thread t2(read_stream, s, std::ref(skipped));
  t1.join();
  t2.join();
  return 0;
}
