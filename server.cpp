#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sstream>
#include <vector>

#define BUFFER_SIZE 1024

#include "FileSys.h"

using namespace std;

int client_sock;

struct Command
{
  string name;        // name of command
  string file_name;   // name of file
  string append_data; // append data (append only)
};

sockaddr_in get_server_addr(in_port_t port);
Command parse_command(string command_str);
bool execute_command(string command_str);
bool recCmd(string &cmdStr, int sock);
FileSys fs;

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cout << "Usage: ./nfsserver port#\n";
    return -1;
  }
  int port = atoi(argv[1]);

  // networking part: create the socket and accept the client connection
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    cout << "ERROR: Failed to open socket.\n";
    return -1;
  }

  sockaddr_in client_addr;
  socklen_t client_address_len = sizeof(client_addr);

  struct sockaddr_in server_addr = get_server_addr(port);
  if (bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Error binding listening socket");
    close(sock);
    exit(1);
  }

  if (listen(sock, 5) < 0)
  {
    perror("Error listening for connections");
    close(sock);
    exit(1);
  }

  cout << " server is listening ";

  /*
    if (accept(listen_sock, (sockaddr*) &client_addr, &addr_len) < 0) {
      cout << "Error accepting connection" << endl;
      close(listen_sock);
      return;
    }
  */
  bool user_quit = false;
  string buffer;
  // mount the file system
  while (true)
  {
    //memset(buffer,'\0',sizeof(buffer));
    // assume that sock is the new socket created
    // for a TCP connection between the client and the server.

    // loop: get the command from the client and invoke the file
    // system operation which returns the results or error messages back to the clinet
    // until the client closes the TCP connection.

    // Accept a new connection
    client_sock = accept(sock, (sockaddr *)&client_addr, &client_address_len);
    if (client_sock < 0)
    {
      perror("Error accepting connection");
      break;
    }
    fs.mount(client_sock);
    while (recCmd(buffer,client_sock))
    {
      //read 
      // maxBufferSize
    

      
      //cout << "test" << endl;
      user_quit = execute_command(buffer);
     

      // close the listening socket
    }
    close(client_sock);
  
    // unmout the file system
  }

  close(sock);

  fs.unmount();

  return 0;
  
}
bool recCmd(string &cmdStr, int sock) {
    //int BUFFER_SIZE = 1024;
  vector<char> response_buffer;//
  char tempBuf;
  char lastBuf;
  int count = 0;
  bool isFinished = false;
  cout << " print 1 \n";
  while(!isFinished)  {
    int bytesRec = 0;
    cout << "print 2 \n";
    while(bytesRec < sizeof(char)) {
      cout << "print 3 \n";
      int x = recv(sock, (void*)&tempBuf, sizeof(char), 0);
      
      if (x == 0){
        return false;
        
      }
      if(x == -1) {
        perror("read");
        close(sock);
        exit(1);
      }
      bytesRec += x;
    }
    if(tempBuf == '\n'  && lastBuf == '\r')  {
      isFinished = true;
    }
    response_buffer.push_back(tempBuf);
    //count++;
    lastBuf = tempBuf;

   }
    cmdStr = string(response_buffer.begin(), response_buffer.end());
    return true;
 
}
bool execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);
  string str = command.file_name;
  const char *cstr = str.c_str();

  // look for the matching command
  if (command.name == "")
  {
    return false;
  }
  else if (command.name == "mkdir")
  {

    fs.mkdir(cstr);
  }
  else if (command.name == "cd")
  {
    fs.cd(cstr);
  }
  else if (command.name == "home")
  {
    fs.home();
  }
  else if (command.name == "rmdir")
  {
    fs.rmdir(cstr);
  }
  else if (command.name == "ls")
  {
    fs.ls();
  }
  else if (command.name == "create")
  {
    fs.create(cstr);
  }
  else if (command.name == "append")
  {
    fs.append(cstr, command.append_data.c_str());
  }
  else if (command.name == "cat")
  {
    fs.cat(cstr);
  }
  else if (command.name == "head")
  {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno)
    {
      fs.head(cstr, n);
    }
    else
    {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm")
  {
    fs.rm(cstr);
  }
  else if (command.name == "stat")
  {
    fs.stat(cstr);
  }
  else if (command.name == "quit")
  {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Command parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name)
  {
    num_tokens++;
    if (ss >> command.file_name)
    {
      num_tokens++;
      if (ss >> command.append_data)
      {
        num_tokens++;
        string junk;
        if (ss >> junk)
        {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0)
  {
    return empty;
  }

  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1)
    {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
           command.name == "cd" ||
           command.name == "rmdir" ||
           command.name == "create" ||
           command.name == "cat" ||
           command.name == "rm" ||
           command.name == "stat")
  {
    if (num_tokens != 2)
    {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3)
    {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else
  {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl;
    return empty;
  }

  return command;
}

sockaddr_in get_server_addr(in_port_t port)
{
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = PF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  return server_addr;
}
