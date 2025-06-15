#include"server.hpp"
std::string ADMIN_PASSWORD;

void Server::safe_print(const string &message) {
    lock_guard<mutex> guard(cout_mutex);
    cout << "[SERVER] " << message << endl;
}

void Server::send_to_client(SOCKET socket, const string &message) {
    send(socket, message.c_str(), (int)message.size(), 0);
}

void Server::broadcast_message(const string &sender, const string &message) {
    lock_guard<mutex> guard(clients_mutex);
    for (auto &pair : clients) {
        if (pair.first != sender) {
            (*this)(pair.second.socket, message);
        }
    }
}

void Server::disconnect_client(const string &client_name) {
    lock_guard<mutex> guard(clients_mutex);
    if (clients.count(client_name)) {
        closesocket(clients[client_name].socket);
        clients.erase(client_name);
        safe_print(client_name + " was disconnected by admin");

        if (active_admins.count(client_name)) {
            delete active_admins[client_name];
            active_admins.erase(client_name);
        }
    }
}

void Server::handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    bool is_admin = false;
    string client_name;

    try {
        // Выбор режима подключения
        string mode_prompt = "Choose mode:\n1. Regular client\n2. Admin\nEnter choice: ";
        (*this)(client_socket, mode_prompt);

        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
            throw runtime_error("Mode selection failed");
        string choice(buffer, bytes_received);

        // Режим администратора
        if (choice == "2") {
            string auth_prompt = "Enter admin password: ";
            (*this)(client_socket, auth_prompt);

            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
                throw runtime_error("Password receive failed");

            string password(buffer, bytes_received);
            if (!Admin::Authenticate(password))
            {
                (*this)(client_socket, "SERVER: Invalid admin password");
                throw runtime_error("Invalid admin password");
            }
            is_admin = true;
            (*this)(client_socket, "SERVER: Admin authentication successful");
        }

        // Получение имени
        string name_prompt = "Enter your name: ";
        (*this)(client_socket, name_prompt);

        bool name_accepted = false;
        while (!name_accepted) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
                throw runtime_error("Name receive failed");
            
            client_name = string(buffer, bytes_received);

            // Проверка на пустое имя
            if (client_name.empty()) {
                (*this)(client_socket, "SERVER: Name cannot be empty. Please enter your name again: ");
                continue;
            }

            // Проверка на недопустимые символы в имени
            if (client_name.find(':') != string::npos || client_name.find(' ') != string::npos) {
                (*this)(client_socket, "SERVER: Name cannot contain spaces or colons. Please enter another name: ");
                continue;
            }

            // Проверка уникальности имени
            lock_guard<mutex> guard(clients_mutex);
            if (clients.count(client_name)) {
                (*this)(client_socket, "SERVER: Name already in use. Please enter another name: ");
                continue;
            }

            // Если все проверки пройдены
            name_accepted = true;
            
            if (is_admin) {
                active_admins[client_name] = new Admin(client_socket, client_name);
                clients[client_name] = *active_admins[client_name];
            } else {
                clients[client_name] = Client(client_socket, client_name);
            }
        }

        safe_print(client_name + (is_admin ? " (admin)" : "") + " connected");
        (*this)(client_socket, "SERVER: Welcome, " + client_name + (is_admin ? " (admin)" : ""));

        // Основной цикл обработки сообщений
        while (true) {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
                break;
            
            if (clients[client_name].is_muted) {
                (*this)(client_socket, "SERVER: You are muted and cannot send messages");
                continue;
            }

            string msg_str(buffer, bytes_received);

            if (msg_str.empty() || msg_str == ":") {
                continue;
            }

            size_t delim_pos = msg_str.find(':');
            if (delim_pos == string::npos) {
                (*this)(client_socket, "SERVER: Invalid message format");
                continue;
            }

            MessageType type = static_cast<MessageType>(stoi(msg_str.substr(0, delim_pos)));
            string content = msg_str.substr(delim_pos + 1);
            cout << content << " Was sent by " << client_name << endl;

            if (type != ADMIN_COMMAND && clients[client_name].is_muted) {
                (*this)(client_socket, "SERVER: You are muted and cannot send messages");
                continue;
            }

            if (type == BROADCAST) {
                string full_msg = client_name + ": " + content;
                safe_print(client_name + " sent broadcast message");
                broadcast_message(client_name, full_msg);
            } else if (type == PRIVATE) {
                size_t first_colon = content.find(':');
                if (first_colon == string::npos) {
                    (*this)(client_socket, "SERVER: Invalid private message format");
                    continue;
                }

                string recipient = content.substr(0, first_colon);
                string private_msg = content.substr(first_colon + 1);

                lock_guard<mutex> guard(clients_mutex);
                if (clients.count(recipient)) {
                    string full_msg = "[Private from " + client_name + "]: " + private_msg;
                    (*this)(clients[recipient].socket, full_msg);
                    (*this)(client_socket, "SERVER: Private message sent to " + recipient);
                } else {
                    (*this)(client_socket, "SERVER: User " + recipient + " not found");
                }
            } else if (type == ADMIN_COMMAND && is_admin) {
                if (content.substr(0, 11) == "/disconnect") {
                    string target = content.substr(12);
                    if (target.empty()) {
                        (*this)(client_socket, "SERVER: Usage: /disconnect <username>");
                        continue;
                    }

                    if (target == client_name) {
                        (*this)(client_socket, "SERVER: You cannot disconnect yourself");
                        continue;   
                    }

                    (*this) -= target;
                    (*this)(client_socket, "SERVER: User " + target + " has been disconnected");
                } else {
                    (*this)(client_socket, "SERVER: Unknown admin command");
                }
            } else if(type == MUTE && is_admin) {
                if (content.substr(0,5) == "/mute"){
                    string target = content.substr(6);
                    if (clients.count(target)) {
                        clients[target].is_muted = true;
                        (*this)(client_socket, "SERVER: User " + target + " has been muted");
                    } else {
                        (*this)(client_socket, "SERVER: User " + target + " not found");
                    }
                }
            } else if(type == DEMUTE && is_admin) {
                if (content.substr(0,7) == "/demute") {
                    string target = content.substr(8);
                    if (clients.count(target)) {
                        clients[target].is_muted = false;
                        (*this)(client_socket, "SERVER: User " + target + " has been unmuted");
                    } else {
                        (*this)(client_socket, "SERVER: User " + target + " not found");
                    }
                }
            } else if (type == COMMAND) {

                string client = "\n";
                int c = 1;
                for (auto it : clients) {
                    client += to_string(c) + ". " + (it.first) + '\n';
                    ++c;
                }
                client.pop_back();

                (*this)(client_socket, client);
            } else {
                (*this)(client_socket, "SERVER: Unknown command type");
            }
        }
    } catch (const exception &e) {
        safe_print("Error with client " + client_name + ": " + e.what());
    }

    // Завершение соединения
    {
        lock_guard<mutex> guard(clients_mutex);
        if (!client_name.empty()) {
            clients.erase(client_name);
            if (is_admin && active_admins.count(client_name)) {
                delete active_admins[client_name];
                active_admins.erase(client_name);
            }
        }
    }
    closesocket(client_socket);
    safe_print((client_name.empty() ? "Unknown client" : client_name) + " disconnected");
}


void Server::run_server() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        safe_print("WSAStartup failed");
        return;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        safe_print("Socket creation failed");
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(DEFAULT_PORT);

    if (bind(server_socket, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        safe_print("Bind failed");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        safe_print("Listen failed");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    safe_print("Server started on port " + to_string(DEFAULT_PORT));
    safe_print("Waiting for connections...");

    while (true) {
        sockaddr_in client_addr;
        int client_addr_size = sizeof(client_addr);

        SOCKET client_socket = accept(server_socket, (sockaddr *)&client_addr, &client_addr_size);
        if (client_socket == INVALID_SOCKET) {
            safe_print("Accept failed");
            continue;
        }

        thread([this, client_socket]() {
            this->handle_client(client_socket);
        }).detach();
    }

    closesocket(server_socket);
    WSACleanup();
}

void Server::operator-=(const string& client_name) {
    disconnect_client(client_name);
}

void Server::operator()(SOCKET socket, const string& message) {
    send_to_client(socket, message);
}

