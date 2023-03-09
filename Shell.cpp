// CPSC 3500: Shell
// Implements a basic shell (command line interface) for the file system

#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cstring>
#include <vector>

using namespace std;

#include "Shell.h"

static const string PROMPT_STRING = "NFS> ";	// shell prompt


// Mount the network file system with server name and port number in the format of server:port
void Shell::mountNFS(string fs_loc) {
   // create the socket cs_sock and connect it to the server and port specified in fs_loc
  // if all the above operations are completed successfully, set is_mounted to true

  
  string hostname;
  string port;
  string junk;

  size_t curr = 0;
  size_t delimit_idx;
 
  istringstream ss(fs_loc);
  string token;
  if (getline(ss, token, ':'))
  {
    hostname = token;
    if (getline(ss, token))
    {
      port = token;
    }
  }

  addrinfo *addr, hints;
  bzero(&hints, sizeof(hints));
  hints.ai_family = PF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = PF_UNSPEC;

  int ret;
  if ((ret = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &addr)) != 0)
  {
    cout << "Error: Could not obtain address information for \"" << hostname << ":" << port << "\"" << endl;
    cout << "\tgetaddrinfo returned with " << ret << endl;
    exit(1);
  }
  // create socket to connect
  this->cs_sock = socket(PF_INET, SOCK_STREAM, PF_UNSPEC);

  if (cs_sock < 0)
  {
    cout << "Error: Failed to create a socket" << endl;
    cout << "\tsocket returned with " << cs_sock << endl;
    exit(1);
  }

  // connect to server
  int connect_ret = connect(cs_sock, addr->ai_addr, addr->ai_addrlen);
  if (connect_ret < 0)
  {
    cout << "Error: Failed to connect with server (" << hostname << ":" << port
         << ")." << endl;
    cout << "\tDouble check that the server is running." << endl;
    exit(1);
  }

  // Free the linked list created by getaddrinfo
  freeaddrinfo(addr);

  is_mounted = true;
	//create the socket cs_sock and connect it to the server and port specified in fs_loc
	//if all the above operations are completed successfully, set is_mounted to true  
}

// Unmount the network file system if it was mounted
void Shell::unmountNFS() {
	// close the socket if it was mounted
    if (is_mounted) {
        close(cs_sock);
        is_mounted = false;
    }
}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname) {
  // to implement
  string command = "mkdir " + dname + "\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on cd
void Shell::cd_rpc(string dname) {
  // to implement
  string command = "cd " + dname + "\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on home
void Shell::home_rpc() {
  // to implement
  string command = "home\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname) {
  // to implement
  string command = "rmdir " + dname + "\n\r";
  cmmdprint(command, false);
} 

// Remote procedure call on ls
void Shell::ls_rpc() {
  // to implement
  string command = "ls\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on create
void Shell::create_rpc(string fname) {
  // to implement
  string command = "create " + fname + "\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data) {
  // to implement
  string command = "append " + fname + " " + data + "\n\r";
  cmmdprint(command, false);
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname) {
  // to implement
  string command = "cat " + fname + "\n\r";
  cmmdprint(command, true);
}

// Remote procedure call on head
void Shell::head_rpc(string fname, int n) {
  // to implement
  string command = "head " + fname + " " + to_string(n) + "\n\r";
  cmmdprint(command, true);
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname) {
  // to implement
  string command = "rm " + fname + "\n\r";
  cmmdprint(command, false);
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname) {
  // to implement
  string command = "stat " + fname + "\n\r";
  cmmdprint(command, false);
}


// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT_STRING;
    getline(cin, command_str);

    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }


  // execute each line in the script
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT_STRING << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}

void Shell::send_command(string message)
{
  // Format message for network transit
  string formatted_message = message + "\r\n";
/*
  // Send command over the network (through the provided socket)
  if (send(cs_sock, formatted_message.c_str(), formatted_message.length(), 0) == -1) {
    perror("Error sending message");
    exit(1);
  }
*/
  for(int i = 0; i < formatted_message.length(); i++) {
    char p = formatted_message[i];
    int bytes_sent = 0;
    int x;
    while(bytes_sent < sizeof(char))  {
      x = send(cs_sock, (void*) &p, sizeof(char), 0);
      if(x == -1) {
        perror("write");
        close(cs_sock);
        exit(1);
      }
      bytes_sent += x;
    }
  }
}

string Shell::receive_response()
{
  //int BUFFER_SIZE = 1024;
  vector<char> response_buffer;//
  char tempBuf;
  char lastBuf;
  int count = 0;
  bool isFinished = false;
  while(!isFinished)  {
    int bytesRec = 0;
    while(bytesRec < sizeof(char)) {
      int x = recv(cs_sock, (void*)&tempBuf, sizeof(char), 0);
      if(x == -1) {
        perror("read");
        close(cs_sock);
        exit(1);
      }
      bytesRec += x;
    }
    if(tempBuf == '\n'  && lastBuf == '\r')  {
      isFinished = true;
    }
    response_buffer.push_back(tempBuf);
//    count++;
    lastBuf = tempBuf;

  }
    return string(response_buffer.begin(), response_buffer.end());
    
  


  /*
  //cout << "hello" << endl;
  memset(response_buffer,'\0',sizeof(response_buffer));
  int bytes_received = read(cs_sock, response_buffer, BUFFER_SIZE); // is a blockig system call not going past here so goes on deadlock 
  cout << response_buffer << endl;
  
  memset(response_buffer,'\0',sizeof(response_buffer));

  //cout << " tf u want ";
  int bytes = read(cs_sock, response_buffer, BUFFER_SIZE);
  //cout << " gifter \n";
  cout << response_buffer << endl;
  //cout << "orint";
 
  //int bytes1 = read(cs_sock, response_buffer, BUFFER_SIZE - 1);
  //cout << response_buffer << endl;
  memset(response_buffer,'\0',sizeof(response_buffer));

  int bytes2 = read(cs_sock, response_buffer, BUFFER_SIZE);
  cout << response_buffer << endl;
  //cout << "print after ";
*/

}

string Shell::receiveDataresponse(int size)
{
  //int BUFFER_SIZE = 1024;
  vector<char> response_buffer;//
  char tempBuf;
  char lastBuf;
  int count = 0;
  while(count < size)  {
    int bytesRec = 0;
    while(bytesRec < sizeof(char)) {
      int x = recv(cs_sock, (void*)&tempBuf, sizeof(char), 0);
      if(x == -1) {
        perror("read");
        close(cs_sock);
        exit(1);
      }
      bytesRec += x;
    }

    response_buffer.push_back(tempBuf);
    count++;
    lastBuf = tempBuf;

  }
  return string(response_buffer.begin(), response_buffer.end());
    
}

void Shell::cmmdprint(string message, bool can_be_empty)
{
  string temp = "";
  int size = 0;
  send_command(message);
  
  temp = receive_response();
  cout << temp;
  
  temp = receive_response();
  
  size_t colonPos = temp.find(':'); // find the position of the colon
  size_t crPos = temp.find('\r'); // find the position of the carriage return

  if (colonPos != std::string::npos && crPos != std::string::npos) {
      // extract the substring between the colon and carriage return
      size = stoi(temp.substr(colonPos + 1, crPos - colonPos - 1));
  }
  temp = receive_response();
  temp = receiveDataresponse(size);
  cout << temp << endl;
}

