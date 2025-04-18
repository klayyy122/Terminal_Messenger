#include "client.hpp"
string ADMIN_PASSWORD = "admin123";
const int ENCRYPTION_KEY = 37;
string Clients::getTime(){
    time_t now = time(0);
            string dt = ctime(&now);
            dt[dt.length()-1] = '\0';
    
    string data = "[" + dt + "]";
    return data;
}
void Clients::safe_print(const string &message)
{
    lock_guard<mutex> guard(cout_mutex);
    
    cout << getTime() <<  message << endl;
}

void Clients::show_help(bool is_admin = false)
{
    cout << "\nCommands:\n"
         << "/help - Show this help\n"
         << "/list - Show online users\n";
    if (is_admin)
    {
        cout << "/disconnect <name> - Disconnect client (admin only)\n";
    }
    cout << "/private <name> <message> - Send private message\n"
         << "/exit - Quit chat\n"
         << "Normal message - Send to all\n"
         << endl;
}

void Clients::run_client()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        safe_print("WSAStartup failed");
        return;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET)
    {
        safe_print("Socket creation failed");
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP_DEFAULT);

    if (connect(client_socket, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        safe_print("Connection failed");
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    // Выбор режима подключения
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0)
    {
        closesocket(client_socket);
        WSACleanup();
        return;
    }
    cout << string(buffer, bytes_received);

    string choice;
    getline(cin, choice);
    send(client_socket, choice.c_str(), (int)choice.size(), 0);

    bool is_admin = false;
    if (choice == "2")
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        cout << string(buffer, bytes_received);

        string password;
        getline(cin, password);
        send(client_socket, password.c_str(), (int)password.size(), 0);

        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        string response(buffer, bytes_received);
        if (response.find("Invalid") != string::npos)
        {
            safe_print(response);
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        is_admin = true;
    }

    // Ввод имени
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0)
    {
        closesocket(client_socket);
        WSACleanup();
        return;
    }
    cout << string(buffer, bytes_received);

    string name;
    getline(cin, name);
    send(client_socket, name.c_str(), (int)name.size(), 0);

    // Получение приветствия
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0)
    {
        safe_print(string(buffer, bytes_received));
    }

    show_help(is_admin);

    thread receiver([client_socket, this]()
                    {
        char buffer[BUFFER_SIZE];
        while (true) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                safe_print("Connection to server lost");
                exit(0);
            }
            string message(buffer, bytes_received);

            lock_guard<mutex> guard(cout_mutex);
            if (message.find("[Private from") == 0) {
                size_t msg_start = message.find("]: ") + 3;
                if (msg_start < message.size()) {
                    string encrypted = message.substr(msg_start);
                    string decrypted = DeFunction(encrypted, ENCRYPTION_KEY);
                    cout << getTime() << ' ' << message.substr(0, msg_start) << decrypted << endl;
                }
            }
            else if (message.find("SERVER:") == 0) {
                cout << getTime() << ' ' <<  message << endl;
            }
            else {
                size_t colon_pos = message.find(':');
                if (colon_pos != string::npos && colon_pos < message.length() - 2) {
                    string sender = message.substr(0, colon_pos);
                    string encrypted_content = message.substr(colon_pos + 2);
                    string decrypted = DeFunction(encrypted_content, ENCRYPTION_KEY);
                    cout << getTime() << ' ' << sender << ": " << decrypted << endl;
                }
                else {
                    cout << getTime() << ' '<<  message << endl;
                }
            }
            cout << "> " << flush;
        } });

    string input;
    while (true)
    {
        cout << "> ";
        getline(cin, input);
        
        last_message = input;
        (*this) > name;
        if (input.empty())
            continue;

        if (input == "/exit")
        {
            break;
        }
        else if (input == "/help")
        {
            show_help(is_admin);
            continue;
        }
        else if (input.substr(0, 8) == "/private")
        {
            size_t space_pos = input.find(' ', 8);
            if (space_pos == string::npos)
            {
                safe_print("Usage: /private <name> <message>");
                continue;
            }

            size_t msg_pos = input.find(' ', space_pos + 1);
            if (msg_pos == string::npos)
            {
                safe_print("Usage: /private <name> <message>");
                continue;
            }

            string recipient = input.substr(space_pos + 1, msg_pos - space_pos - 1);
            string message = input.substr(msg_pos + 1);

            if (message.empty())
            {
                safe_print("Message cannot be empty");
                continue;
            }

            string encrypted = Function(message, ENCRYPTION_KEY);
            string cmd = to_string(PRIVATE) + ":" + recipient + ":" + encrypted;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
        else if (input == "/list")
        {
            string list_cmd = to_string(COMMAND) + ":list";
            send(client_socket, list_cmd.c_str(), (int)list_cmd.size(), 0);
        }
        else if (is_admin && input.substr(0, 11) == "/disconnect")
        {
            string target = input.substr(12);
            if (target.empty())
            {
                safe_print("Usage: /disconnect <name>");
                continue;
            }
            string cmd = to_string(ADMIN_COMMAND) + ":/disconnect " + target;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
        else if(is_admin && input.substr(0, 5) == "/mute")
        {
            string target = input.substr(6);
            string cmd = to_string(MUTE) + ":/mute " + target;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
        else
        {
            string encrypted = Function(input, ENCRYPTION_KEY);
            string cmd = to_string(BROADCAST) + ":" + encrypted;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
    }

    closesocket(client_socket);
    receiver.detach();
    WSACleanup();
}
void Clients::operator >(const string& name) 
{
    string filename = "logs_" + name + ".txt";
    ofstream out;
    out.open(filename);
    if(out.is_open())
    {
        out << last_message << endl;
    }
    out.close();
}
