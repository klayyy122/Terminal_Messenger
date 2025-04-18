#include "client.hpp"

const char* IP_DEFAULT = "0.0.0.0";
int DEFAULT_PORT = 1111;
const string COLOR_RED = "\033[31m";
const string COLOR_RESET = "\033[0m";
string ADMIN_PASSWORD;
int ENCRYPTION_KEY;
int speed, frequency;

void clearCurrentLine() {
    std::cout << "\033[2K";  // очистить текущую строку
    std::cout << "\r";       // вернуть курсор в начало строки
}
void roll(int a = 0, int b = 100) {
    srand(time(0));
    for (int i = 0; i < 20; ++i) {
        clearCurrentLine();
        int tempRoll = a + rand() % b;
        std::cout << "roll: " << tempRoll;
        Sleep(200);
    }
    cout << endl;
    
}

void configes() {
    ifstream file("Port.json");
    if (!file.is_open()) {
        cerr << "Using default connection settings (config not found)" << endl;
    }
    else {
        try {
            nlohmann::json config;
            file >> config;

            string new_ip = config["ip"];
            int new_port = config["port"];
            const string COLOR_RED = config["COLOR_RED"];
            const string COLOR_RESET = config["COLOR_RESET"];
            frequency = config["frequency"];
            speed = config["speed"];

            static string persistent_ip = new_ip;
            static int persistent_port = new_port;

            IP_DEFAULT = persistent_ip.c_str();
            DEFAULT_PORT = persistent_port;

        }
        catch (const exception& e) {
            cerr << "Config error: " << e.what() << "\nUsing defaults" << endl;
        }
    }
}

void get_crypto_key() {
    cout << "Enter encryption key: ";
    string key_input;
    getline(cin, key_input);
    int temp = (atoi(key_input.c_str()) % 40) + 1;
    ENCRYPTION_KEY = temp;
}

string Clients::getTime() {
    time_t now = time(0);
    string dt = ctime(&now);
    dt[dt.length() - 1] = '\0';

    string data = "[" + dt + "]";
    return data;
}
void Clients::safe_print(const string& message) {
    lock_guard<mutex> guard(cout_mutex);

    cout << getTime() << message << endl;
}

void Clients::show_help(bool is_admin = false) {
    cout << "\nCommands:\n"
        << "/help - Show this help\n"
        << "/list - Show online users\n";
    if (is_admin) {
        cout << "/disconnect <name> - Disconnect client (admin only)\n";
        cout << "/mute <name> - mute the client (admit only)\n";
        cout << "/demute <name> - demute the client (admit only)\n";
    }
    cout << "/private <name> <message> - Send private message\n"
        << "/exit - Quit chat\n"
        << "/roll - random number from 0 to 100\n"
        << "Normal message - Send to all\n"
        << endl;
}

void Clients::run_client() {

    get_crypto_key();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        safe_print("WSAStartup failed");
        return;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        safe_print("Socket creation failed");
        WSACleanup();
        return;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DEFAULT_PORT);
    server_addr.sin_addr.s_addr = inet_addr(IP_DEFAULT);

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        safe_print("Connection failed");
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    // Выбор режима подключения
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        WSACleanup();
        return;
    }
    cout << string(buffer, bytes_received);

    string choice;
    getline(cin, choice);
    send(client_socket, choice.c_str(), (int)choice.size(), 0);

    bool is_admin = false;
    if (choice == "2") {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        cout << string(buffer, bytes_received);

        string password;
        getline(cin, password);
        send(client_socket, password.c_str(), (int)password.size(), 0);

        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        string response(buffer, bytes_received);
        if (response.find("Invalid") != string::npos) {
            safe_print(response);
            closesocket(client_socket);
            WSACleanup();
            return;
        }
        is_admin = true;
    }

    // Ввод имени
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        WSACleanup();
        return;
    }
    cout << string(buffer, bytes_received);
    string name;
    while (true) {
        getline(cin, name);
        send(client_socket, name.c_str(), (int)name.size(), 0);

        // Получение ответа о принятии имени
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            closesocket(client_socket);
            WSACleanup();
            return;
        }

        string response(buffer, bytes_received);

        // Если имя принято (нет ключевых слов об ошибке)
        if (response.find("already in use") == string::npos &&
            response.find("cannot be empty") == string::npos &&
            response.find("cannot contain") == string::npos) {
            break;
        }

        // Выводим сообщение об ошибке и продолжаем цикл
        safe_print(response);
        cout << "> " << flush;
    }

    show_help(is_admin);

    thread receiver([client_socket, this, name]() {
        char buffer[BUFFER_SIZE];
        while (true) {
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                safe_print("Connection to server lost");
                exit(0);
            }
            string message(buffer, bytes_received);

            Beep(frequency, speed);
            
            lock_guard<mutex> guard(cout_mutex);
            if (message.find("[Private from") == 0) {
                size_t msg_start = message.find("]: ") + 3;
                if (msg_start < message.size()) {
                    string encrypted = message.substr(msg_start);
                    string decrypted = DeFunction(encrypted, ENCRYPTION_KEY);
                    string full_message = message.substr(0, msg_start) + decrypted;
                    cout << getTime() << ' ' << full_message << endl;

                    last_message = full_message;
                    (*this) > name;
                }
            }
            else if (message.find("SERVER:") == 0) {
                cout << getTime() << ' ' << message << endl;
                last_message = message;
                (*this) > name;
            }
            else {
                size_t colon_pos = message.find(':');
                if (colon_pos != string::npos && colon_pos < message.length() - 2) {
                    string sender = message.substr(0, colon_pos);
                    string encrypted_content = message.substr(colon_pos + 2);
                    string decrypted = DeFunction(encrypted_content, ENCRYPTION_KEY);
                    bool is_admin = (sender.find("[ADMIN]") != string::npos);
                    string full_message;
                    if (is_admin) {
                        full_message = COLOR_RED + sender + COLOR_RESET + ": " + decrypted;
                    }
                    else {
                        full_message = sender + ": " + decrypted;
                    }
                    cout << getTime() << ' ' << full_message << endl;

                    last_message = full_message;
                    (*this) > name;
                }
                else {
                    cout << getTime() << ' ' << message << endl;
                    last_message = message;
                    (*this) > name;
                }
            }
            cout << "> " << flush;
        }
        });

    string input;
    while (true) {
        cout << "> ";
        getline(cin, input);

        if (input.empty()) {
            continue;
        }

        if (!input.empty() && input != "/help") {  // Не логируем пустые строки и команду /help
            last_message = name + ": " + input;    // Добавляем имя отправителя
            (*this) > name;
        }

        if (input == "/exit") {
            break;
        }
        else if (input == "/help") {
            show_help(is_admin);
            continue;
        }
        else if (input.substr(0, 8) == "/private") {
            size_t space_pos = input.find(' ', 8);
            if (space_pos == string::npos) {
                safe_print("Usage: /private <name> <message>");
                continue;
            }

            size_t msg_pos = input.find(' ', space_pos + 1);
            if (msg_pos == string::npos) {
                safe_print("Usage: /private <name> <message>");
                continue;
            }

            string recipient = input.substr(space_pos + 1, msg_pos - space_pos - 1);
            string message = input.substr(msg_pos + 1);

            if (message.empty()) {
                safe_print("Message cannot be empty");
                continue;
            }

            string encrypted = Function(message, ENCRYPTION_KEY);
            string cmd = to_string(PRIVATE) + ":" + recipient + ":" + encrypted;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
        else if (input == "/list") {

            string list_cmd = to_string(COMMAND) + ":list";
            send(client_socket, list_cmd.c_str(), (int)list_cmd.size(), 0);

        }
        else if (is_admin && input.substr(0, 11) == "/disconnect") {
            string target = input.substr(12);
            if (target.empty()) {
                safe_print("Usage: /disconnect <name>");
                continue;
            }
            string cmd = to_string(ADMIN_COMMAND) + ":/disconnect " + target;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        } else if(is_admin && input.substr(0, 5) == "/mute") {
            string target = input.substr(6);
            string cmd = to_string(MUTE) + ":/mute " + target;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        } else if(is_admin && input.substr(0, 7) == "/demute") {
            string target = input.substr(8);
            string cmd = to_string(DEMUTE) + ":/demute " + target;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        } else if(input == "/roll") {
            roll();
        } else {
            string encrypted = Function(input, ENCRYPTION_KEY);
            string cmd = to_string(BROADCAST) + ":" + encrypted;
            send(client_socket, cmd.c_str(), (int)cmd.size(), 0);
        }
    }

    closesocket(client_socket);
    receiver.detach();
    WSACleanup();
}
void Clients::operator >(const string& name) {
    string filename = "logs_" + name + ".txt";
    ofstream out;
    out.open(filename, ios::app);
    if (out.is_open()) {
        out << getTime() << " " << last_message << endl;
    }
    out.close();
}
