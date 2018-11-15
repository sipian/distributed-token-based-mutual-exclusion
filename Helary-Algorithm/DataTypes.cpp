#include <list>
#include <chrono>

#define MAX_NODES 50
#define MAX_LENGTH 500
#define FALSE 0
#define TRUE 1
int startPort;

typedef std::chrono::time_point<std::chrono::system_clock> Time;
typedef long long int LLONG;

/**
 * @brief message type to ascertain the struct the received message should be typecasted to
 * 
 */
enum messageType
{
    TOKEN,
    REQUEST,
    TERMINATE
};

/**
 * @brief Request message broadcasted by a node
 * 
 */
typedef struct RequestMessage
{
    enum messageType type;
    int senderID;
    int reqOriginId;
    LLONG reqTime;
    char alreadySeen[MAX_LENGTH]; // process-identifiers delimited by comma
} RequestMessage;

/**
 * @brief Token message shared between nodes
 * 
 */
typedef struct Token
{
    enum messageType type;
    int senderID;
    int elecID;                // destination ID
    LLONG lud[MAX_NODES]; // array of nodes logical clock
} Token;

typedef struct TerminateMessage
{
    enum messageType type;
    int senderID;
} TerminateMessage;

/**
 * @brief element of the request array
 * 
 */
typedef struct RequestID
{
    int reqOriginId;
    LLONG reqTime;
} RequestID;

typedef std::list<RequestID> RequestArrayNode;