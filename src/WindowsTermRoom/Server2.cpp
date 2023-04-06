#include <cstdio>
#include <cstring>
#include <queue>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define BUFFER_LEN 1024
#define NAME_LEN 20
#define MAX_CLIENT_NUM 32

struct Client {
    int valid;
    int fd_id;
    SOCKET socket;
    string name;
    queue<string> messages;
    condition_variable cv;
    mutex mtx;
};

unique_ptr<Client> clients[MAX_CLIENT_NUM];

int current_client_num = 0;
mutex num_mtx;

void handle_send(Client* client) {
    while (true) {
        unique_lock<mutex> lock(client->mtx);
        client->cv.wait(lock, [&]() { return !client->messages.empty(); });
        while (!client->messages.empty()) {
            string& message_buffer = client->messages.front();
            int n = message_buffer.length();
            int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            while (n > 0) {
                int len = send(client->socket, message_buffer.c_str(), trans_len, 0);
                if (len < 0) {
                    perror("send");
                    return;
                }
                n -= len;
                message_buffer.erase(0, len);
                trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
            }
            message_buffer.clear();
            client->messages.pop();
        }
    }
}

void handle_recv(Client* client) {
    string message_buffer;
    int message_len = 0;
    char buffer[BUFFER_LEN + 1];
    int buffer_len = 0;

    while ((buffer_len = recv(client->socket, buffer, BUFFER_LEN, 0)) > 0) {
        for (int i = 0; i < buffer_len; i++) {
            if (message_len == 0) {
                message_buffer = client->name + ":";
                message_len = message_buffer.length();
            }

            message_buffer += buffer[i];
            message_len++;

            if (buffer[i] == '\n') {
                unique_lock<mutex> lock(num_mtx);
                for (int j = 0; j < MAX_CLIENT_NUM; j++) {
                    if (clients[j] && clients[j]->valid && clients[j]->socket != client->socket) {
                        unique_lock<mutex> lock(clients[j]->mtx);
                        clients[j]->messages.push(message_buffer);
                        clients[j]->cv.notify_one();
                    }
                }
                message_len = 0;
                message_buffer.clear();
            }
        }
    }
    if (buffer_len == -1) {
        perror("recv");
    }
    closesocket(client->socket);
    client->valid = false;
    unique_lock<mutex> lock(num_mtx);
    current_client_num--;
}

void chat(Client* client) {
    thread send_thread(handle_send, client);
    thread recv_thread(handle_recv, client);
    send_thread.join();
    recv_thread.join();
}


int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 1;
    }

    // Create server socket
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCKET) {
        perror("socket");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;

    // Get server port and bind
    int server_port = 0;
    while (1) {
        printf("Please enter the port number of the server: ");
        scanf("%d", &server_port);

        addr.sin_port = htons(server_port);
        if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            perror("bind");
            continue;
        }
        break;
    }

    if (listen(server_sock, MAX_CLIENT_NUM + 1) == SOCKET_ERROR) {
        perror("listen");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Server started successfully!\n");
    printf("You can join the chatroom by connecting to 127.0.0.1:%d\n\n", server_port);

    // Wait for new clients to join
    while (1) {
        SOCKET client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == INVALID_SOCKET) {
            perror("accept");
            closesocket(server_sock);
            WSACleanup();
            return 1;
        }

        char name[NAME_LEN + 1] = { 0 };
        int state = recv(client_sock, name, NAME_LEN, 0);
        if (state == SOCKET_ERROR) {
            perror("recv");
            closesocket(client_sock);
            continue;
        } else if (state == 0) {
            closesocket(client_sock);
            continue;
        }

        bool client_added = false;
        for (int i = 0; i < MAX_CLIENT_NUM; i++) {
            if (!clients[i]) {
                clients[i] = make_unique<Client>();
                clients[i]->name = name;
                clients[i]->valid = true;
                clients[i]->socket = client_sock;
                clients[i]->fd_id = i;
                thread chat_thread(chat, clients[i].get());
                chat_thread.detach();
                printf("%s joined the chatroom. Online User Number: %d\n", clients[i]->name.c_str(), ++current_client_num);
                client_added = true;
                break;
            }
        }

        if (!client_added) {
            printf("Chatroom is full, %s cannot join.\n", name);
            closesocket(client_sock);
        }
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}






