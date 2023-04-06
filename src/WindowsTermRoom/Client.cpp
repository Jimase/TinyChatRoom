#include <iostream>
#include <cstdio>
#include <cstring>
#include <limits>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <array>
#include <string>

using namespace std;

constexpr size_t BUFFER_LEN = 1024;
constexpr size_t NAME_LEN = 20;

array<char, NAME_LEN + 1> name; // client's name

// receive message and print out
void handle_recv(int pipe)
{
    // message buffer
    string message_buffer;
    int message_len = 0;

    // one transfer buffer
    array<char, BUFFER_LEN + 1> buffer;
    int buffer_len = 0;

    // receive
    while ((buffer_len = recv(pipe, buffer.data(), BUFFER_LEN, 0)) > 0)
    {
        // to find '\n' as the end of the message
        for (int i = 0; i < buffer_len; i++)
        {
            if (message_len == 0)
                message_buffer = buffer[i];
            else
                message_buffer += buffer[i];

            message_len++;

            if (buffer[i] == '\n')
            {
                // print out the message
                cout << message_buffer << endl;

                // new message start
                message_len = 0;
                message_buffer.clear();
            }
        }
        memset(buffer.data(), 0, sizeof(buffer));
    }
    // because the recv() function is blocking, so when the while() loop break, it means the server is offline
    printf("The Server has been shutdown!\n");
}

int main()
{
    // create a socket to connect with the server
    int client_sock;
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket");
        return -1;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;

    // get IP address and port of the server and connect
    int server_port = 0;
    array<char, 16> server_ip{};
    while (1)
    {
        printf("Please enter IP address of the server: ");
        scanf("%s", server_ip.data());
        printf("Please enter port number of the server: ");
        scanf("%d", &server_port);
        getchar(); // read useless '\n'

        addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.data(), &addr.sin_addr);
        // connect the server
        if (connect(client_sock, (struct sockaddr *)&addr, sizeof(addr)))
        {
            perror("connect");
            continue;
        }
        break;
    }

    // check state
    printf("Connecting......");
    fflush(stdout);
    array<char, 10> state{};
    if (recv(client_sock, state.data(), sizeof(state), 0) < 0)
    {
        perror("recv");
        return -1;
    }
    if (strcmp(state.data(), "OK"))
    {
        printf("\rThe chatroom is already full!\n");
        return 0;
    }
    else
    {
        printf("\rConnect Successfully!\n");
    }
    //////////////// get client name ////////////////
    printf("Welcome to Use TinyChatroom!\n");
    while (1)
    {
        printf("Please enter your name: ");
        cin.get(name.data(), NAME_LEN);
        int name_len = strlen(name.data());
        // no input
        if (cin.eof())
        {
            // reset
            cin.clear();
            clearerr(stdin);
            printf("\nYou need to input at least one word!\n");
            continue;
        }
            // single enter
        else if (name_len == 0)
        {
            // reset
            cin.clear();
            clearerr(stdin);
            cin.get();
            continue;
            printf("\nYou need to input at least one word!\n");
        }
        // overflow
        if (name_len > NAME_LEN - 2)
        {
            // reset
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printf("\nReached the upper limit of the words!\n");
            continue;
        }
        cin.get(); // remove '\n' in stdin
        name[name_len] = '\0';
        break;
    }
    if (send(client_sock, name.data(), strlen(name.data()), 0) < 0)
    {
        perror("send");
        return -1;
    }
    //////////////// get client name ////////////////

    // create a new thread to handle receive message
    thread recv_thread(handle_recv, client_sock);

    // get message and send
    while (1)
    {
        array<char, BUFFER_LEN + 1> message;
        cin.get(message.data(), BUFFER_LEN);
        int n = strlen(message.data());
        if (cin.eof())
        {
            // reset
            cin.clear();
            clearerr(stdin);
            continue;
        }
            // single enter
        else if (n == 0)
        {
            // reset
            cin.clear();
            clearerr(stdin);
        }
        // overflow
        if (n > BUFFER_LEN - 2)
        {
            // reset
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printf("Reached the upper limit of the words!\n");
            continue;
        }
        cin.get();         // remove '\n' in stdin
        message[n] = '\n'; // add '\n'
        message[n + 1] = '\0';
        // the length of message now is n+1
        n++;
        printf("\n");
        // the length of message that has been sent
        int sent_len = 0;
        // calculate one transfer length
        int trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;

        // send message
        while (n > 0)
        {
            int len = send(client_sock, message.data() + sent_len, trans_len, 0);
            if (len < 0)
            {
                perror("send");
                return -1;
            }
            // if one transfer has not sent the full message, then send the remain message
            n -= len;
            sent_len += len;
            trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
        }
        // clean the buffer
        memset(message.data(), 0, sizeof(message));
    }

    recv_thread.join();
    shutdown(client_sock, 2);
    return 0;
}

