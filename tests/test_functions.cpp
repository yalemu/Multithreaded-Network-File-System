#include <iostream>
#include <string>
#include <cassert>
#include <arpa/inet.h>

using namespace std;

#define private public
#include "Shell.h"
#undef private

// Rename main in server.cpp to avoid conflicts
#define main server_main
#include "server.cpp"
#undef main

bool test_server_parse_command_valid() {
    Command c = parse_command("mkdir foo");
    assert(c.name == "mkdir");
    assert(c.file_name == "foo");
    assert(c.append_data == "");

    c = parse_command("append file data");
    assert(c.name == "append");
    assert(c.file_name == "file");
    assert(c.append_data == "data");

    c = parse_command("head file 10");
    assert(c.name == "head");
    assert(c.file_name == "file");
    assert(c.append_data == "10");

    c = parse_command("ls");
    assert(c.name == "ls");
    assert(c.file_name == "");
    assert(c.append_data == "");

    return true;
}

bool test_server_parse_command_invalid() {
    Command c = parse_command("ls extra");
    assert(c.name == "");

    c = parse_command("unknown command");
    assert(c.name == "");

    c = parse_command("");
    assert(c.name == "");

    return true;
}

bool test_get_server_addr() {
    int port = 12345;
    sockaddr_in addr = get_server_addr(port);
    assert(addr.sin_family == PF_INET);
    assert(addr.sin_port == htons(port));
    assert(addr.sin_addr.s_addr == htonl(INADDR_ANY));
    return true;
}

bool test_shell_parse_command() {
    Shell shell;
    Shell::Command cmd = shell.parse_command("create file");
    assert(cmd.name == "create");
    assert(cmd.file_name == "file");
    assert(cmd.append_data == "");

    cmd = shell.parse_command("append file data");
    assert(cmd.name == "append");
    assert(cmd.file_name == "file");
    assert(cmd.append_data == "data");

    cmd = shell.parse_command("quit");
    assert(cmd.name == "quit");
    return true;
}

int main() {
    test_server_parse_command_valid();
    test_server_parse_command_invalid();
    test_get_server_addr();
    test_shell_parse_command();
    std::cout << "All tests passed" << std::endl;
    return 0;
}

