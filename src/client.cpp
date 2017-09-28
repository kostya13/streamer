/*
    UDP клиент.
	Посылает пакеты состоящие из заголовка: номер пакета
	и данных.
	Может пропускать некоторые пакеты.
*/
#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <cstdio>
#include <cstdlib> 
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <list>
#include <mutex>
#include <random>
#include <sys/socket.h>
#include <string>
#include <thread>
 
const int SOCKET_ERROR = -1;
const unsigned int BUFLEN =  1028;  
const unsigned int CACHE_SIZE = 1024;  
const bool FOREVER = true;   
const unsigned int RECV_BUF_LEN = 2;


void die(const char *s)
{
    perror(s);
    exit(1);
}


int prepare_socket()
{
    int s; 
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
    {
        die("Prepare socket");
    }
	return s;
}


// Хранилище пакетов, пропущенных сервером
class Container 
{
     std::mutex _lock;
     std::list<uint32_t> _elements;
public:
     void push(uint32_t start, uint32_t end)
	 {
	   _lock.lock();
	   for(auto i = start; i < end; i++)
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

	 bool is_empty()
	 {
	   _lock.lock();
	   bool is_empty = _elements.empty();
	   _lock.unlock();
	   return is_empty;
	 }
};

// Проверка условий принудительного пропуска пакетов
// Настраивается вероятность пропуска
// И количество пропущенных пакетов
class SkipCheck
{
  std::random_device random_device; 
  std::mt19937 generator; 
  std::uniform_int_distribution<> expectation; 
  std::uniform_int_distribution<> count; 
  unsigned int lost_packets;

  public:
	SkipCheck(): 
	  generator(random_device()), 
	  expectation(0, 10000),
	  count(0, 3),
	  lost_packets(0){};

	bool send_allowed()
	{
	  if (lost_packets > 0)
	  {
		lost_packets--;
		return false;
	  }
	  if (expectation(generator) == 0)
	  {
		lost_packets = count(generator);
		return lost_packets > 0;
	  }
	  return true;
	}
};


// Круговой буфер.
// Используется как кэш переданных значений
// Если элемент не найден в кэше, то передается буфер
// заполненный нулями
class Cache
{
  typedef char buf_item[BUFLEN];
  std::array<uint32_t, CACHE_SIZE> index;
  std::array<buf_item, CACHE_SIZE> elements;
  int position = 0;
  public:
	Cache()
	{
	  index.fill(0);
	}

	void add(uint32_t num, char* data)
	{
	  index[position] = num;
	  std::memcpy(elements[position], data, sizeof(buf_item));
	  position++;
	  if(position == CACHE_SIZE)
	  {
		position = 0;
	  }
	}

	void get(uint32_t num, char* data)
	{
	  auto res = std::find(index.cbegin(), index.cend(), num);
	  bool cache_miss = (res == index.cend());
	  if(cache_miss)
	  {
		std::memset(data, 0, sizeof(buf_item));
		uint32_t net_num = htonl(num);
		std::memcpy(data, &net_num, sizeof(num));
	  }
	  else
	  {
		auto packet_index = res - index.cbegin();
		std::memcpy(data, elements[packet_index], sizeof(buf_item));
	  }
	}
};


// Эмулятор источника данных
class Producer
{
  std::ifstream fin;
  public:
	Producer(): 
	  fin("/dev/urandom", std::ios::in | std::ios::binary)
	{
	  if(!fin)
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
	  fin.read(buf, BUFLEN - sizeof(uint32_t));
	}
};


void prepare_buffer(uint32_t counter, Producer& producer, Cache& cache, char* buf)
{
  char payload[BUFLEN - sizeof(uint32_t)];
  uint32_t packet = htonl(counter);
  std::memcpy(buf, &packet, sizeof(packet));
  producer.read(payload);
  std::memcpy(buf + sizeof(packet), payload, BUFLEN - sizeof(packet));
  cache.add(counter, buf);
}

void send_thread(int socket, std::string& server_ip, int port, Container& skipped)
{
  struct sockaddr_in si_other;
  socklen_t slen = sizeof(si_other);
  char buf[BUFLEN];
  uint32_t counter = 0;
  uint32_t last_counter = 0;
  SkipCheck checker;
  Cache cache;
  Producer producer;

  memset((char*) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(port);
  if (inet_aton(server_ip.c_str() , &si_other.sin_addr) == 0) 
  {
	errno = EILSEQ;
	die("inet_aton() failed");
  }

  auto start = std::chrono::steady_clock::now();
  while(FOREVER)
  {
	if(skipped.is_empty())
	{
		prepare_buffer(counter, producer, cache, buf);
		if(checker.send_allowed())
		{
		  if (sendto(socket, buf, BUFLEN, 0 , (sockaddr*)&si_other, slen) == SOCKET_ERROR)
		  {
			std::cout<<"Skip packet"<<std::endl;
		  }
		}
		counter++;
	}
	else
	{
	  uint32_t skipped_num = skipped.pop();
	  cache.get(skipped_num, buf);
	  if (sendto(socket, buf, BUFLEN, 0 , (sockaddr*)&si_other, slen) == SOCKET_ERROR)
	  {
		std::cout<<"Skip packet"<<std::endl;
	  }
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds> 
						  (std::chrono::steady_clock::now() - start);
	if(duration.count() >= 1000)
	{
	  std::cout<<"Bitrate: "<<(counter - last_counter)/1024<<" Mb/сек"<<std::endl;
	  start = std::chrono::steady_clock::now();
	  last_counter = counter;
	}
  }
}

void read_thread(int socket, Container& skipped)
{
  uint32_t buf[RECV_BUF_LEN];
  while(FOREVER)
  {
	auto recieved = recvfrom(socket, buf, sizeof(buf), 0, nullptr, 0);
	if (recieved == SOCKET_ERROR)
	{
	  std::cout<<"Recieve error"<<std::endl;
	  continue;
	}
	if (recieved == sizeof(buf))
	{
	  skipped.push(ntohl(buf[0]), ntohl(buf[1]));
	}
	else
	{
	  std::cout<<"Incorrect data"<<std::endl;
	}
  }
}


int main(int argc, char *argv[])
{
  std::string server("127.0.0.1");
  int port = 8888;   
  if(argc == 1)
  {
	std::cout<<"Use default server: "<<server<<":"<<port<<std::endl;
  }
  else
  {
	const char *opts = "s:p:";
	int opt;
	while((opt = getopt(argc, argv, opts)) != -1) 
	{
	  switch(opt)
	  {
		case 'p': 
		  port = atoi(optarg);
		  break;
		case 's':
		  server.assign(optarg);
		  break;
		default:
		  break;
	  }
	}
	std::cout<<"Use server: "<<server<<":"<<port<<std::endl;
  }
  int socket = prepare_socket();
  Container skipped;
  std::thread t1(send_thread, socket, std::ref(server), port, std::ref(skipped));
  std::thread t2(read_thread, socket, std::ref(skipped));
  t1.join();
  t2.join();
  return 0;
}
