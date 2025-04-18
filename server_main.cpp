#include"server.hpp"
int main()
{
    cout << "Please edit admin`s password" << endl;
    cin >> ADMIN_PASSWORD;
    Server server;
    server.run_server();
    return 0;
}