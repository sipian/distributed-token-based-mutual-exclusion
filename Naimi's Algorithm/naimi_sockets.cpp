#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
using namespace std;


// Typedef for chrono based time module
typedef chrono::time_point<chrono::system_clock> Time;

// enum for different kinds of messages
enum messageType{
  PRIVILEGE,
  REQUEST,
  TERMINATE
};

// Definition of message structure used in send and receive process
typedef struct Message{
  enum messageType type;
  int senderID;
  int reqProcess;
} Message1;

// Useful constants for Naimi's Algorithm
#define MAX_NODES 30
#define FALSE 0
#define TRUE 1
#define NIL_PROCESS -1

// Define starting port number
int startPort;

// atomic variables used for analysis of the algorithm
std::atomic<int> totalReceivedMessages = ATOMIC_VAR_INIT(0);
std::atomic<long long int> totalResponseTime = ATOMIC_VAR_INIT(0);

// Creates a socket which is used for receiving the messages from other processes
// Arguments:
// @myID : Identifier of the node
// @myPort : Port number on which socket is created
//
// Return Value: the socket identifier
int createReceiveSocket(int myID, int myPort){
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
      exit(1);
  }

  // binding TCP server to IP and port
  if (bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0)
  {
      printf("ERROR :: Node %d: createReceiveSocket -> Error in binding socket to %hu :: Reason %s\n", myID, ntohs(server.sin_port), strerror(errno));
      exit(1);
  }

  // mark it for listening
  if (listen(sockfd, MAX_NODES))
  {
      printf("ERROR :: Node %d: createReceiveSocket -> Error in listening to %hu :: Reason %s\n", myID, ntohs(server.sin_port), strerror(errno));
      exit(1);
  }

  // set socket timeout
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
      printf("ERROR :: Node %d: createReceiveSocket -> Error in creating timeout for TCP connection :: Reason %s\n", myID, strerror(errno));
      exit(1);
  }
  return sockfd;
}


// Helper function to send message to a given node
// Arguments:
// @myID : Identifier of the node
// @dstID : Identifier of the destination node during the send procedure
// @type : type of the message to be sent
// @needsReqProcess : Identifier of the request process when a request message
//                    is sent(Specific to Naimi's algorithm)
//
// Return Value: Boolean whether the message is sent or not
bool sendMessage(int myID, int dstID, messageType type, int needsReqProcess)
{
    // Sender and receiver should not be the same
    assert(myID != dstID);

    // Creating the message to be sent
    Message1 message;
    message.senderID = myID;
    message.type = type;
    // In case a request message is sent, the identifier of the original requested
    // process is also needed, which is never -1
    if(needsReqProcess != -1)
      message.reqProcess = needsReqProcess;

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
        printf("WARN :: Node %d: sendMessage -> Error in sending message-type %s to %d :: Reason - %s\n", myID, ((type == PRIVILEGE) ? "PRIVILEGE" : "REQUEST"), dstID, strerror(errno));
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}


// Barrier function called before entering CS
// Arguments:
// @myID : Identifier of the node
// @request_cs :    Node-specific variable denoting whether it is requesting for
//                  cs or not
// @next_process :  Node-specific variable denoting the next process to which
//                  the token is sent after completing our CS
// @father :        Node-specific variable denoting the father of the node to
//                  which request messages are sent
// @token_present : Node-specific variable denoting whether token is present to
//                  enter CS
// @outfile :       File pointer to the file where the log messages are written
// @m_sendrec :     mutex lock used to guard variables between work() and
//                  receive() threads
// @start :         Reference point to measure time used in logging
//
// Return Value: Void
bool wantToEnter(int myID, int &request_cs, int &next_process, int &father, int &token_present, FILE *outfile, pthread_mutex_t *m_sendrec, Time &start){
  pthread_mutex_lock(m_sendrec);
  request_cs = TRUE;
  if(father != NIL_PROCESS){

    long long int sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
    fprintf(outfile,"p%d sending request message to its father:p%d at %lld\n",myID,father,sysTime);
    fflush(outfile);
    if(sendMessage(myID, father, REQUEST, myID) == false) {
        printf("ERROR :: Node %d: assignPrivilege -> Unable to send REQUEST to %d\n", myID, father);
        exit(1);
    }
    father = NIL_PROCESS;
  }
  pthread_mutex_unlock(m_sendrec);

  while(true){
    pthread_mutex_lock(m_sendrec);
    if(token_present){
        pthread_mutex_unlock(m_sendrec);
        break;
    }
    pthread_mutex_unlock(m_sendrec);
  }
}

// Function called after entering CS
// Arguments:
// @myID : Identifier of the node
// @request_cs :    Node-specific variable denoting whether it is requesting for
//                  cs or not
// @next_process :  Node-specific variable denoting the next process to which
//                  the token is sent after completing our CS
// @father :        Node-specific variable denoting the father of the node to
//                  which request messages are sent
// @token_present : Node-specific variable denoting whether token is present to
//                  enter CS
// @outfile :       File pointer to the file where the log messages are written
// @m_sendrec :     mutex lock used to guard variables between work() and
//                  receive() threads
// @start :         Reference point to measure time used in logging
//
// Return Value: Void
bool wantToLeave(int myID, int &request_cs, int &next_process, int &father, int &token_present, FILE *outfile, pthread_mutex_t *m_sendrec, Time &start){
  pthread_mutex_lock(m_sendrec);

  request_cs = FALSE;
  if(next_process != NIL_PROCESS){
    long long int sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
    fprintf(outfile,"p%d sending its token to its next_process:p%d at %lld\n",myID,next_process,sysTime);
    fflush(outfile);
    if(sendMessage(myID, next_process, PRIVILEGE,-1) == false) {
        printf("ERROR :: Node %d: assignPrivilege -> Unable to send PRIVILEGE to %d\n", myID, father);
        exit(1);
    }
    token_present = FALSE;
    next_process = NIL_PROCESS;
  }
  pthread_mutex_unlock(m_sendrec);

}

// The working thread which tries to simulate the critical section algorithm
// using Naimi's algorithm
//
// Arguments:
// @myID :          Identifier of the node
// @father :        Node-specific variable denoting the father of the node to
//                  which request messages are sent
// @next_process :  Node-specific variable denoting the next process to which
//                  the token is sent after completing our CS
// @request_cs :    Node-specific variable denoting whether it is requesting for
//                  cs or not
// @token_present : Node-specific variable denoting whether token is present to
//                  enter CS
// @noOfNodes :     Total number of nodes present in the node setup
// @mutexCounts :   Number of times this thread enters the CS
//
// @finishedProcessesCount :   atomic variable which is used in the termination
//                             phase
// @lambda1 :       variable which is used for simulating time spent before
//                  entering CS
// @lambda2 :       variable which is used for simulating time spent inside CS
// @start :         Reference point to measure time used in logging
//
// @outfile :       File pointer to the file where the log messages are written
// @m_sendrec :     mutex lock used to guard variables between work() and
//                  receive() threads
// Return Value: Void
void work(int myID, int &father, int &next_process, int &request_cs, int &token_present, int noOfNodes, int mutexCounts,
          atomic<int> &finishedProcessesCount, double lambda1, double lambda2, Time &start, FILE *outfile, pthread_mutex_t *m_sendrec){
  std::default_random_engine generator1;
  std::exponential_distribution<double> distribution1(1/lambda1);

  std::default_random_engine generator2;
  std::exponential_distribution<double> distribution2(1/lambda2);

  long long int sysTime;
  Time requestCSTime;

  printf("INFO :: Node %d: working -> Starting Critical Section Simulations\n", myID);

  for(int i = 1; i<=mutexCounts; i++){
      // Waiting for a random amount of time before entering CS
      int outCSTime = distribution1(generator1);
      int inCSTime = distribution2(generator2);

      sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d request to enter CS at %lld for the %dth time\n",myID,sysTime,i);
      fflush(outfile);
      sleep(outCSTime);


      // Barrier function before entering CS
      requestCSTime = chrono::system_clock::now();
      wantToEnter(myID,request_cs,next_process,father, token_present, outfile,m_sendrec,start);
      totalResponseTime += totalResponseTime += chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - requestCSTime).count();;

      sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d enters CS at %lld\n",myID,sysTime);
      fflush(outfile);

      // Simulating local computation before entering CS
      sleep(inCSTime);

      sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d is doing local computation at %lld\n",myID,sysTime);
      fflush(outfile);

      // Leaving CS
      sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d leaves CS at %lld\n",myID,sysTime);
      fflush(outfile);
      wantToLeave(myID,request_cs,next_process,father,token_present,outfile,m_sendrec,start);
  }

  sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
  fprintf(outfile, "%d completed all %d transactions at %lld\n", myID, mutexCounts, sysTime);
  fflush(outfile);



  // Initiating the terimation part of the process, inform all the nodes about this
  for(int i = 0;i<noOfNodes;i++){
    if(i != myID){
      sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
      fprintf(outfile,"p%d sends terminate message to %d at %lld\n",myID,i,sysTime);
      fflush(outfile);
      if (sendMessage(myID, i, TERMINATE,-1) == false)
      {
          printf("ERROR :: Node %d: working -> Could not send TERMINATE to %d\n", myID, i);
          exit(EXIT_FAILURE);
      }
    }
  }

  pthread_mutex_lock(m_sendrec);
  finishedProcessesCount += 1;
  pthread_mutex_unlock(m_sendrec);

  // Wait till we get messages from all other processes
  while(finishedProcessesCount < noOfNodes);
  sysTime = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - start).count();
  fprintf(outfile, "%d finished any pending transactions at %lld\n", myID, sysTime);
  fflush(outfile);
}

// The receive thread which tries to receive the messages directed to this node
//
// Arguments:
// @myID :          Identifier of the node
// @myPort :        Port number on which the receive socket is created
// @father :        Node-specific variable denoting the father of the node to
//                  which request messages are sent
// @next_process :  Node-specific variable denoting the next process to which
//                  the token is sent after completing our CS
// @request_cs :    Node-specific variable denoting whether it is requesting for
//                  cs or not
// @token_present : Node-specific variable denoting whether token is present to
//                  enter CS
// @finishedProcessesCount :   atomic variable which is used in the termination
//                             phase
// @lambda1 :       variable which is used for simulating time spent before
//                  entering CS
// @lambda2 :       variable which is used for simulating time spent inside CS
// @start :         Reference point to measure time used in logging
//
// @outfile :       File pointer to the file where the log messages are written
// @m_sendrec :     mutex lock used to guard variables between work() and
//                  receive() threads
// Return Value: Void
void receive(int myID, int myPort, int noOfNodes, int &father, int &next_process, int &request_cs, int &token_present,
             atomic<int> &finishedProcessesCount, Time &start, FILE *outfile, pthread_mutex_t *m_sendrec){

  struct sockaddr_in client;
  socklen_t len = sizeof(struct sockaddr_in);

  Message1 message;
  Time now;
  int clientId;
  int send_process;
  long long int sysTime;
  double actionEnterTime;

  int sockfd = createReceiveSocket(myID, myPort);
  printf("INFO :: Node %d: receiveMessage -> Started listening for connections\n", myID);

  while(finishedProcessesCount < noOfNodes){

    if ((clientId = accept(sockfd, (struct sockaddr *)&client, &len)) >= 0){
      int data_len = recv(clientId, (char *)&message, sizeof(message), 0);
      if (data_len > 0){
        now = chrono::system_clock::now();
        totalReceivedMessages++;
        switch (message.type){
          case PRIVILEGE:
            sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
            fprintf(outfile,"p%d receives p%d\'s token to enter CS at %lld\n",myID,message.senderID,sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            token_present = TRUE;
            pthread_mutex_unlock(m_sendrec);
            break;

          case REQUEST:
            send_process = message.reqProcess;
            sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
            fprintf(outfile,"p%d receives p%d\'s request to enter CS at %lld\n",myID,send_process,sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            if(father == NIL_PROCESS){
              fprintf(outfile,"p%d has the token\n",myID);
              fflush(outfile);
              if(request_cs){
                fprintf(outfile,"p%d is currently requesting for the token\n",myID);
                fflush(outfile);
                next_process = send_process;
              }
              else{
                token_present = FALSE;
                sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
                fprintf(outfile,"p%d sending token message to the requested process:p%d at %lld\n",myID,send_process,sysTime);
                fflush(outfile);
                if (sendMessage(myID, send_process, PRIVILEGE,-1) == false)
                {
                    printf("ERROR :: Node %d: working -> Could not send PRIVILEDGE to %d\n", myID, send_process);
                    exit(EXIT_FAILURE);
                }
              }
            }
            else{
              sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
              fprintf(outfile,"p%d forwarding request message to its father:p%d at %lld\n",myID,father,sysTime);
              fflush(outfile);
              if (sendMessage(myID, father, REQUEST, send_process) == false)
              {
                  printf("ERROR :: Node %d: working -> Could not send REQUEST to %d\n", myID, father);
                  exit(EXIT_FAILURE);
              }
            }
            father = message.reqProcess;
            pthread_mutex_unlock(m_sendrec);
            break;

          case TERMINATE:
            sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
            fprintf(outfile,"p%d receives terminate message from %d at %lld\n",myID,message.senderID,sysTime);
            fflush(outfile);
            pthread_mutex_lock(m_sendrec);
            finishedProcessesCount += 1;
            pthread_mutex_unlock(m_sendrec);
            break;

          default:
            printf("ERROR :: Node %d: receiveMessage -> Invalid Message Type %d\n", myID, message.type);
        }
      }
    }
  }
  sysTime = chrono::duration_cast<chrono::microseconds>(now - start).count();
  fprintf(outfile, "%d stopped receiving threads at %lld\n", myID, sysTime);
  fflush(outfile);
}

// Function which spawn's n number of work() and receive() processes simulating
// a n-node distributed setup
//
// Arguments:
// @noOfNodes :     Number of nodes used in the distributed setting
// @mutualExclusionCounts : Number of times CS is accessed by every node
// @initialTokenNode : Node identifier which initially holds the token
// @alpha :         lambda value used to generate random local computation times
// @beta :          lambda value used to generate random CS execution times
//
// Return Value: Void
void run(int noOfNodes, int mutualExclusionCounts, int initialTokenNode, double alpha, double beta)
{
    fclose(fopen("output.txt", "w")); // remove file contents
    FILE *fp = fopen("output.txt", "a+");
    FILE *file_pointers[noOfNodes];
    vector<pthread_mutex_t> locks(noOfNodes);

    for(int i  = 0;i<noOfNodes; i++){
      pthread_mutex_init(&locks[i],NULL);
    }
    Time start = chrono::system_clock::now();
    vector<thread> workerSenders(noOfNodes);
    vector<thread> workerReceivers(noOfNodes);

    // since graph is completely connected the spanning tree is a tree with just 1 level and rooted at initialTokenNode
    vector<int> father(noOfNodes, initialTokenNode);
    father[initialTokenNode] = NIL_PROCESS;
    vector<int> next_process(noOfNodes, NIL_PROCESS);
    vector<int> request_cs(noOfNodes, FALSE);
    vector<int> token_present(noOfNodes, FALSE);
    token_present[initialTokenNode] = TRUE;
    vector<atomic<int> > finishedProcessesCount(noOfNodes);


    printf("Creating receiver threads\n");

    // starting the receiver threads
    for (int i = 0; i < noOfNodes; i++)
    {
        finishedProcessesCount[i] = ATOMIC_VAR_INIT(0);

        workerReceivers[i] = thread(receive, i, startPort + i, noOfNodes,
            ref(father[i]), ref(next_process[i]), ref(request_cs[i]), ref(token_present[i]),
            ref(finishedProcessesCount[i]), ref(start), fp, &locks[i]);
    }
    usleep(1000000);

    printf("Creating CS executor threads\n");


    // starting the sender threads
    for (int i = 0; i < noOfNodes; i++)
    {
        workerSenders[i] = thread(work, i,
            ref(father[i]), ref(next_process[i]), ref(request_cs[i]), ref(token_present[i]), noOfNodes,
            mutualExclusionCounts, ref(finishedProcessesCount[i]), alpha, beta, ref(start), fp, &locks[i]);
    }

    for (int i = 0; i < noOfNodes; i++)
    {
        workerSenders[i].join();
        workerReceivers[i].join();
    }

    // Gathering the results with respect to average number of messages and
    // response times to enter CS
    float averageMessagesExchanged = totalReceivedMessages / (noOfNodes*1.0);
    float averageResponseTime = totalResponseTime / (1000.0 * noOfNodes * mutualExclusionCounts);

    cout << "\n\nAnalysis:\n\tTotal Messages Exchanged:  " << totalReceivedMessages << "\n\tAverage Messages Exchanged: " << averageMessagesExchanged << endl;
    cout <<"\n\tAverage Response Time: " << averageResponseTime << " milliseconds" << endl;

    for(int i  = 0;i<noOfNodes; i++)
      pthread_mutex_destroy(&locks[i]);

}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "\033[1;31mMissing input file path in arguments\033[0m\n";
        exit(EXIT_FAILURE);
    }

    // Taking the file as input from command line
    ifstream fin(argv[1]);

    if (!fin)
    {
        cout << "\033[1;31mError In Opening inp-params.txt\033[0m\n";
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
    int noOfNodes,
        mutualExclusionCounts,
        initialTokenNode;
    double alpha,
         beta;

    fin >> noOfNodes >> mutualExclusionCounts >> initialTokenNode >> alpha >> beta;
    fin.close();

    // Simulate Naimi's algorithm
    run(noOfNodes, mutualExclusionCounts, initialTokenNode, alpha, beta);

    return 0;
}