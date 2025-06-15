To compile project use command "make". 
server.exe & client.exe files will be in ./bin. 
all .o files, maked after compiling will be in ./build 

Start server with "./bin/server"
Start client with "./bin/client"

To compile server use this: g++ server.cpp server_main.cpp -o server -lws2_32 -Iinclude

To compile client use this: g++ client.cpp client_main.cpp -o client -lws2_32 -Iinclude

