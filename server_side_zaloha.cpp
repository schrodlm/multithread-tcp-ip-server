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
    const char *SERVER_CONFIRMATION;
    const char *SERVER_MOVE = "102 MOVE\a\b";
    const char *SERVER_TURN_LEFT = "103 TURN LEFT\a\b";
    const char *SERVER_TURN_RIGHT = "104 TURN RIGHT\a\b";
    const char *SERVER_PICK_UP = "105 GET MESSAGE\a\b";
    const char *SERVER_LOGOUT = "106 LOGOUT\a\b";
    const char *SERVER_KEY_REQUEST = "107 KEY REQUEST\a\b";
    const char *SERVER_OK = "200 OK\a\b";
    const char *SERVER_LOGIN_FAILED = "300 LOGIN FAILED\a\b";
    const char *SERVER_SYNTAX_ERROR = "301 SYNTAX ERROR\a\b";
    const char *SERVER_LOGIC_ERROR = "302 LOGIC ERROR\a\b";
    const char *SERVER_KEY_OUT_OF_RANGE_ERROR = "303 KEY OUT OF RANGE\a\b";
};

struct client_sizes
{
    int SIZE_USERNAME = 20;
    int SIZE_KEY_ID = 5;
    int SIZE_CONFIRMATION = 7; 
    int SIZE_OK = 12;
    int SIZE_RECHARGING = 12;
    int SIZE_FULL_POWER = 12;
    int SIZE_MESSAGE = 12; 
};

struct client
{
    string CLIENT_USERNAME;
    int CLIENT_KEY_ID;
    int CLIENT_CONFIRMATION;
    string CLIENT_OK;
    string CLIENT_RECHARGING;
    string CLIENT_FULL_POWER;
    string CLIENT_MESSAGE;
};

void send_message(int &socket, const char *response)
{
    if (send(socket, response, strlen(response), MSG_NOSIGNAL) < 0)
    {
        perror("Unable to send data - locally detected error");
        close(socket);
        exit(SEND_ERROR);
    }
}

//==============================================
/**
 * @brief Checks if received message had the right ending chars
 *
 * @return true
 * @return false
 */
bool endingCharCheck(const char checkBuffer[8000], int BytesRead)
{
    if (checkBuffer[BytesRead - 2] != '\a' || checkBuffer[BytesRead - 1] != '\b')
    {
        // TODO
        cout << "Chyba" << endl;
        return false;
    }
    return true;
}

int recv_message(int &socket, char recv_buffer[8000], int size)
{
    int BytesRead = recv(socket, recv_buffer, SIZE_OF_BUFFER - 1, 0);

    //size check
    if(BytesRead > size)
    {
        close(socket);
        send_message(socket, "301 SYNTAX ERROR\a\b");
        exit(1);
    }

    endingCharCheck(recv_buffer, BytesRead);
    recv_buffer[BytesRead - 2] = '\0';

    if (BytesRead <= 0)
    {
        close(socket);
        exit(1);
    }

    return BytesRead;
}

//======================================================================================================================================
//
//                                      AUTHENTIZATION
//
//======================================================================================================================================
/**
 * @brief checks if provided key is right -subfunction of authentization
 *
 * @param client_id         --> index of the key - it should be between 0-4
 * @param key               --> pair of keys, first for server, second for client
 * @param responses         --> struct of responses to send to client
 * @param fd_sock           --> socket
 * @return pair<int,int>    --> returns the pair from key vector
 */
pair<int, int> keyCheck(const int client_id, const vector<pair<int, int>> &key, responses &responses, int &fd_sock)
{
    if (client_id < 0 || client_id > 4)
    {
        send_message(fd_sock, responses.SERVER_KEY_OUT_OF_RANGE_ERROR);
        cout << "SERVER_KEY_OUT_OF_RANGE" << endl;
        close(fd_sock);
        exit(1);
    }

    pair<int, int> key_pair = key[client_id];
    return key_pair;
}
//===============================================
/**
 * @brief authentizates the client -
 *
 * @param fd_sock   --> provided socket
 * @return true     --> client was authentizated
 * @return false    --> client didnt authentizate
 */
void authentization(int &fd_sock, client &client, responses &responses)
{   
    client_sizes sizes;
    char recv_buffer[8000];
    // authentizační klíče - slouží k autentizaci klienta
    //  first - server key
    //  second - client key
    vector<pair<int, int>> key = {{23019, 32037}, {32037, 2925}, {18789, 13603}, {16443, 29533}, {18189, 21952}};

    recv_message(fd_sock, recv_buffer, sizes.SIZE_USERNAME);
    client.CLIENT_USERNAME = recv_buffer;

    // sending KEY_REQUEST
    send_message(fd_sock, responses.SERVER_KEY_REQUEST);
    recv_message(fd_sock, recv_buffer,sizes.SIZE_KEY_ID);
    client.CLIENT_KEY_ID = atoi(recv_buffer);

    cout << "CLIENT_KEY_ID:\t" << client.CLIENT_KEY_ID << endl;

    // checks if key send by client is legit
    pair<int, int> key_pair = keyCheck(client.CLIENT_KEY_ID, key, responses, fd_sock);

    // vypočítáni hashe
    //========================================
    const int MAX_NUM = 65536; // highest possible 12-bit int
    int hash = 0;

    for (size_t i = 0; i < client.CLIENT_USERNAME.size(); i++)
    {
        hash += (int)client.CLIENT_USERNAME[i] % MAX_NUM;
        // debug
        // cout << (int)client.CLIENT_USERNAME[i] << ":" << client.CLIENT_USERNAME[i] << endl;
    }
    hash = (hash * 1000) % MAX_NUM;

    int hash_server = (hash + key_pair.first) % MAX_NUM;

    char hash_server_char[100];
    // from int to char
    sprintf(hash_server_char, "%d", hash_server);
    string conf_message = hash_server_char;
    conf_message += "\a\b";
    const char *confirm_message_char = conf_message.c_str();

    cout << "SERVER HASH:\t" << hash_server_char << endl;

    // assign it to server response that I will send to client
    responses.SERVER_CONFIRMATION = confirm_message_char;
    // int hash_client = (hash * key_pair.second) % MAX_NUM;

    send_message(fd_sock, responses.SERVER_CONFIRMATION);

    // receiving bash client hash
    recv_message(fd_sock, recv_buffer, sizes.SIZE_CONFIRMATION);
    client.CLIENT_CONFIRMATION = atoi(recv_buffer);

    cout << "CLIENT_CNFRM:\t" << client.CLIENT_CONFIRMATION << endl;

    int hash_client = (hash + key_pair.second) % MAX_NUM;

    // is hash_client send by client and calculated by server the same
    if (client.CLIENT_CONFIRMATION == hash_client)
        send_message(fd_sock, responses.SERVER_OK);

    else
    {
        send_message(fd_sock, responses.SERVER_LOGIN_FAILED);
        close(fd_sock);
        exit(1);
    }
}
//======================================================================================================================================
//
//                                      SEARCH FOR SECRET
//
//======================================================================================================================================
void searchForSecret(int &fd_sock, client &client, responses &responses)
{
    client_sizes sizes;
    char recv_buffer[8000];

    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    
    
}
//======================================================================================================================================
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
            // communication loop
            while (1)
            {
                responses responses;
                client client;
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
                //==============================================================================
                // AUTENTIZACE
                authentization(fd_sock, client, responses);
                //==============================================================================

                searchForSecret(fd_sock, client, responses);

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
