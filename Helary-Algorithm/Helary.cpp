#include <map>
#include <mutex>
#include <algorithm>

#include "Utilities.cpp"

/**
 * @brief to pass on the token to another node
 * 
 * @param myID 
 * @param tokenHere true if process has the token
 * @param logicalClock lamport's scalar logical clock
 * @param reqArray std::vector of requests received from neighbors
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param start for logging purposes
 * @param fp file pointer for saving the logs
 */
void transmitToken(const int myID, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray,
                   Token **sharedTokenPtrPtr, const int numNodes, const Time &start, FILE *fp)
{
    if (*sharedTokenPtrPtr == NULL)
    {
        printf("WARN :: Node %d: transmitToken -> sharedTokenPtrPtr points to NULL\n", myID);
        return;
    }

    // Compute the minimum val of the processes owning a pending request then find the oldest and send it the token.
    LLONG minRequestTime = 0;
    int minRequestID = -1;
    int minNbrVal;
    std::list<RequestID>::iterator minRequestNodeIterator;

    for (auto &i : reqArray)
    {
        for (auto it = i.second.begin(); it != i.second.end(); it++)
        {
            // finding minimum request from totally ordered reqArray

            if ((*sharedTokenPtrPtr)->lud[it->reqOriginId] < 0 || (*sharedTokenPtrPtr)->lud[it->reqOriginId] < it->reqTime)
            {
                if (minRequestID == -1)
                { // first time this condition is true
                    minRequestTime = it->reqTime;
                    minRequestID = it->reqOriginId;
                    minNbrVal = i.first;
                    minRequestNodeIterator = it;
                }
                else
                {
                    // minimum :: (y,z) -> (y < y') || (y == y' && z < z')
                    if (it->reqTime < minRequestTime)
                    {
                        minRequestTime = it->reqTime;
                        minRequestID = it->reqOriginId;
                        minNbrVal = i.first;
                        minRequestNodeIterator = it;
                    }
                    else if (it->reqTime == minRequestTime)
                    {
                        if (it->reqOriginId < minRequestID)
                        {
                            minRequestTime = it->reqTime;
                            minRequestID = it->reqOriginId;
                            minNbrVal = i.first;
                            minRequestNodeIterator = it;
                        }
                    }
                }
            }
        }
    }

    if (minRequestID != -1)
    {
        // remove pending request from request Array
        reqArray[minNbrVal].erase(minRequestNodeIterator);

        // update token fields
        (*sharedTokenPtrPtr)->type = messageType::TOKEN;
        (*sharedTokenPtrPtr)->senderID = myID;
        (*sharedTokenPtrPtr)->elecID = minRequestID;
        (*sharedTokenPtrPtr)->lud[myID] = logicalClock;

        logicalClock++;
        tokenHere = false;

        printf("INFO :: Node %d: transmitToken -> Sending Token for %d with clock %llu to neighbor %d\n", myID, minRequestID, minRequestTime, minNbrVal);
        long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d sends TOKEN to neighbor %d for %d with lud |%s| at %lld\n", myID, minNbrVal, minRequestID, getLudFromToken(numNodes, sharedTokenPtrPtr).c_str(), sysTime);
        fflush(fp);

        if (sendMessage(myID, minNbrVal, **sharedTokenPtrPtr) == false)
        {
            printf("ERROR :: Node %d: transmitToken -> Unable to send TOKEN to %d\n", myID, minNbrVal);
            exit(EXIT_FAILURE);
        }

        // removing the token
        *sharedTokenPtrPtr = NULL;
    }
}

/**
 * @brief On receiving the token either use it or pass it on the neighbor who has requested it.
 * 
 * @param myID 
 * @param inCS true if process is inside its CS
 * @param tokenHere true if process has the token
 * @param reqArray vector of requests received from neighbors
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param start for logging purposes
 * @param fp file pointer for saving the logs
 * @param lock mutex lock for the variables shared with the receiver thread
 */
void receiveToken(const int myID, int &inCS, int &tokenHere, std::map<int, RequestArrayNode> &reqArray,
                  Token **sharedTokenPtrPtr, const int numNodes, const Time &start, FILE *fp)
{

    tokenHere = TRUE;

    if ((*sharedTokenPtrPtr)->elecID == myID)
    {
        inCS = TRUE;
    }
    else
    {
        //The token is following the path that the corresponding request established.

        int nbrVal = getNeighborIDfromReqArray(myID, (*sharedTokenPtrPtr)->elecID, reqArray);
        // update token fields
        (*sharedTokenPtrPtr)->type = messageType::TOKEN;
        (*sharedTokenPtrPtr)->senderID = myID;
        tokenHere = FALSE;

        printf("INFO :: Node %d: receiveToken -> Sending Token for %d to neighbor %d\n", myID, (*sharedTokenPtrPtr)->elecID, nbrVal);
        long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d sends TOKEN to neighbor %d for %d with lud |%s| at %lld\n", myID, nbrVal, (*sharedTokenPtrPtr)->elecID, getLudFromToken(numNodes, sharedTokenPtrPtr).c_str(), sysTime);
        fflush(fp);
        if (sendMessage(myID, nbrVal, **sharedTokenPtrPtr) == false)
        {
            printf("ERROR :: Node %d: receiveToken -> Unable to send TOKEN for %d to neighbor %d\n", myID, (*sharedTokenPtrPtr)->elecID, nbrVal);
            exit(EXIT_FAILURE);
        }

        // removing the token
        *sharedTokenPtrPtr = NULL;
    }
}

/**
 * @brief 
 * 
 * @param myID 
 * @param inCS 
 * @param tokenHere 
 * @param logicalClock 
 * @param reqArray 
 * @param request 
 * @param neighbors 
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param start 
 * @param fp 
 * @param lock 
 */
void receiveRequest(const int myID, int &inCS, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray, RequestMessage *request,
                    std::vector<int> &neighbors, Token **sharedTokenPtrPtr, const int numNodes, const Time &start, FILE *fp)
{
    bool isRequestAlreadyPresent = false;

    for (auto &nbr : reqArray)
    {
        std::list<RequestID>::iterator it = nbr.second.begin();
        while (it != nbr.second.end())
        {
            if (it->reqOriginId == request->reqOriginId)
            {
                if (it->reqTime < request->reqTime)
                {
                    nbr.second.erase(it++);         //Delete an old request
                    continue;
                }
                else
                {
                    isRequestAlreadyPresent = true;
                }
            }
            it++;
        }
    }

    if (!isRequestAlreadyPresent)
    {
        //The request just received is a new one and is the youngest that the process ever received from requesting process
        logicalClock = std::max(logicalClock, request->reqTime) + 1;
        reqArray[request->senderID].push_back(RequestID{
            request->reqOriginId,
            request->reqTime});

        std::vector<int> alreadySeen = extractProcessIDFromAlreadySeen(request->alreadySeen);

        request->senderID = myID;
        strcpy(request->alreadySeen, unionAlreadySeenNeighbors(alreadySeen, neighbors).c_str());

        for (const int nbr : neighbors)
        {
            if (find(alreadySeen.begin(), alreadySeen.end(), nbr) == alreadySeen.end())
            {
                long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
                fprintf(fp, "%d sends REQUEST to %d for %d at %lld\n", myID, nbr, request->reqOriginId, sysTime);
                fflush(fp);
                if (sendMessage(myID, nbr, *request) == false)
                {
                    printf("ERROR :: Node %d: receiveRequest -> Unable to send REQUEST to %d for %d\n", myID, nbr, request->reqOriginId);
                    exit(EXIT_FAILURE);
                }
            }
        }
        if (tokenHere == TRUE && inCS == FALSE)
        {
            transmitToken(myID, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr, numNodes, start, fp);
        }
    }
}

/**
 * @brief is called when a process wants to enter the CS
 * 
 * @param myID 
 * @param inCS true if process is inside its CS
 * @param tokenHere true if process has the token
 * @param logicalClock lamport's scalar logical clock
 * @param neighbors to broadcast the request to
 * @param start for logging purposes
 * @param fp file pointer for saving the logs
 * @param lock mutex lock for the variables shared with the receiver thread
 */
void requestCS(const int myID, int &inCS, int &tokenHere, LLONG &logicalClock,
               std::vector<int> &neighbors, const Time &start, FILE *fp, std::mutex *lock)
{
    lock->lock();

    if (tokenHere == TRUE)
    {
        inCS = TRUE;
        lock->unlock();
    }
    else
    {
        RequestMessage message;
        message.reqTime = logicalClock;
        lock->unlock(); // unlock early to avoid unnecessary blocking of receiver thread

        message.type = messageType::REQUEST;
        message.senderID = myID;
        message.reqOriginId = myID;

        strcpy(message.alreadySeen, constructAlreadySeenString(myID, neighbors).c_str());

        // broadcast a request
        for (const int nbr : neighbors)
        {
            long long int sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
            fprintf(fp, "%d sends REQUEST to %d at %lld\n", myID, nbr, sysTime);
            fflush(fp);
            if (sendMessage(myID, nbr, message) == false)
            {
                printf("ERROR :: Node %d: requestCS -> Unable to send REQUEST to %d\n", myID, nbr);
                exit(EXIT_FAILURE);
            }
        }
    }

    while (true)
    {
        lock->lock();
        // wait to enter CS
        if (inCS == TRUE)
        {
            lock->unlock();
            break;
        }
        lock->unlock();
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/**
 * @brief is called when a process wants to exit the CS
 * 
 * @param myID 
 * @param inCS true if process is inside its CS
 * @param tokenHere true if process has the token
 * @param logicalClock lamport's scalar logical clock
 * @param reqArray std::vector of requests received from neighbors
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param start for logging purposes
 * @param fp file pointer for saving the logs
 * @param lock mutex lock for the variables shared with the receiver thread
 */
void exitCS(const int myID, int &inCS, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray,
            Token **sharedTokenPtrPtr, const int numNodes, const Time &start, FILE *fp, std::mutex *lock)
{
    lock->lock();
    inCS = false;
    transmitToken(myID, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr, numNodes, start, fp);
    lock->unlock();
}
