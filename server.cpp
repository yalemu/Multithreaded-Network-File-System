#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

#include "FileSys.h"
using namespace std;

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
        return;
    }

    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_address_len = sizeof(client_addr);

    bzero((char *)&server_addr, sizeof(server_addr));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(sock, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror( "Error binding listening socket");
        close(sock);
        exit(1);
    }

    if (listen(sock, 10) < 0)
    {
        perror("Error listening for connections");
        close(sock);
        exit(1);
    }

    /*
      if (accept(listen_sock, (sockaddr*) &client_addr, &addr_len) < 0) {
        cout << "Error accepting connection" << endl;
        close(listen_sock);
        return;
      }
    */

    // mount the file system
    FileSys fs;
    fs.mount(sock); // assume that sock is the new socket created
                    // for a TCP connection between the client and the server.

    // loop: get the command from the client and invoke the file
    // system operation which returns the results or error messages back to the clinet
    // until the client closes the TCP connection.

    while (true)
    {
        // Accept a new connection
        int client_sock = accept(sock, (sockaddr *)&client_addr, &client_address_len);
        if (client_sock < 0)
        {
            perror("Error accepting connection");
            continue;
        }
        string command;
        while (true)
        {
            // maxBufferSize
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            int x = read(sock, buffer, BUFFER_SIZE - 1);
            if (x <= 0)
            {
                cout << "client disconnected" << endl;
                break;
            }
            command = string(buffer);
            command = command.substr(0, command.find_first_of("\r\n"));

            if (command == "cd")
            {
                fs.cd(name);
            }
            else if (command == "home")
            {
                fs.home();
            }

            else if (command == "mkdir")
            {
                fs.mkdir(name);
            }

            else if (command == "rmdir")
            {
                fs.rmdir(name);
            }

            else if (command == "ls")
            {
                fs.ls();
            }

            else if (command == "create")
            {
                fs.create(name);
            }

            else if (command == "append")
            {
                fs.append(name); // needs more argument 
            }

            else if (command == "cat")
            {
                fs.cat(name);
            }

            else if (command == "head")
            {
                fs.head(name); //needs more argument 
            }

            else if (command == "rm")
            {
                fs.rm(name);
            }

            else if (command == "stat")
            {
                fs.stat(name);
            }
        }
    }

    // close the listening socket
    close(sock);

    // unmout the file system
    fs.unmount();
    return 0;
}
