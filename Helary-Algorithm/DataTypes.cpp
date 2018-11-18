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
    int senderID;       // Process ID which sent the request
    int reqOriginId;    // Process ID original request creator

    LLONG reqTime;      // Lamport's logical clock value of the requesting
                        // process at the request creation time.

    char alreadySeen[MAX_LENGTH];
    /* process-IDs delimited by comma. Every process whose ID is in
        alreadySeen has received or is about to receive the request.
        This is used to reduce the message complexity.
    */

} RequestMessage;


/**
 * @brief Token message shared between nodes
 * 
 */
typedef struct Token
{
    enum messageType type;
    int senderID;               // Process ID which sent the token.

    int elecID;                // Process ID the token's final addressee.

    LLONG lud[MAX_NODES];
    /*
        Array whose ith index stores the value that Process_i logical-clock
        had when Process_i gave the token to another process.
    */
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