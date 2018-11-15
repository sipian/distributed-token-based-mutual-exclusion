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
