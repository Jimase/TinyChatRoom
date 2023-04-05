# Linux Terminal Chat Room Demo

1. Navigate to the `src/Linux` folder and run the `make` command to compile the server and client programs, which will generate the executable files `Server` and `Client`, respectively.
2. To start the server, enter `./Server` in the terminal. You will see the following prompt:
```
Please enter the port number of the server: 
```
Enter the port number, for example, `8888`, and you will see the following message:
```
Server start successfully!
You can join the chatroom by connecting to 127.0.0.1:8888
```
This means that the server is up and running and is listening on `TCP` connections at `127.0.0.1:8888`.

3. Open another terminal window and run the client program by entering `./Client`. The program will prompt you to enter the server's IP address and port number, for example:
```
Please enter IP address of the server: 127.0.0.1
Please enter port number of the server: 8888
Connect Successfully!
Welcome to Use TinyChatroom!
```
Once the handshake is successful, the program will prompt you to enter a username:
```
Please enter your name: Jimase
Hello Jimase, Welcome to join the chatroom. Online User Number: 1
```
After confirmation, the server will print the following message:
```
Jimase join in the chatroom. Online User Number: 1
```
This means that a new user has joined. To disconnect from the chatroom, you can press `ctrl+c` or simply close the terminal window. The server will print the following message:
```
Jimase left the chatroom. Online Person Number: 0
```
This means that the user has left the chatroom.
