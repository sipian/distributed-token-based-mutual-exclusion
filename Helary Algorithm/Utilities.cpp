#include <set>
#include <vector>
#include <sstream>
#include <assert.h> 
#include <iostream>
#include <string.h>
#include <arpa/inet.h>

#include "DataTypes.cpp"

/**
 * @brief Create a Receive Socket object
 * 
 * @param myID 
 * @param myPort 
 * @return int the receiver socket identifier
 */
int createReceiveSocket(const int myID, const int myPort)
{
    printf("INFO :: Node %d: createReceiveSocket -> Creating Socket %hu\n", myID, myPort);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(myPort);
    bzero(&server.sin_zero, 0);

    // creating the recv socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR :: Node %d: createReceiveSocket -> Error in creating socket :: Reason %s\n", myID, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // binding TCP server to IP and port
    if (bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
    {
        printf("ERROR :: Node %d: createReceiveSocket -> Error in binding socket to %hu :: Reason %s\n", myID, ntohs(server.sin_port), strerror(errno));
        exit(EXIT_FAILURE);
    }

    // mark it for listening
    if (listen(sockfd, MAX_NODES))
    {
        printf("ERROR :: Node %d: createReceiveSocket -> Error in listening to %hu :: Reason %s\n", myID, ntohs(server.sin_port), strerror(errno));
        exit(EXIT_FAILURE);
    }

    // set socket timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        printf("ERROR :: Node %d: createReceiveSocket -> Error in creating timeout for TCP connection :: Reason %s\n", myID, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

/**
 * @brief Send a message to another node
 * 
 * @tparam T 
 * @param myID 
 * @param dstID 
 * @param message 
 * @return true if the message was sent successfully, else false
 */
template <typename T>
bool sendMessage(const int myID, const int dstID, const T &message)
{
    assert(myID != dstID);

    // creating the send socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR :: Node %d: sendMessage -> Error in creating send socket :: Reason - %s\n", myID, strerror(errno));
        close(sockfd);
        return false;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(startPort + dstID);
    bzero(&server.sin_zero, 8);

    // connecting TCP to server
    if (connect(sockfd, (struct sockaddr *)(&server), sizeof(struct sockaddr_in)) < 0)
    {
        printf("ERROR :: Node %d: sendMessage -> Error in connecting to server %d:: Reason - %s\n", myID, dstID, strerror(errno));
        close(sockfd);
        return false;
    }

    if (send(sockfd, (char *)(&message), sizeof(message), 0) < 0)
    {
        printf("WARN :: Node %d: sendMessage -> Error in sending message-type %s to %d :: Reason - %s\n", myID, ((message.type == TOKEN) ? "TOKEN" : ((message.type == REQUEST) ? "REQUEST" : "TERMINATE")), dstID, strerror(errno));
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}

/* Req Array Utility Function */

/**
 * @brief Get the Neighbor ID from reqArray object. Used to get neighbor value on receiveToken.
 * 
 * @param elecID 
 * @return int neighbor ID from whom we received the request
 */
int getNeighborIDfromReqArray(const int myID, const int elecID, std::vector<RequestArrayNode> &reqArray)
{
    for (auto &nbr : reqArray)
    {
        for (auto it = nbr.requests.begin(); it != nbr.requests.end(); it++)
        {
            if (it->reqOriginId == elecID)
            {
                return nbr.neighborID;
            }
        }
    }

    printf("ERROR :: Node %d: getNeighborIDfromReqArray -> elecID not found in request array\n", myID);
    exit(EXIT_FAILURE);
}

/* Token Message Utility Functions */

/**
 * @brief Get the Lud From Token object. Used for logging vector clock values.
 * 
 * @param numNodes 
 * @param sharedTokenPtrPtr 
 * @return comma delimited string of logical clock values for each process  
 */
std::string getLudFromToken(const int numNodes, Token **sharedTokenPtrPtr)
{
    std::string res = "";
    for (int i = 0; i < numNodes; i++)
    {
        res += (*sharedTokenPtrPtr)->lud[i] + ",";
    }
    return res;
}

/* Request Message Utility Functions */

/**
 * @brief splits the comma delimited string and returns the process IDs in alreadySeen field in requestmessage
 * 
 * @param Q alreadySeen string in request message
 * @return vector<int> 
 */
std::vector<int> extractProcessIDFromAlreadySeen(char Q[])
{
    std::string str = std::string(Q);

    std::vector<int> processIDs;
    std::stringstream ss(str);
    int i;
    while (ss >> i)
    {
        processIDs.push_back(i);

        if (ss.peek() == ',')
        {
            ss.ignore();
        }
    }
    return processIDs;
}

/**
 * @brief Find Union of alreadySeen and neighbors and generate comma delimited string to be send as a part of request message
 * 
 * @param myID 
 * @param neighbors 
 * @return string message in format alreadySeen1,alreadySeen2,alreadySeen3,...,neighbors1,neighbors2,neighbors3...neighbors_m
 */
std::string unionAlreadySeenNeighbors(std::vector<int> alreadySeen, std::vector<int> &neighbors)
{
    std::set<int> s;
    for (int i : alreadySeen)
    {
        s.insert(i);
    }
    for (int i : neighbors)
    {
        s.insert(i);
    }

    std::string res = "";
    for (int nbr : s)
    {
        res += std::to_string(nbr) + ",";
    }
    return (res.size() > 0) ? res.substr(0, res.size() - 1) : res; //remove the trailing comma
}

/**
 * @brief Construct an already_seen message from neighbors and myID. Used in requestCS
 * 
 * @param myID 
 * @param neighbors 
 * @return string message in format neighbors1,neighbors2,neighbors3,...,myID
 */
std::string constructAlreadySeenString(const int myID, std::vector<int> &neighbors)
{
    std::string s = "";
    for (const int nbr : neighbors)
    {
        s += std::to_string(nbr) + ",";
    }
    s += std::to_string(myID);
    return s;
}