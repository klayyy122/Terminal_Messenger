#pragma once
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <ctime>
#include<fstream>
#include <cmath>
using namespace std;
const int DEFAULT_PORT = 1111;
const int BUFFER_SIZE = 1024;
const string COLOR_RED = "\033[31m";
const string COLOR_RESET = "\033[0m";
extern std::string ADMIN_PASSWORD;
enum MessageType {
    BROADCAST,
    PRIVATE,
    COMMAND,
    ADMIN_COMMAND,
    MUTE,
    DEMUTE
};

class Client {
public:
    SOCKET socket;
    string name;
    bool is_muted = false;
    Client(SOCKET sock = INVALID_SOCKET, const string& n = "") : socket(sock), name(n) {}
};

class Admin : public Client {
public:
    Admin(SOCKET sock = INVALID_SOCKET, const string& n = "") : Client(sock, n) {}

    static bool Authenticate(const string& password) {
        return password == ADMIN_PASSWORD;
    }
};

class Server final {
private:
    mutex cout_mutex;
    
    mutex clients_mutex;
    map<string, Client> clients;
    map<string, Admin *> active_admins;

    void safe_print(const string &message);

    void send_to_client(SOCKET socket, const string &message);

    void broadcast_message(const string &sender, const string &message);

    void disconnect_client(const string &client_name);

    void handle_client(SOCKET client_socket);
    void operator -=(const string& client_name);
    void operator()(SOCKET socket, const string& message);
    

public:
    void run_server();
};