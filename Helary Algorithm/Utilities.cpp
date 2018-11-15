#include <set>
#include <vector>
#include <sstream>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>

#include "DataTypes.cpp"

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
std::string unionAlreadySeenNeighbors(std::vector<int> alreadySeen, const std::vector<const int> &neighbors)
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
std::string constructAlreadySeenString(const int myID, const std::vector<const int> &neighbors)
{
    std::string s = "";
    for (const int nbr : neighbors)
    {
        s += std::to_string(nbr) + ",";
    }
    s += std::to_string(myID);
    return s;
}