#include <list>
#include <chrono>

#define MAX_NODES 30
#define MAX_LENGTH 100
#define FALSE 0
#define TRUE 1
int startPort;

typedef std::chrono::time_point<std::chrono::system_clock> Time;
typedef unsigned long long int ULLONG_MAX;

enum messageType
{
    TOKEN,
    REQUEST,
    TERMINATE
};

typedef struct RequestMessage
{
    enum messageType type;
    int senderID;
    int reqOriginId;
    ULLONG_MAX reqTime;
    char alreadySeen[MAX_LENGTH]; // process-identifiers delimited by comma
} RequestMessage;

typedef struct Token
{
    enum messageType type;
    int senderID;
    int elecID;                // destination ID
    ULLONG_MAX lud[MAX_NODES]; // array of nodes logical clock
} Token;

typedef struct TerminateMessage
{
    enum messageType type;
    int senderID;
} TerminateMessage;

typedef struct RequestID
{
    int reqOriginId;
    ULLONG_MAX reqTime;
} RequestID;

typedef struct RequestArrayNode
{
    int neighborID;
    std::list<RequestID> requests;
} RequestArrayNode;
