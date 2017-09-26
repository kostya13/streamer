/*
    Simple udp client
*/
#include <thread>
#include <iostream>
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <unistd.h>

 
const char* SERVER = "127.0.0.1";
const unsigned int BUFLEN =  512;  //Max length of buffer
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
 
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 1;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	
	return s;
}

void send_stream()
{
  struct sockaddr_in si_other;
  socklen_t slen = sizeof(si_other);
  char buf[BUFLEN];
    const char* message = "message";
	int s = prepare_socket();

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    while(1)
    {
        printf("Message : %s", message);
         
        //send the message
        if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
        {
            die("sendto()");
        }
        std::chrono::milliseconds w(20);
		std::this_thread::sleep_for(w);		
        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
        memset(buf,'\0', BUFLEN);
        //try to receive some data, this is a blocking call
        if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == -1)
        {
            die("recvfrom()");
        }
         
        puts(buf);
    }
 
    close(s);
}

int main(void)
{
  std::thread t(send_stream);
  t.join();
  return 0;
}
