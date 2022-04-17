/**
 * @file main.cpp
 * @author Matěj Schrödl (matej.schrodl@gmail.com)
 * @brief   multi-threaded server for TCP / IP communication, its purpose is to guide robot (client) through a maze to get a secret message
 * @version 0.8
 * @date 2022-04-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */

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
#include <list>
#include <string>
#include <regex>
#include <bsd/stdlib.h>
#include <limits.h>
//======================================================================================================================================
//
//                                      INITIALIZATION
//
//======================================================================================================================================

#define SIZE_OF_BUFFER 500
#define TIMEOUT 1 // time for a client to respond

enum Direction : char //-> for directions
{
    DOLEVA = 0,
    DOPRAVA = 1,
    DOLU = 2,
    NAHORU = 3,
};

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


/**
 * @brief server messages to client
 * 
 */
struct response
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
response responses;

/**
 * @brief max sizes of clients messages
 * 
 */
struct client_sizes
{
    int SIZE_USERNAME = 20;
    int SIZE_KEY_ID = 5;
    int SIZE_CONFIRMATION = 7;
    int SIZE_OK = 12;
    int SIZE_RECHARGING = 12;
    int SIZE_FULL_POWER = 12;
    int SIZE_MESSAGE = 100;
};
client_sizes sizes;

/**
 * @brief storage for clients info
 * 
 */
struct client_info
{
    string CLIENT_USERNAME;
    int CLIENT_KEY_ID;
    int CLIENT_CONFIRMATION;
    string CLIENT_OK;
    string CLIENT_RECHARGING;
    string CLIENT_FULL_POWER;
    string CLIENT_MESSAGE;
};
client_info client;

//list of received messages, messages need to be stores beccause of fragmentation and merging 
list<string> v_messages;

//timeout structures
fd_set sockets;
int select_retval;


//======================================================================================================================================
//
//                                      UTILITY FUNCTIONS
//
//======================================================================================================================================




/**
 * @brief sends a message to a client
 * 
 * @param socket    --> socket
 * @param response  --> message to send
 */
void send_message(int &socket, const char *response)
{
    if (send(socket, response, strlen(response), MSG_NOSIGNAL) < 0)
    {
        perror("Unable to send data - locally detected error");
        close(socket);
        _exit(SEND_ERROR);
    }
}

/**
 * @brief removes all occurences of '\0', used to deal with strange usernames such as "\a\a\0\b\b\a\0\a\b"
 * 
 * @param buffer         --> buffer
 * @param c         --> "\0"
 * @param buffer_lenght     --> lenght of buffer
 * @return int 
 */
int RemoveChars(char *buffer, char c, int buffer_lenght)
{
    int writer = 0, reader = 0;
    int i = 0;
    while (i < buffer_lenght)
    {
        if (buffer[reader] != c)
        {
            buffer[writer++] = buffer[reader];
        }
        i++;
        reader++;
    }

    buffer[writer] = 0;

    return (buffer_lenght - writer);
}

/**
 * @brief splits string received from a buffer a pushes individual messages to list v_messages
 * 
 * @param s     --> buffer
 */
void splitToVector(string s)
{
    
    std::string delimeter = "\a\b";
    if (s.find("\a\b") == std::string::npos)
    {
        v_messages.push_back(s);
    }
    else
    {
        size_t pos = 0;
        std::string token;
        while ((pos = s.find(delimeter)) != std::string::npos)
        {
            token = s.substr(0, pos);
            token += "\a\b\0";
            v_messages.push_back(token);
            s.erase(0, pos + delimeter.length());
        }
        if (s != "\0")
            v_messages.push_back(s);
    }
}

/**
 * @brief function to deal with weird usernames
 * 
 * @param c 
 * @param size 
 */
void weird_char_check(char c[SIZE_OF_BUFFER], int size, int& sock)
{
    for (int i = 0; i < (size - 3); i++)
    {
        if (c[i] == '\a' && c[i + 1] == '\0' && c[i + 2] == '\b')
        {
            c[i] = 15;
            c[i + 2] = '\0';
        }
         if (c[i+1] == ' ' && c[i+2] == '\a' &&  c[i + 3] == '\b')
         {

                    send_message(sock, "301 SYNTAX ERROR\a\b");
                    close(sock);
                    _exit(EXIT_FAILURE);
         }
    }
}

/**
 * @brief receives a message a pushes it to v_messages list
 * 
 * @param socket 
 * @param recv_buffer 
 * @param size 
 * @return int 
 */
int recv_message(int &socket, char recv_buffer[SIZE_OF_BUFFER], int size)
{    
    //======================================================================================================================================
    //  TIMEOUT INITIALIZATION
    struct timeval timeout;
                timeout.tv_sec = 2;
                timeout.tv_usec = 0;
                FD_ZERO(&sockets);         // set adress of fd_set sockets to zero
                FD_SET(socket, &sockets); // add sockets, we want to listen to


    //======================================================================================================================================
    //  INITIALIZATION
    int removed_chars = 0;
    int BytesRead = 0;
    memset(recv_buffer, '\0', 100);

    
    //======================================================================================================================================
    //  RECEIVING AND PUSHING MESSAGES TO LIST
    if (v_messages.empty())
    {
            //======================================================================================================================================
            //  TIMEOUT
                        select_retval = select(socket + 1, &sockets, NULL, NULL, &timeout);
                if (select_retval == 0)
                {
                    perror("Select error: ");
                    close(socket);
                    _exit(EXIT_FAILURE);
                }
                if (!FD_ISSET(socket, &sockets))
                {
                    cout << "Connection timeout!" << endl;
                    close(socket);
                    _exit(EXIT_FAILURE);
                }


        int number_bytes = recv(socket, recv_buffer, SIZE_OF_BUFFER - 1, 0);
        if (number_bytes == 0)
            _exit(EXIT_SUCCESS);
        else if (number_bytes < 0)
                _exit(EXIT_SUCCESS);

        //checks weird usernames                
        weird_char_check(recv_buffer, number_bytes, socket);
        removed_chars = RemoveChars(recv_buffer, '\0', number_bytes);
        //
        string new_recv_buffer = recv_buffer;
        splitToVector(new_recv_buffer);
    }
    //======================================================================================================================================
    //  RETREIVING MESSAGES FROM THE LIST
    string str = v_messages.front();
    v_messages.pop_front();

    const char *recv_buffer_const = str.c_str();
    strcpy(recv_buffer, recv_buffer_const);
    BytesRead = strlen(recv_buffer);

    removed_chars = RemoveChars(recv_buffer, '\0', BytesRead);



    //======================================================================================================================================
    //  FRAGMENTATION RESOLUTION

    if (str.find("\a\b") == std::string::npos)
    {
        while (str.find("\a\b") == std::string::npos)
        {
                //======================================================================================================================================
                //  TIMEOUT
             select_retval = select(socket + 1, &sockets, NULL, NULL, &timeout);
                if (select_retval == 0)
                {
                    perror("Select error: ");
                    close(socket);
                    _exit(EXIT_FAILURE);
                }
                // FD_ISSET - checks if in data structure sockets - fd_sock is set
                //  --> testing this because selectv() unsets it after timeout
                if (!FD_ISSET(socket, &sockets))
                {
                    cout << "Connection timeout!" << endl;
                    close(socket);
                     _exit(EXIT_FAILURE);
                }
                memset(recv_buffer, '\0', 100);
            int number_bytes = recv(socket, recv_buffer, SIZE_OF_BUFFER - 1, 0);
            if (number_bytes == 0)
                _exit(EXIT_FAILURE);

            else if (number_bytes < 0)
                _exit(EXIT_SUCCESS);
            
            str += recv_buffer;
        }
        // pushing messages and fragments of messages into a list
        splitToVector(str);
        str = v_messages.front();
        v_messages.pop_front();
        const char *recv_buffer_const = str.c_str();
        strcpy(recv_buffer, recv_buffer_const);
        BytesRead = strlen(recv_buffer);
    }



    //adding null byte to the message
    recv_buffer[BytesRead - (2 + removed_chars)] = '\0';

    if (BytesRead > size)
    {

        send_message(socket, "301 SYNTAX ERROR\a\b");
        close(socket);
        _exit(EXIT_FAILURE);
    }

    if (BytesRead <= 0)
    {
        close(socket);
        _exit(EXIT_FAILURE);
    }

    return BytesRead;
}

/**
 * @brief return a pair of coordinates robot is at, also checks if received message was correct
 *
 * @param recv_buffer       --> buffer
 * @param fd_sock           --> socket
 * @return pair<int,int>    --> coordinates
 */
pair<int, int> split(char recv_buffer[SIZE_OF_BUFFER], int &fd_sock)
{
    pair<int, int> pair;
    const char test[20] = "OK";
    const char *delim = " ";
    char *token = strtok(recv_buffer, delim);

    if (strcmp(token, test))
    {
        cout << strcmp(token, test) << endl;
        cout << token << endl;
        close(fd_sock);
        exit(1);
    }

    token = strtok(NULL, delim);
    pair.first = atoi(token);

    token = strtok(NULL, delim);
    pair.second = atoi(token);

    return pair;
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

 * @param fd_sock           --> socket
 * @return pair<int,int>    --> returns the pair from key vector
 */
pair<int, int> keyCheck(const int client_id, const vector<pair<int, int>> &key, response &responses, int &fd_sock)
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
 * @brief authentizates the client
 *
 * @param fd_sock   --> provided socket
 * @return true     --> client was authentizated
 * @return false    --> client didnt authentizate
 */
void authentization(int &fd_sock)
{
    char recv_buffer[SIZE_OF_BUFFER];
    // authentizační klíče - slouží k autentizaci klienta
    //  first - server key
    //  second - client key
    vector<pair<int, int>> key = {{23019, 32037}, {32037, 2925}, {18789, 13603}, {16443, 29533}, {18189, 21952}};

    cout << "\n--Robot is authentizating to the server--\n"
         << endl;

    recv_message(fd_sock, recv_buffer, sizes.SIZE_USERNAME);
    client.CLIENT_USERNAME = recv_buffer;
    cout << "CLIENT_NAME:\t" << client.CLIENT_USERNAME << endl;
    if ((int)client.CLIENT_USERNAME.size() > sizes.SIZE_USERNAME)
    {
        close(fd_sock);
        exit(1);
    }

    //======================================================================================================================================
    //  SENDING KEY REQUEST
    send_message(fd_sock, responses.SERVER_KEY_REQUEST);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_KEY_ID);
    char* err = nullptr;
    cout << "recv buffer:\t" << recv_buffer << endl;
    client.CLIENT_KEY_ID = strtol(recv_buffer, &err, 10);
    if(*err)
        {
        cout << "-- Error: Expected a number --" << endl;
        send_message(fd_sock, "301 SYNTAX ERROR\a\b");
        close(fd_sock);
        _exit(EXIT_FAILURE);
        }

    cout << "CLIENT_KEY_ID:\t" << client.CLIENT_KEY_ID << endl;

    // checks if key send by client is legit
    pair<int, int> key_pair = keyCheck(client.CLIENT_KEY_ID, key, responses, fd_sock);
    
    //======================================================================================================================================
    //  CALCULATING SERVER HASH
    const int MAX_NUM = 65536; // highest possible 12-bit int
    int hash = 0;

    for (size_t i = 0; i < client.CLIENT_USERNAME.size(); i++)
    {

        hash += (int)client.CLIENT_USERNAME[i] % MAX_NUM;
    }
    hash = (hash * 1000) % MAX_NUM;

    int hash_server = (hash + key_pair.first) % MAX_NUM;

    char hash_server_char[100];
    // from int to char
    sprintf(hash_server_char, "%d", hash_server);
    string conf_message = hash_server_char;
    conf_message += "\a\b";
    const char *confirm_message_char = conf_message.c_str();

    cout << "SERVER_HASH:\t" << hash_server_char << endl;

    // assign it to server response that I will send to client
    responses.SERVER_CONFIRMATION = confirm_message_char;

    send_message(fd_sock, responses.SERVER_CONFIRMATION);

    //======================================================================================================================================
    //  CHECKING CLIENT HASH

    // receiving bash client hash
    recv_message(fd_sock, recv_buffer, sizes.SIZE_CONFIRMATION);
    client.CLIENT_CONFIRMATION = atoi(recv_buffer);
    if(client.CLIENT_CONFIRMATION == 0)
        cout << "sadly not a number" << endl;

    cout << "CLIENT_HASH:\t" << client.CLIENT_CONFIRMATION << endl;

    int hash_client = (hash + key_pair.second) % MAX_NUM;

    // is hash_client send by client and calculated by server the same
    if (client.CLIENT_CONFIRMATION == hash_client)
        send_message(fd_sock, responses.SERVER_OK);

    else
    {
        cout << "\n --Server sent a wrong authentization--\n"
             << endl;
        send_message(fd_sock, responses.SERVER_LOGIN_FAILED);
        close(fd_sock);
        _exit(EXIT_FAILURE);
    }
}
//======================================================================================================================================
//
//                                      SEARCH FOR SECRET
//
//======================================================================================================================================

/**
 * @brief i have to know the direction which the robot is pointed toward so server can navigate toward secret message
 *
 * @param fd_sock               --> socket
 * @param responses             --> server responses
 * @param recv_buffer           --> buffer
 * pair<int,int>& second_step   --> pair of coordinates, when we will begin moving the robot - server will be reffering to them
 */
int direction(int fd_sock, char recv_buffer[SIZE_OF_BUFFER], pair<int, int> &second_step)
{

    pair<int, int> first_step;
    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    first_step = split(recv_buffer, fd_sock);

    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    second_step = split(recv_buffer, fd_sock);

    // comparing X values
    if (first_step.first > second_step.first)
        return DOLEVA;
    else if (first_step.first < second_step.first)
        return DOPRAVA;
    // comparing Y values
    else if (first_step.second > second_step.second)
        return DOLU;
    else if (first_step.second < second_step.second)
        return NAHORU;

    //Vyskytla se prekazka
    else
    {   
        int v1 = rand() % 2;
        switch(v1)
        {
            case 0: send_message(fd_sock, responses.SERVER_TURN_LEFT);
                    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);

            case 1:send_message(fd_sock, responses.SERVER_TURN_RIGHT);
                    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
             
        } 
        cout << "--Obstacle on the start--" << endl;
        send_message(fd_sock, responses.SERVER_TURN_LEFT);
        recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
        return direction(fd_sock, recv_buffer, second_step);
    }
}

/**
 * @brief   when moving the robot, we have to take into account his rotation, to make it easier,
 *          i decided, after every move (to the left, to the right, up and down) he will be rotated to the right
 *          -->this will make moving the robot much easier (also more expensive thought)
 *
 * @param fd_sock       --> socket
 * @param responses     --> server responses
 * @param recv_buffer   --> receiving buffer
 * @param smer          --> enum to which he direction is rotated righ now
 */
void setDirRight(int fd_sock, char recv_buffer[SIZE_OF_BUFFER], int smer)
{


    switch (smer)
    {
    case DOPRAVA:
        break;
    case DOLEVA:
        send_message(fd_sock, responses.SERVER_TURN_LEFT);
        recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
        send_message(fd_sock, responses.SERVER_TURN_LEFT);
        recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
        break;

    case NAHORU:
        send_message(fd_sock, responses.SERVER_TURN_RIGHT);
        recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
        break;
    case DOLU:

        send_message(fd_sock, responses.SERVER_TURN_LEFT);
        recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
        break;
    }
}

//======================================================================================================================================
//                                      ROBOT MOVE FUNCTIONS
//======================================================================================================================================
pair<int, int> moveLeft(int &fd_sock, char recv_buffer[SIZE_OF_BUFFER])
{
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    pair<int, int> coords = split(recv_buffer, fd_sock);
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);

    return coords;
}
pair<int, int> moveRight(int &fd_sock,  char recv_buffer[SIZE_OF_BUFFER] )
{
    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    pair<int, int> coords = split(recv_buffer, fd_sock);

    return coords;
}

pair<int, int> moveUp(int &fd_sock, char recv_buffer[SIZE_OF_BUFFER])
{
    send_message(fd_sock, responses.SERVER_TURN_LEFT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    pair<int, int> coords = split(recv_buffer, fd_sock);
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);

    return coords;
}
pair<int, int> moveDown(int &fd_sock, char recv_buffer[SIZE_OF_BUFFER])
{
    send_message(fd_sock, responses.SERVER_TURN_RIGHT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    send_message(fd_sock, responses.SERVER_MOVE);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);
    pair<int, int> coords = split(recv_buffer, fd_sock);
    send_message(fd_sock, responses.SERVER_TURN_LEFT);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_OK);

    return coords;
}

/**
 * @brief function robots movement on the x axis, only in a function because the code would have to be copied otherwise, works on algorithm explained later
 * 
 * @param fd_sock       --> socket
 * @param now_coords    --> cordination robot is at right now
 * @param responses     --> server responses to the client
 * @param recv_buffer   --> buffer
 * @param sizes         --> sizes check
 * @param previous_coords 
 */
void xMovement(int &fd_sock, pair<int, int> &now_coords,  char recv_buffer[SIZE_OF_BUFFER], pair<int, int> &previous_coords)
{

    if (now_coords.first > 0)
    {
        previous_coords = now_coords;
        now_coords = moveLeft(fd_sock, recv_buffer);
        cout << "move: Left\n("
             << "now_coords:\t" << now_coords.first << "," << now_coords.second << " )" << endl;
    }
    if (now_coords.first < 0)
    {
        previous_coords = now_coords;
        now_coords = moveRight(fd_sock,  recv_buffer);
        cout << "move: Right\n("
             << "now_coords:\t" << now_coords.first << "," << now_coords.second << " )" << endl;
    }
}
/**
 * @brief navigates robot throught the maze a picks up a secret message
 * 
 * @param fd_sock   --> socket
 */

void searchForSecret(int &fd_sock)
{

    //======================================================================================================================================
    //  SEARCHING ALGORTHITHM INITIALIZATION 
    char recv_buffer[SIZE_OF_BUFFER];
    pair<int, int> previous_coords = {0, 0};
    pair<int, int> now_coords;
    pair<int, int> secret_pair = {0, 0};

    int smer = direction(fd_sock, recv_buffer, now_coords);

    // i want direction to be right so I can work with robot two dimensionaly
    setDirRight(fd_sock, recv_buffer, smer);
    //======================================================================================================================================

    //======================================================================================================================================
    //  ROBOT MOVING THROUGHT THE MAZE ALGORTHITHM
    // (0,0) has been reached, now we can pick up the secret message

    /*
        Searching algorthimth, robot needs to reach x,y = (0,0) coordination, robot will first try to reach y = 0
        obstacles may appear, if it happens robot will try to move towards x = 0 coordination instead, right after that he will again try to reach y = 0
        after he reaches y = 0, he will then move in the x direction. If an obstacle appears in x direction he will move one block in y and one block in x,
        -- we know for a fact that obstacle is always 1x1, otherwise this algorthitm wouldn't always work -- 
    */
       cout << "\n--Robot is starting its search for secret message--\n"
         << endl;
    while (now_coords != secret_pair)
    {

        if (now_coords == previous_coords && (now_coords.first == 0 || now_coords.second == 0))
        {
            moveDown(fd_sock,  recv_buffer);
            xMovement(fd_sock, now_coords, recv_buffer, previous_coords);
        }

        if (now_coords.second > 0)
        {
            previous_coords = now_coords;
            now_coords = moveDown(fd_sock,  recv_buffer);
            cout << "move: Down\n( "
                 << "now_coords:\t" << now_coords.first << "," << now_coords.second << " )" << endl;

            if (now_coords == previous_coords && (now_coords.first == 0 || now_coords.second == 0))
            {
                moveUp(fd_sock, recv_buffer);
                xMovement(fd_sock, now_coords, recv_buffer, previous_coords);
            }
            else if (now_coords != previous_coords)
                continue;
        }

        if (now_coords.second < 0)
        {
            previous_coords = now_coords;
            now_coords = moveUp(fd_sock,  recv_buffer);
            cout << "move: Up\n("
                 << "now_coords:\t" << now_coords.first << "," << now_coords.second << " )" << endl;

            if (now_coords == previous_coords && (now_coords.first == 0 || now_coords.second == 0))
            {
                moveDown(fd_sock, recv_buffer);
                xMovement(fd_sock, now_coords, recv_buffer,   previous_coords);
            }
            if (now_coords != previous_coords)
                continue;
        }

        // if we are done with Y coordination --> Y=0 OR Y path is blocked we move in X axis
        xMovement(fd_sock, now_coords,  recv_buffer, previous_coords);
    }

    //======================================================================================================================================
    //  SECRET MESSAGE PICK-UP
    // (0,0) has been reached, now we can pick up the secret message
    
    cout << "\n --Robot has found the secret message--\n" << endl;
    cout << "The message reads:" << endl;
    send_message(fd_sock, responses.SERVER_PICK_UP);
    recv_message(fd_sock, recv_buffer, sizes.SIZE_MESSAGE);
    cout << recv_buffer << endl;
    send_message(fd_sock, responses.SERVER_LOGOUT);
    close(fd_sock);
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
   int return_listen = listen(sock, 10);
    if (return_listen < 0)
    {
        perror("Listen error:");
        close(sock);
        return LISTEN_ERROR;
    }
    cout << "listen: " << return_listen << endl;

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
        cout << "fd_sock:" <<fd_sock << endl;
        if (fd_sock < 0)
        {
            perror("Accept error: ");
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

            while(1){
            

            // special data type that works with select()
            // communication loop

            //==============================================================================
            // AUTHENTIZATION OF ROBOT
            authentization(fd_sock);
            //==============================================================================

            //==============================================================================
            // NAVIGATING ROBOT TO THE SECRET
            searchForSecret(fd_sock);
            //==============================================================================
            }   
            cout << "\n --Closing connection with the robot--\n"
                 << endl;
            close(fd_sock);
            return SUCCESS;
            
        }
        cout << "--Closing process--" << endl;
        int status = 0;
        waitpid(0, &status, WNOHANG);
        close(fd_sock);
            
    }
    close(sock);
    return SUCCESS;
}
