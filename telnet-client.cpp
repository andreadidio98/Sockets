//Author: Andrea Di Dio (s2593888)
//Date-of-creation: 11-03-2018
//Date-last-modified: 22-03-2018
//Title: TELNET Client

#include <iostream>
#include <string>
#include <sstream>
#include <stdio.h> /*included various C libs because i preferred managing many functions/variables in "C-style"
                     (Bjarne Stroustrup would probably kill me :) )*/
#include <stdlib.h>
#include <memory.h>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>//socket management
#include <netinet/in.h>//socket management
#include <sys/types.h>
#include <netdb.h>//socket management
#include <arpa/inet.h>//socket management
#include <termios.h>//To manage the terminal and "spawn" a terminal in raw mode instead of cooked
#include <sys/select.h>/*for fd_set typedef, helps synchronising I/O
                         using pselect(), I can monitor file descriptors
                         and wait until one of these become ready for any
                         I/O operation. i.e., This helps sync the client's
                         and server's I/O operations which avoids undefined
                         behaviour when the server is sending/waiting_to_recv data
                         unexpectedly causing recv() to "hang".*/
#define HTTP_PORT 80

#define IP_PROTOCOL 0

#define MAX_BUFFER_SIZE 1023 //not 1024 to avoid undefined behaviour with FD_SETSIZE (pselect()) --see README Section 4.1
#define NO_FLAGS 0

//Options:
#define IAC 255 //Starts negotiation sequence (Interpret As Command- IAC)
#define DONT 254 //Indicates host is NOT willing to negotiate
#define DO 253 //Indicates host IS willing to negotiate
#define WONT 252 //Confirmation host is NOT willing to negotiate
#define WILL 51 // Confirmation host IS willing to negotiate
#define SB 250 // Starts sub-negotiation options
#define SE 240 // Ends sub-negotiation options
#define NOP 241 // No operation
#define IS 0 //IS command sub-negotiation
#define SEND 1 // SEND command sub-negotiation
#define SUPPRESS_GO_AHEAD 3//SGA option
#define TERMINAL_TYPE 24 //TTYPE option
#define NAWS 31 // Window resizing option

using namespace std;

class Environment{ //uses termios.h to handle terminal
  private:
    struct termios oldEnvironment, newEnvironment;
    const int FILE_DESCRIPTOR = 0;
    const int TIME_OF_CHANGE = TCSADRAIN;
    int get_attr_status;
    int set_attr_status;
  public:
    void makeTemporaryTerminal();
    int getFileDescriptor();

};

class HTTP{ //handles the HTTP GET request
  private:
    string URI_path;

  public:
    string HTTPGet(string, string);
};


class ClientSocket{ //socket management class
  private:
    int client_sock;
    struct sockaddr_in server_host;
    hostent* ip_address;
    short port;
    const int DOMAIN = AF_INET;//IPv4 family defined in sys/socket.h
    const int TYPE = SOCK_STREAM;//TCP Type defined in sys/socket.h
  public:
    void createSocket(hostent*, short);
    void sendHTTPGET(string, string);
    unsigned char* receiveData(unsigned char[], int, int);
    void negotiate(unsigned char [3]);
    int getSocket();
    bool checkForData(fd_set&, struct timespec&);
    void sendData(unsigned char[], int);
};

int error(string error_message){ //prints error and exits program
  cerr << error_message << endl;
  exit(EXIT_FAILURE);
}

string HTTP::HTTPGet(string uri, string host) { //puts together parts of the GET request given a path
  std::ostringstream buf;
  buf << "GET " << uri << " HTTP/1.1\r\n\r\n";
  string smth = buf.str();
  cout<<smth <<endl;
  return buf.str();
}

int Environment::getFileDescriptor(){
  return FILE_DESCRIPTOR;
}

void Environment::makeTemporaryTerminal() { //"spawns" a terminal in raw mode except for ISIG flag -- see README Section 4.2
    get_attr_status = tcgetattr(FILE_DESCRIPTOR, &oldEnvironment);

    if(get_attr_status == -1) {
      error("Could get the termios attributes for the current terminal");
    }

    memcpy(&newEnvironment, &oldEnvironment, sizeof(oldEnvironment));
    cfmakeraw(&newEnvironment);//sets the new terminal to "raw mode" where input is made char-by-char as per telnet specifications
    newEnvironment.c_lflag |= (ISIG);//still want ^C to work
    set_attr_status = tcsetattr(FILE_DESCRIPTOR,TIME_OF_CHANGE,&newEnvironment);

    if(set_attr_status == -1) {
      error("Could not \"spawn\" a new terminal environment");
    }
}


bool ClientSocket::checkForData(fd_set& sync, struct timespec& timeout){ //uses pselect to sync I/O operations --see README Section 4.3
    int sync_io_with_server;
    sync_io_with_server = pselect(client_sock + 1, &sync, NULL, NULL, &timeout, NULL);//socket + 1 because param is the highest-numbered file descriptor + 1...avoids deadlocking 
    if(sync_io_with_server == -1) {
        error("Could not sync I/O operations with remote host");
    }
    else if(sync_io_with_server == 0) {
      timeout.tv_sec = 10;
      timeout.tv_nsec = 0;
    }
    else if(sync_io_with_server > 0) {
        return true;
    }
}

int ClientSocket::getSocket(){ //returns sockfd
	return client_sock;
}

void ClientSocket::createSocket(hostent* address, short portNum){ //sets up connection with remote host

  client_sock = socket(DOMAIN, TYPE, IP_PROTOCOL);
  if(client_sock == -1) {
    error("ERROR: Cannot create the Socket");
  }
  else{
    cout << "SUCCESS: Socket created Succesfully!!! :)\n";
  }

  memcpy(&server_host.sin_addr, address->h_addr_list[0], address->h_length);
  server_host.sin_family = DOMAIN;
  server_host.sin_port = htons(portNum);//host to network short
  int server_host_size = sizeof(server_host);

  int connection = connect(client_sock, (struct sockaddr*) &server_host, server_host_size);

  if(connection < 0) {
    error("ERROR: Cannot connect to host");
  }
  else {
    cout << "SUCCESS: Connected to host successfully!!! :)\n";
  }
}



void ClientSocket::sendData(unsigned char data_to_send[], int char_count){

  int send_status = send(client_sock, data_to_send, char_count, NO_FLAGS);

  if(send_status == -1) {
    error("ERROR: Could not send Data");
  }
}

unsigned char* ClientSocket::receiveData(unsigned char buffer[], int char_count, int index_offset){
  Environment env;
  unsigned char* data_received;

  int recv_status = recv(client_sock, buffer + index_offset, char_count, NO_FLAGS);

  if(recv_status == -1) {
    error("ERROR: Could not receive data");
  }
  else if(recv_status == 0) {
    error("Remote host has no more data to send");
  }
  else {
    data_received = buffer;
    return data_received;
  }

}

void ClientSocket::sendHTTPGET(string input, string host){//sends the HTTP GET request

  int send_status = send(client_sock, input.c_str(), sizeof(input), NO_FLAGS);

  if(send_status == -1) {
    error("ERROR: Could not send GET request");
  }
}


void ClientSocket::negotiate(unsigned char input[]){ //negotiates options with remote host --see README Section 4.4
  int send_status;
  ClientSocket s;
  unsigned char variable_option = input[2];

  if(input[0] == IAC && input[1] == DO && input[2] == NAWS) {
    unsigned char tmpBuffer[3] = {IAC, WILL, NAWS};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate window size.");
    }
    else{
      unsigned char cmdBuffer[9] = {IAC, SB, NAWS, IS, 80, IS, 24, IAC, SE};
      send_status = send(client_sock, cmdBuffer, sizeof(cmdBuffer), NO_FLAGS);
      if(send_status == -1){
        error("ERROR: Could not negotiate window size.");
      }
    }
  }
  else if(input[0] == IAC && input[1] == WILL && input[2] == ECHO) {
    unsigned char tmpBuffer[3] = {IAC, WILL, ECHO};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate ECHO.");
    }
  }
  else if(input[0] == IAC && input[1] == WILL && input[2] == SUPPRESS_GO_AHEAD) {
    unsigned char tmpBuffer[3] = {IAC, WILL, SUPPRESS_GO_AHEAD};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate SUPPRESS_GO_AHEAD.");
    }
  }
  else if(input[0] == IAC && input[1] == DO && input[2] == TERMINAL_TYPE) {
    unsigned char tmpBuffer[3] = {IAC, WILL, TERMINAL_TYPE};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate TTYPE.");
    }
    else{
    	unsigned char initBuffer[11] = {IAC, SB, TERMINAL_TYPE, IS, 'v', 't', 1, 0, 0, IAC, SE};
    	send_status = send(client_sock, initBuffer, sizeof(initBuffer), NO_FLAGS);
    	if(send_status == -1){
    	  error("ERROR: Could not negotiate TERMINAL_TYPE.");
    	}
    }
  }
  else if(input[0] == IAC && input[1] == DO) {
    unsigned char tmpBuffer[3] = {IAC, WONT, variable_option};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate.");
    }
  }
  else if(input[0] == IAC && input[1] == WILL) {
    unsigned char tmpBuffer[3] = {IAC, DONT, variable_option};
    send_status = send(client_sock, tmpBuffer, sizeof(tmpBuffer), NO_FLAGS);
    if(send_status == -1){
      error("ERROR: Could not negotiate.");
    }
  }
}

int main(int argc, char* argv[]) {
    ClientSocket socket;
    Environment environment;
    unsigned char buffer[MAX_BUFFER_SIZE];
    bool closed = false;
    struct timespec timeout;//used timespec as I want the timeout to be constant, not changed as in struct timeval and timespec is used by pselect
    timeout.tv_sec = 10;//waits up to 10secs to see if any data becomes available
    timeout.tv_nsec = 0;
    bool sync_io;
    
    if(argc != 3){
        error("Usage: ./telnet {IP Address} {Port Number}");
    }
    

    hostent* server = gethostbyname(argv[1]);
    short port = atoi(argv[2]);

    if(server == NULL){
        error("The specified host does not exist!");
    }

    socket.createSocket(server, port);

    if(port == HTTP_PORT) {
      HTTP request;
      cout << "enter http path: ";
      string uri;
      cin >> uri;
      string get;
      get = request.HTTPGet(uri, (char*) argv[1]);
      socket.sendHTTPGET(get, argv[1]);
      unsigned char * smth = socket.receiveData(buffer, MAX_BUFFER_SIZE, 0);
      smth[MAX_BUFFER_SIZE] = '\0';
      cout << smth << endl;
    }
    else{//if not HTTP -> TELNET
    //if(port == 23) {
      environment.makeTemporaryTerminal();
    //}
    while(closed == false) {
        fd_set sync;
        FD_ZERO(&sync);
        FD_SET(socket.getSocket(), &sync);//adds to set file descriptor as socket to check if data has to be received
        FD_SET(0, &sync);//adds to set file descriptor to 0 (stdin)
        sync_io = socket.checkForData(sync, timeout);
        
        if(sync_io == true) {
            if(socket.getSocket() != 0 && FD_ISSET(socket.getSocket(), &sync)) {
                unsigned char* recv_string;
                recv_string = socket.receiveData(buffer, 1, 0);
                if(buffer[0] == IAC) {
                    socket.receiveData(buffer, 2, 1);
                    socket.negotiate(buffer);
                }
                else if(sizeof(buffer) == 0) {
                    closed = true;
                }
                else{
                    buffer[1] = '\0';//because working with unsigned chars we need to null-terminate a char[] before printing
                    cout << buffer;
                    FD_CLR(socket.getSocket(), &sync);
                    cout << std::flush;
                }
            }
            else if(FD_ISSET(0, &sync)){//stdin ready
                buffer[0] = getc(stdin);//gets one character from stdin
                socket.sendData(buffer, 1);
            }
        }
    } 
    close(socket.getSocket()); //closes connection
    }
    
    return 0;
}
