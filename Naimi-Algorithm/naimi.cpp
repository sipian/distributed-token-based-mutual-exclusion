#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
using namespace std;


struct rep {
  double enterTime;
  int num_msgs;
};


struct token_msg{
  int isTerm;
  int isToken;
  int reqProcess;
};

struct msg {
  int isTerm;
  int priv_or_req;
  int dest;
  int seq_num;
  int l;
  int deq[15];
  int ln[11];
};

// Useful message differentiators
#define DATA 0
#define REQUEST 0
#define TOKEN 1
#define NIL_PROCESS -1
#define TERMINATE -1
#define PRIVILEDGE -2
// #define REQUEST -3
#define TIMEOUT_TIME 20

// Some handy global variables
int idx,t;
int world_rank,world_size;
vector<int> arr[11],spantree[11];
int ln[11], rn[11];
pthread_mutex_t m_sendrec;
FILE *outfile;
MPI_Datatype mpi_msg, mpi_rep,mpi_token_msg;
double start_time1,end_time1,enterTime;
double lambda1,lambda2;
int termCount,priv_msg,req_msg;
bool HavePrivilege, Requesting;
int j,n_seq;
struct msg mss;
struct rep report;
struct token_msg tmsg;
deque<int> deq;
//Specific to Naimi's algorithm
int father;
int next_process;
bool request_cs,token_present;


//Utility for time measurement
double my_clock(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (1.0e-6*t.tv_usec + t.tv_sec);
}

// Barrier function called before entering CS
bool wantToEnter(){
  pthread_mutex_lock(&m_sendrec);
  request_cs = true;
  if(father != NIL_PROCESS){

    tmsg.isTerm = 0;
    tmsg.isToken = REQUEST;
    tmsg.reqProcess = world_rank;
    double actionEnterTime = my_clock();
    fprintf(outfile,"p%d sending request message to its father:p%d at %lf\n",world_rank,father,actionEnterTime - start_time1);
    fflush(outfile);
    MPI_Send(&tmsg,1,mpi_token_msg,father,0,MPI_COMM_WORLD);
    father = NIL_PROCESS;
    req_msg+=1;
  }
  pthread_mutex_unlock(&m_sendrec);

  while(true){
    pthread_mutex_lock(&m_sendrec);
    if(token_present){
        pthread_mutex_unlock(&m_sendrec);
        break;
    }
    pthread_mutex_unlock(&m_sendrec);
  }
}

// Function called after entering CS
bool wantToLeave(){
  pthread_mutex_lock(&m_sendrec);

  request_cs = false;
  if(next_process != NIL_PROCESS){
    tmsg.isTerm = 0;
    tmsg.isToken = TOKEN;
    double actionEnterTime = my_clock();
    fprintf(outfile,"p%d sending its token to its next_process:p%d at %lf\n",world_rank,next_process,actionEnterTime - start_time1);
    fflush(outfile);
    MPI_Send(&tmsg,1,mpi_token_msg,next_process,0,MPI_COMM_WORLD);
    token_present = false;
    next_process = NIL_PROCESS;
    priv_msg+=1;
  }
  pthread_mutex_unlock(&m_sendrec);

}

void work(){
  std::default_random_engine generator1;
  std::exponential_distribution<double> distribution1(1/lambda1);

  std::default_random_engine generator2;
  std::exponential_distribution<double> distribution2(1/lambda2);
  for(int i = 1;i<=t;i++){
      // Waiting for a random amount of time before entering CS
      usleep(distribution1(generator1)*1000);
      double actionEnterTime = my_clock();
      fprintf(outfile,"p%d request to enter CS at %lf for the %dth time\n",world_rank,actionEnterTime - start_time1,i);
      fflush(outfile);

      // Barrier function before entering CS
      wantToEnter();
      actionEnterTime = my_clock();
      fprintf(outfile,"p%d enters CS at %lf\n",world_rank,actionEnterTime - start_time1);
      fflush(outfile);

      // Simulating local computation before entering CS
      enterTime += my_clock()-actionEnterTime;
      usleep(distribution2(generator2)*10000);

      actionEnterTime = my_clock();

      fprintf(outfile,"p%d is doing local computation at %lf\n",world_rank,actionEnterTime - start_time1);
      fflush(outfile);

      // Leaving CS
      actionEnterTime = my_clock();
      fprintf(outfile,"p%d leaves CS at %lf\n",world_rank,actionEnterTime - start_time1);
      fflush(outfile);
      wantToLeave();
  }
  for(int i = 1;i<world_size;i++){
    if(i != world_rank){
      tmsg.isTerm = 1;
      double actionEnterTime = my_clock();
      fprintf(outfile,"p%d sends terminate message to %d at %lf\n",world_rank,i,actionEnterTime - start_time1);
      fflush(outfile);
      MPI_Send(&tmsg,1,mpi_token_msg,i,0,MPI_COMM_WORLD);;
    }
  }
  pthread_mutex_lock(&m_sendrec);
  termCount += 1;
  pthread_mutex_unlock(&m_sendrec);

  while(termCount < world_size-1);
}

void receive(){
  while(termCount < world_size-1){
    int gotData;
    struct token_msg message;
    MPI_Status status;
    MPI_Request request;
    MPI_Irecv(&message,1,mpi_token_msg,MPI_ANY_SOURCE,0,MPI_COMM_WORLD,&request);
    time_t start_time = time(NULL);
    MPI_Test(&request,&gotData,&status);
    while(!gotData && difftime(time(NULL),start_time) < TIMEOUT_TIME)
      MPI_Test(&request,&gotData,&status);

    if(!gotData){
      MPI_Cancel(&request);
      MPI_Request_free(&request);
      fprintf(outfile,"Timeout error\n");
      fflush(outfile);
      continue;
    }

    if(message.isTerm){
      // TODO:
      double actionEnterTime = my_clock();
      fprintf(outfile,"p%d receives terminate message from %d at %lf\n",world_rank,status.MPI_SOURCE,actionEnterTime - start_time1);
      fflush(outfile);
      pthread_mutex_lock(&m_sendrec);
      termCount += 1;
      pthread_mutex_unlock(&m_sendrec);
      if(termCount == world_size-1)break;
    }
    else if(message.isToken == TOKEN){
      double actionEnterTime = my_clock();
      fprintf(outfile,"p%d receives p%d\'s token to enter CS at %lf\n",world_rank,status.MPI_SOURCE,actionEnterTime - start_time1);
      fflush(outfile);


      pthread_mutex_lock(&m_sendrec);
      token_present = true;
      pthread_mutex_unlock(&m_sendrec);

    }
    else if(message.isToken == REQUEST){
      int send_process = message.reqProcess;
      double actionEnterTime = my_clock();
      fprintf(outfile,"p%d receives p%d\'s request to enter CS at %lf\n",world_rank,send_process,actionEnterTime - start_time1);
      fflush(outfile);


      pthread_mutex_lock(&m_sendrec);
      if(father == NIL_PROCESS){
        fprintf(outfile,"p%d has the token\n",world_rank);
        fflush(outfile);
        if(request_cs){
          fprintf(outfile,"p%d is currently requesting for the token\n",world_rank);
          fflush(outfile);
          next_process = send_process;
        }
        else{
          token_present = false;
          tmsg.isToken = TOKEN;
          double actionEnterTime = my_clock();
          fprintf(outfile,"p%d sending token message to the requested process:p%d at %lf\n",world_rank,send_process,actionEnterTime - start_time1);
          fflush(outfile);
          MPI_Send(&tmsg,1,mpi_token_msg,send_process,0,MPI_COMM_WORLD);
          priv_msg += 1;
        }
      }
      else{
        tmsg.isToken = REQUEST;
        tmsg.reqProcess = send_process;
        double actionEnterTime = my_clock();
        fprintf(outfile,"p%d forwarding request message to its father:p%d at %lf\n",world_rank,father,actionEnterTime - start_time1);
        fflush(outfile);
        MPI_Send(&tmsg,1,mpi_token_msg,father,0,MPI_COMM_WORLD);
        req_msg+=1;
      }
      father = message.reqProcess;
      pthread_mutex_unlock(&m_sendrec);
    }
  }
}

int n,na;
int main(){
  srand(532);
  pthread_mutex_init(&m_sendrec,NULL);
  ifstream infile;
  infile.open("inputs_5.txt");
  infile >> n >> t >> idx >> lambda1 >> lambda2;


  infile.close();
  MPI_Init(NULL, NULL);
  // Get the number of processes
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  // Get the rank of the process
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  string filename = "log_process"+to_string(world_rank)+".txt";
  const char* cfilename = filename.c_str();
  outfile = fopen(cfilename,"w");

  //MPI definition of data type
  /* create a type for struct msg */
  const int nitems=7;
  int blocklengths[7] = {1,1,1,1,1,15,11};
  MPI_Datatype types[7] = {MPI_INT, MPI_INT,  MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
  MPI_Aint     offsets[7];

  offsets[0] = offsetof(struct msg, isTerm);
  offsets[1] = offsetof(struct msg, priv_or_req);
  offsets[2] = offsetof(struct msg, dest);
  offsets[3] = offsetof(struct msg, seq_num);
  offsets[4] = offsetof(struct msg, l);
  offsets[5] = offsetof(struct msg, deq);
  offsets[6] = offsetof(struct msg, ln);

  int x = MPI_Type_create_struct(nitems, blocklengths, offsets, types, &mpi_msg);
  MPI_Type_commit(&mpi_msg);


  //MPI definition of data type
  /* create a type for struct rep */
  const int nitems1=2;
  int blocklengths1[2] = {1,1};
  MPI_Datatype types1[2] = {MPI_DOUBLE, MPI_INT};
  MPI_Aint     offsets1[2];

  offsets1[0] = offsetof(struct rep, enterTime);
  offsets1[1] = offsetof(struct rep, num_msgs);

  int x1 = MPI_Type_create_struct(nitems1, blocklengths1, offsets1, types1, &mpi_rep);
  MPI_Type_commit(&mpi_rep);

  //MPI definition of data type
  /* create a type for struct rep */
  const int nitems2=3;
  int blocklengths2[3] = {1,1,1};
  MPI_Datatype types2[3] = {MPI_INT, MPI_INT, MPI_INT};
  MPI_Aint     offsets2[3];

  offsets2[0] = offsetof(struct token_msg, isTerm);
  offsets2[1] = offsetof(struct token_msg, isToken);
  offsets2[2] = offsetof(struct token_msg, reqProcess);

  int x2 = MPI_Type_create_struct(nitems2, blocklengths2, offsets2, types2, &mpi_token_msg);
  MPI_Type_commit(&mpi_token_msg);


  // For the coordinator process
  if(world_rank == 0){
      double respTime = 0;
      int n_msg = 0;

      // Waiting for all the nodes to share their response time and message complexity
      for(int i = 1;i<world_size;i++){
        MPI_Status status;
        struct rep r;
        MPI_Recv(&r,1,mpi_rep,i,2,MPI_COMM_WORLD,&status);
        respTime += r.enterTime;
        n_msg += r.num_msgs;
      }
      fprintf(outfile,"Response time: %f, message complexity: %f\n",respTime/(world_size-1),n_msg/(double)(world_size-1));
      fflush(outfile);
      MPI_Finalize();
  }
  // For all the node processes in the system
  else{
      // Initialize parameters of the system
      father = idx;
      next_process = NIL_PROCESS;
      request_cs = false;
      token_present = father == idx;
      if(father == world_rank)
        father = NIL_PROCESS;

      cout<<"Process "<<world_rank<<": "<<father<<endl;
      // create two threads, one for working and other being receive thread
      start_time1 = my_clock();
      thread working = thread(work);
      thread recv = thread(receive);

      recv.join();
      working.join();

      pthread_mutex_destroy(&m_sendrec);
      report.enterTime = enterTime;
      report.num_msgs = priv_msg+req_msg;
      MPI_Send(&report,1,mpi_rep,0,2,MPI_COMM_WORLD);
      MPI_Finalize();
      return 0;
  }
  fclose(outfile);
  pthread_mutex_destroy(&m_sendrec);
  return 0;
}