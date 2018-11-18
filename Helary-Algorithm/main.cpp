#include <atomic>
#include <thread>
#include <random>
#include <fstream>
#include <unistd.h>

#include "Helary.cpp"

/**
 * @brief simulate mutual exclusion requests
 * 
 * @param myID 
 * @param inCS 
 * @param tokenHere 
 * @param logicalClock 
 * @param reqArray 
 * @param neighbors 
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param mutualExclusionCounts 
 * @param alpha 
 * @param beta 
 * @param start 
 * @param fp 
 * @param lock 
 */

std::atomic<int> totalReceivedMessages = ATOMIC_VAR_INIT(0);
std::atomic<long long int> totalResponseTime = ATOMIC_VAR_INIT(0);

void working(const int myID, int &inCS, int &tokenHere, LLONG &logicalClock, std::map<int, RequestArrayNode> &reqArray,
             std::vector<int> &neighbors, Token **sharedTokenPtrPtr, const int numNodes, std::atomic<int> &finishedProcessesCount,
             int mutualExclusionCounts, float alpha, float beta, const Time &start, FILE *fp, std::mutex *lock)
{
    std::default_random_engine generatorLocalComputation;
    std::default_random_engine generatorCSComputation;
    std::exponential_distribution<double> distributionLocalComputation(1 / alpha);
    std::exponential_distribution<double> distributionCSComputation(1 / beta);

    long long int sysTime;
    Time requestCSTime;

    printf("INFO :: Node %d: working -> Starting Critical Section Simulations\n", myID);

    for (int i = 1; i <= mutualExclusionCounts; i++)
    {
        int outCSTime = distributionLocalComputation(generatorLocalComputation);
        int inCSTime = distributionCSComputation(generatorCSComputation);

        sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d is doing local computation at %lld\n", myID, sysTime);
        fflush(fp);
        sleep(outCSTime);

        sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d requests to enter CS for the %dst time at %lld\n", myID, i, sysTime);
        fflush(fp);

        requestCSTime = std::chrono::system_clock::now();
        requestCS(myID, inCS, tokenHere, logicalClock, neighbors, start, fp, lock);
        totalResponseTime += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - requestCSTime).count();

        sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d ENTERS CS for the %dst time at %lld\n", myID, i, sysTime);
        fflush(fp);
        sleep(inCSTime);

        sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
        fprintf(fp, "%d EXITS CS for the %dst time at %lld\n", myID, i, sysTime);
        fflush(fp);
        exitCS(myID, inCS, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr, numNodes, start, fp, lock);
    }
    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
    fprintf(fp, "%d completed all %d transactions at %lld\n", myID, mutualExclusionCounts, sysTime);
    fflush(fp);

    // node is alive as long as all processes have finished
    // other nodes might need routing of privilege across completed nodes

    // informing all other nodes of termination
    TerminateMessage terminateMessage;
    terminateMessage.senderID = myID;
    terminateMessage.type = TERMINATE;

    for(int i = 0; i < numNodes; i++)
    {
        if(i != myID) 
        {
            sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
            fprintf(fp, "%d sends TERMINATE to %d at %lld\n", myID, i, sysTime);
            fflush(fp);
            if (sendMessage(myID, i, terminateMessage) == false)
            {
                printf("ERROR :: Node %d: working -> Could not send TERMINATE to %d\n", myID, i);
                exit(EXIT_FAILURE);
            }
        }
    }

    finishedProcessesCount++;
    while (finishedProcessesCount < numNodes)
    {
        lock->lock();
        if (tokenHere == TRUE)
        {
            lock->unlock();
            exitCS(myID, inCS, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr, numNodes, start, fp, lock);
        }
        else
        {
            lock->unlock();            
        }
        // this_thread::sleep_for(chrono::milliseconds(10));
    }
    sysTime =std:: chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
    fprintf(fp, "%d finished any pending transactions at %lld\n", myID, sysTime);
    fflush(fp);
}

/**
 * @brief receiver function for receiver thread
 * 
 * @param myID 
 * @param myPort 
 * @param inCS 
 * @param tokenHere 
 * @param logicalClock 
 * @param reqArray 
 * @param neighbors 
 * @param sharedTokenPtrPtr 
 * @param numNodes 
 * @param start 
 * @param fp 
 * @param lock 
 */
void receiveMessage(const int myID, const int myPort, int &inCS, int &tokenHere, LLONG &logicalClock, 
                    std::map<int, RequestArrayNode> &reqArray, std::atomic<int> &finishedProcessesCount,
                    std::vector<int> &neighbors, Token **sharedTokenPtrPtr,
                    const int numNodes, const Time &start, FILE *fp, std::mutex *lock)
{
    struct sockaddr_in client;
    socklen_t len = sizeof(struct sockaddr_in);

    int maxRecvBufferLen = std::max(sizeof(RequestMessage),sizeof(Token));
    char recvBuffer[maxRecvBufferLen];

    Token token;

    Time now;
    int clientSockfd;
    long long int sysTime;
    long long int receivedTokenMessages = 0, receivedRequestMessages = 0;

    int sockfd = createReceiveSocket(myID, myPort);

    printf("INFO :: Node %d: receiveMessage -> Started listening for connections\n", myID);
    while (finishedProcessesCount < numNodes)
    {
        if ((clientSockfd = accept(sockfd, (struct sockaddr *)&client, &len)) >= 0)
        {
            // receiving message from client
            int data_len = recv(clientSockfd, recvBuffer, maxRecvBufferLen, 0);
            if (data_len > 0)
            {
                now = std::chrono::system_clock::now();

                // determining message type
                RequestMessage *requestMessage = reinterpret_cast<RequestMessage *>(recvBuffer);
                Token *tokenMessage = reinterpret_cast<Token *>(recvBuffer);
                TerminateMessage* terminateMessage = reinterpret_cast<TerminateMessage *>(recvBuffer);

                int msgType = tokenMessage->type;

                switch (msgType)
                {
                case TOKEN:
                    totalReceivedMessages++;
                    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                    fprintf(fp, "%d receives TOKEN from %d at %lld\n", myID, tokenMessage->senderID, sysTime);
                    fflush(fp);

                    token = *tokenMessage;

                    lock->lock();

                    token.senderID = myID;
                    *sharedTokenPtrPtr = &token; // to share token with sender thread
                    receiveToken(myID, inCS, tokenHere, reqArray, sharedTokenPtrPtr, numNodes, start, fp);

                    lock->unlock();
                    break;

                case REQUEST:
                    totalReceivedMessages++;
                    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                    fprintf(fp, "%d received REQUEST from %d with alreadySeen as |%s| at %lld\n", myID, requestMessage->senderID, requestMessage->alreadySeen, sysTime);
                    fflush(fp);

                    lock->lock();

                    receiveRequest(myID, inCS, tokenHere, logicalClock, reqArray, requestMessage, neighbors, sharedTokenPtrPtr, numNodes, start, fp);

                    lock->unlock();
                    break;

                case TERMINATE:
                    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                    fprintf(fp, "%d received TERMINATE from %d at %lld\n", myID, terminateMessage->senderID, sysTime);
                    fflush(fp);
                    finishedProcessesCount++;
                    break;

                default:
                    printf("ERROR :: Node %d: receiveMessage -> Invalid Message Type %d\n", myID, msgType);
                }
            }
            close(clientSockfd);
        }
    }
    sysTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start).count();
    fprintf(fp, "%d stopped receiving threads at %lld\n", myID, sysTime);
    fflush(fp);
}

void run(int numNodes, int mutualExclusionCounts, int initialTokenNode, float alpha, float beta, std::vector<std::vector<int>> &topology)
{
    fclose(fopen("output.txt", "w")); // remove file contents
    FILE *fp = fopen("output.txt", "a+");

    // initializing all data structures
    Time start = std::chrono::system_clock::now();

    std::vector<std::thread> workerSenders(numNodes);
    std::vector<std::thread> workerReceivers(numNodes);

    std::vector<int> inCS(numNodes, FALSE);

    std::vector<int> tokenHere(numNodes, FALSE);
    tokenHere[initialTokenNode] = TRUE;

    std::vector<LLONG> logicalClock(numNodes, 0);

    std::vector<std::map<int, RequestArrayNode> > reqArray(numNodes);
    std::vector<Token **> sharedTokenPtrPtr(numNodes, NULL);
    std::vector<Token *> sharedTokenPtr(numNodes, NULL);
    std::vector<std::atomic<int> > finishedProcessesCount(numNodes);

    for (int i = 0; i < numNodes; i++)
    {
        sharedTokenPtrPtr[i] = &sharedTokenPtr[i];
        finishedProcessesCount[i] = ATOMIC_VAR_INIT(0);

        for (int &nbr : topology[i])
        {
            reqArray[i][nbr] = std::list<RequestID>(0);
        }
    }

    std::vector<std::mutex> locks(numNodes);

    Token tokenObj;
    tokenObj.senderID = initialTokenNode;
    tokenObj.type = messageType::TOKEN;
    tokenObj.elecID = initialTokenNode;

    for (int i = 0; i < MAX_NODES; i++)
    {
        tokenObj.lud[i] = -1;
    }

    sharedTokenPtr[initialTokenNode] = &tokenObj;

    printf("Creating receiver threads\n");

    // starting the receiver threads
    for (int i = 0; i < numNodes; i++)
    {
        workerReceivers[i] = std::thread(receiveMessage, i, startPort + i,
                                         std::ref(inCS[i]), std::ref(tokenHere[i]), std::ref(logicalClock[i]), std::ref(reqArray[i]),
                                         std::ref(finishedProcessesCount[i]), std::ref(topology[i]), sharedTokenPtrPtr[i], numNodes,
                                         ref(start), fp, &locks[i]);
    }

    // wait for receiver threads to start listening on sockets
    std::this_thread::sleep_for(std::chrono::seconds(1));

    printf("Creating CS executor threads\n");

    // starting the sender threads
    for (int i = 0; i < numNodes; i++)
    {
        workerSenders[i] = std::thread(working, i,
                                       std::ref(inCS[i]), std::ref(tokenHere[i]), std::ref(logicalClock[i]), std::ref(reqArray[i]),
                                       std::ref(topology[i]), sharedTokenPtrPtr[i], numNodes, std::ref(finishedProcessesCount[i]),
                                       mutualExclusionCounts, alpha, beta, ref(start), fp, &locks[i]);
    }

    for (int i = 0; i < numNodes; i++)
    {
        workerSenders[i].join();
        workerReceivers[i].join();
    }

    float averageMessagesExchanged = totalReceivedMessages / (numNodes * mutualExclusionCounts * 1.0);
    float averageResponseTime = totalResponseTime / (1000.0 * numNodes * mutualExclusionCounts);
    std::cout << "\n\nAnalysis:\n\tTotal Messages Exchanged:  " << totalReceivedMessages << "\n\tAverage Messages Exchanged Per CS request: " << averageMessagesExchanged << std::endl;
    std::cout << "\n\tAverage Response Time per CS request: " << averageResponseTime << " milliseconds" << std::endl;
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        std::cout << "\033[1;31mMissing input file path in arguments\033[0m\n";
        exit(EXIT_FAILURE);
    }
    std::ifstream fin(argv[1]);

    if (!fin)
    {
        std::cout << "\033[1;31mError In Opening inp-params.txt\033[0m\n";
        exit(EXIT_FAILURE);
    }

    if (argc < 3)
    {
        startPort = 10000;
    }
    else
    {
        startPort = atoi(argv[2]);
    }

    srand(time(NULL));

    //reading necessary details from the FILE
    int numNodes,
        mutualExclusionCounts,
        initialTokenNode;

    float alpha, beta;

    fin >> numNodes >> mutualExclusionCounts >> initialTokenNode >> alpha >> beta;

    if (numNodes > MAX_NODES) {
        std::cout << "\033[1;31mERROR :: Number of Nodes > MAX_NODES (" << MAX_NODES << ")\033[0m\n";
        exit(EXIT_FAILURE);

    }

    std::string list;
    std::vector<std::vector<int>> topology(numNodes, std::vector<int>(0));
    while (!fin.eof())
    {
        getline(fin, list);
        if (list.size() > 0)
        {
            std::istringstream ss(list);
            std::string word;
            ss >> word;
            int nodeID = std::stoi(word);
            while (true)
            {
                ss >> word;
                if (!ss)
                {
                    break;
                }
                // construct adjacency list
                topology[nodeID].push_back(std::stoi(word));
            }
        }
    }

    fin.close();

    run(numNodes, mutualExclusionCounts, initialTokenNode, alpha, beta, topology);

    return 0;
}