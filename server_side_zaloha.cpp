#include <iostream>
using namespace std;

#include <cstdlib>
#include <cstdio>
#include <sys/socket.h> // socket(), bind(), connect(), listen()
#include <unistd.h>     // close(), read(), write()
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h>    // bzero()
#include <wait.h>       // waitpid()
#include <vector>
#include <utility>
#include <string.h>

enum Returns : char //-> enums for error handling returns
{
    SUCCESS = 0,
    WRONG_INPUT = -1,
    CREATE_SOCKET_FAIL = -2,
    CONNECTION_FAIL = -3,
    SEND_ERROR = -4,
    BIND_ERROR = -5,
    LISTEN_ERROR = -6,
    ACCEPT_ERROR = -7,
    SELECT_ERROR = -8,
    SOCKET_READING_ERROR = -9
};

#define SIZE_OF_BUFFER 8000
#define TIMEOUT 1

struct responses
{
    const char* SERVER_CONFIRMATION;
    const char* SERVER_MOVE = "102 MOVE\a\b";
    const char* SERVER_TURN_LEFT = "103 TURN LEFT";
    const char* SERVER_TURN_RIGHT = "104 TURN RIGHT";
    const char* SERVER_PICK_UP = "105 GET MESSAGE";
    const char* SERVER_LOGOUT = "106 LOGOUT";
    const char* SERVER_KEY_REQUEST = "107 KEY REQUEST\a\b";
};

struct client
{
    string CLIENT_USERNAME;
    string CLIENT_KEY_ID;
    string CLIENT_INFORMATION;
    string CLIENT_OK;
    string CLIENT_RECHARGING;
    string CLIENT_FULL_POWER;
    string CLIENT_MESSAGE;
};

bool send_funct(int& socket,const char* response)
{
    if (send(socket, response, strlen(response), MSG_NOSIGNAL) < 0)
    {
        perror("Unable to send data - locally detected error");
        close(socket);
        return SEND_ERROR;
    }
    return true;
}
//===============================================
bool authentization(int &fd_sock)
{
    client client;
    responses responses;
    char recv_buffer[8000];
    // authentizační klíče - slouží k autentizaci klienta
    //  first - server key
    //  second - client key
    vector<pair<int, int>> key = {{23019, 32037}, {32037, 2925}, {18789, 13603}, {16443, 29533}, {18189, 21952}};

    while (1)
    {
        int BytesRead = recv(fd_sock, recv_buffer, SIZE_OF_BUFFER - 1, 0);

        if (recv_buffer[BytesRead - 2] != '\a' || recv_buffer[BytesRead - 1] != '\b')
        {
            cout << "Chyba" << endl;
            return false;
            exit(1);
        }
        recv_buffer[BytesRead - 2] = '\0';
        client.CLIENT_USERNAME = recv_buffer;

        send_funct(fd_sock, responses.SERVER_KEY_REQUEST);

            return true;
    }
    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2) // error handling: wrong input
    {
        cerr << "Wrong input.\n"
             << "Arguments: [port]" << endl;
        return WRONG_INPUT;
    }

    //=====================
    // creating socket -
    // parameters -   1)network layer  protocol  -> AF_INET = IPv4
    //               2)transport layer protocol -> SOCK_STREAM = TCP
    //               3)return value             -> 0 (success)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Creating socket fail");
        return CREATE_SOCKET_FAIL;
    }

    // port - self-explanatory
    int port = atoi(argv[1]); // error handling - port is not a number or is 0
    if (port == 0)
    {
        cerr << "Wrong input.\n"
             << "Arguments: [port]" << endl;
        close(sock);
        return WRONG_INPUT;
    }
    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in serverAddr;
    // bzero() -> better safety - erases the data in the n bytes of the memory
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    // htons() - host to network translation - makes sure endian is correct for network transport
    serverAddr.sin_port = htons(port);
    // htonl() - network to host translation - used by receiver to ensure that endian is correct for receiver CPU
    // INADDR_ANY - binds the socket to all available interfaces.
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
    When a socket is created, it exists, but has no address assigned to it.
       => bind()  assigns the address specified by addr to the socket
        - bind has its own error that it will print to perror -> for example EINVAL - socket is already bound to adress
    */
    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("Bind error");
        close(sock);
        return BIND_ERROR;
    }
    /*
     listen()   - listen for connections on a socket
                - marks the socket as a passive socket
                -> used to accept incoming connection requests using accept()
                - second parameter is limit of connected end devices - after limit is reached - it wont open more connections
    */
    if (listen(sock, 10) < 0)
    {
        perror("Listen error:");
        close(sock);
        return LISTEN_ERROR;
    }

    struct sockaddr_in RemoteAddress;
    socklen_t AddrSize;

    while (true)
    {
        /*
            extracts the first connection request on the queue of pending connections
           listening socket and creates a new connected socket

           --> wait until client connect then assign clients connection data
           1) source port
           2) source IP address

           -return value of accept is file descriptor of the accepted socket
        */
        int fd_sock = accept(sock, (struct sockaddr *)&RemoteAddress, &AddrSize);

        if (fd_sock < 0)
        {
            perror("Accepting error: ");
            close(sock);
            return ACCEPT_ERROR;
        }

        /*
         multi-threading
        process id of one client - server can host multiple clients at once
        fork() -> creates new process
        - return 0 is it is a child
        -return childs number of process if it is a parent
        */
        pid_t pid = fork();

        // child process
        if (pid == 0)
        {
            // OS checks how many references points to listening socket - we only need parent to listen
            close(sock);

            // special data type that works with select()
            fd_set sockets;

            int select_retval;
            char recv_buffer[SIZE_OF_BUFFER];
            char send_buffer[SIZE_OF_BUFFER];
            bool authentization_bool = false;
            // communication loop
            while (1)
            {
                // timeout implementation to avoid zombie processes
                struct timeval timeout;
                timeout.tv_sec = TIMEOUT;
                timeout.tv_usec = 0;

                FD_ZERO(&sockets);         // set adress of fd_set sockets to zero
                FD_SET(fd_sock, &sockets); // add sockets, we want to listen to

                /*
                everytime we call recv(), we want to avoid blocking (recv() is a blocking function)
                -> we call select() before recv() => it blocks multiple sockets at once, so if socket has
                ready to read data select() unblocks and we can react

                select() timeout argument - The timeout argument is a timeval structure that specifies the interval that select() should block
                waiting for a file descriptor to become ready

                --> after each read select() decrement the time that select() was waiting for from the timeout value
                --> in this case client has 10 seconds to do everything he needs to do

                */
                select_retval = select(fd_sock + 1, &sockets, NULL, NULL, &timeout);
                if (select_retval < 0)
                {
                    perror("Select error: ");
                    close(fd_sock);
                    return SELECT_ERROR;
                }
                // FD_ISSET - checks if in data structure sockets - fd_sock is set
                //  --> testing this because selectv() unsets it after timeout
                if (!FD_ISSET(fd_sock, &sockets))
                {
                    cout << "Connection timeout!" << endl;
                    close(fd_sock);
                    return SUCCESS;
                }

                // AUTENTIZACE
                if (!authentization_bool)
                    authentization_bool = authentization(fd_sock);

                /*
                    recv() - used to receive messages from a socket
                            - equivalent to read() -> but has flags
                            - returns lenght of the message on succesfull completion
                */
                int BytesRead = recv(fd_sock, recv_buffer, SIZE_OF_BUFFER - 1, 0);
                // error handling
                if (BytesRead <= 0)
                {
                    perror("Socket reading error:");
                    close(fd_sock);
                    return SOCKET_READING_ERROR;
                }

                recv_buffer[BytesRead] = '\0';

                // Pokud prijmu konec ukoncim aktualni child procces
                if (string("konec") == recv_buffer)
                    break;

                cout << recv_buffer << endl;
            }
            close(fd_sock);
            return SUCCESS;
        }
        int status = 0;
        waitpid(0, &status, WNOHANG);
        close(fd_sock);
    }
    close(sock);
    return SUCCESS;
}
