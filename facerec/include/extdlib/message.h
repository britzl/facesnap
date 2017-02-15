#ifndef DM_MESSAGE_H
#define DM_MESSAGE_H

#include <stdint.h>
#include <dlib/hash.h>
#include <string.h>

namespace dmMessage
{
    /**
     * Result enum
     */
    enum Result
    {
        RESULT_OK = 0,                      //!< RESULT_OK
        RESULT_SOCKET_EXISTS = -1,          //!< RESULT_SOCKET_EXISTS
        RESULT_SOCKET_NOT_FOUND = -2,       //!< RESULT_SOCKET_NOT_FOUND
        RESULT_SOCKET_OUT_OF_RESOURCES = -3,//!< RESULT_SOCKET_OUT_OF_RESOURCES
        RESULT_INVALID_SOCKET_NAME = -4,    //!< RESULT_INVALID_SOCKET_NAME
        RESULT_MALFORMED_URL = -5           //!< RESULT_MALFORMED_URL
    };

    /**
     * Socket handle
     */
    typedef uint32_t HSocket;

    /**
     * URL specifying a receiver of messages
     */
    struct URL
    {
        URL()
        {
            memset(this, 0, sizeof(URL));
        }
        /// Socket
        HSocket     m_Socket;
        /// Lua function reference for callback dispatching.
        /// The value is ref + 2 as LUA_NOREF is defined as -2 as
        /// we the convention of zero for default values.
        /// It's unfortunate that we have to add lua related
        /// functionality here though.
        int         m_Function;
        /// Path of the receiver
        dmhash_t    m_Path;
        /// Fragment of the receiver
        dmhash_t    m_Fragment;
    };

    struct StringURL
    {
        StringURL()
        {
            memset(this, 0, sizeof(StringURL));
        }
        /// Socket part of the URL, not null-terminated
        const char* m_Socket;
        /// Size of m_Socket
        uint32_t m_SocketSize;
        /// Path part of the URL, not null-terminated
        const char* m_Path;
        /// Size of m_Path
        uint32_t m_PathSize;
        /// Fragment part of the URL, not null-terminated
        const char* m_Fragment;
        /// Size of m_Fragment
        uint32_t m_FragmentSize;
    };

    /**
     * Message data desc used at dispatch callback. When a message is posted,
     * the actual object is copied into the sockets internal buffer.
     */
    struct Message
    {
        URL             m_Sender;       //! Sender uri
        URL             m_Receiver;     //! Receiver uri
        dmhash_t        m_Id;           //! Unique id of message
        uintptr_t       m_UserData;     //! User data pointer
        uintptr_t       m_Descriptor;   //! User specified descriptor of the message data
        uint32_t        m_DataSize;     //! Size of userdata in bytes
        struct Message* m_Next;         //! Ptr to next message (or 0 if last)
        uint8_t         m_Data[0];      //! Payload
    };

    /**
     * @see #Dispatch
     */
    typedef void(*DispatchCallback)(dmMessage::Message *message, void* user_ptr);

    /**
     * Create a new socket
     * @param name Socket name. Its length must be more than 0 and it cannot contain the characters '#' or ':' (@see ParseURL)
     * @param socket Socket handle (out value)
     * @return RESULT_OK on success
     */
    Result NewSocket(const char* name, HSocket* socket);

    /**
     * Delete a socket
     * @note  The socket must not have any pending messages
     * @param socket Socket to delete
     * @return RESULT_OK if the socket was deleted
     */
    Result DeleteSocket(HSocket socket);

    /**
     * Get socket by name
     * @param name Socket name
     * @param out_socket The socket as an out-parameter. The handle pointed to is only written to if the function is successfull
     * @return RESULT_OK if the socket was found
     */
    Result GetSocket(const char *name, HSocket* out_socket);

    /**
     * Get socket name
     * @param socket Socket
     * @return socket name, 0x0 if it was not found
     */
    const char* GetSocketName(HSocket socket);

    /**
     * Tests if a socket is valid (not deleted).
     * @param socket Socket
     * @return if the socket is valid or not
     */
    bool IsSocketValid(HSocket socket);

    /**
     * Test if a socket has any messages
     * @param socket Socket
     * @return if the socket has messages or not
     */
    bool HasMessages(HSocket socket);

    /**
     * Resets the given URL to default values.
     * @note Previously the URL wasn't reset in the constructor and certain calls
     *       to ResetURL might currently be redundant
     * @param url URL to reset
     */
    void ResetURL(const URL& url);

    /**
     * Post an message to a socket
     * @note Message data is copied by value
     * @param sender The sender URL if the receiver wants to respond. 0x0 is accepted
     * @param receiver The receiver URL, must not be 0x0
     * @param message_id Message id
     * @param user_data User data that can be used when both the sender and receiver are known
     * @param descriptor User specified descriptor of the message data
     * @param message_data Message data reference
     * @param message_data_size Message data size in bytes
     * @return RESULT_OK if the message was posted
     */
    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data, uintptr_t descriptor, const void* message_data, uint32_t message_data_size);

    /**
     * Dispatch messages
     * @note When dispatched, the messages are considered destroyed. Messages posted during dispatch
     *       are handled in the next invocation to #Dispatch
     * @param socket Socket handle of the socket of which messages to dispatch.
     * @param dispatch_callback Callback function that will be called for each message
     *        dispatched. The callbacks parameters contains a pointer to a unique Message
     *        for this post and user_ptr is the same as Dispatch called user_ptr.
     * @return Number of dispatched messages
     */
    uint32_t Dispatch(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr);

    /**
     * Dispatch messages blocking. The function will return as soon at least one message
     * is dispatched.
     * See Dispatch() for additional information
     * @param socket socket
     * @param dispatch_callback dispatch callback
     * @param user_ptr user data
     * @return Number of dispatched messages
     */
    uint32_t DispatchBlocking(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr);

    /**
     * Consume all pending messages
     * @param socket Socket handle
     * @return Number of dispatched messages
     */
    uint32_t Consume(HSocket socket);

    /**
     * Convert a string to a URL struct
     * @param uri string of the format [socket:][path][#fragment]
     * @param parsed url in string format, must not be 0x0
     * @return
     * - RESULT_OK on success
     * - RESULT_MALFORMED_URL if the uri could not be parsed
     */
    Result ParseURL(const char* uri, StringURL* out_url);
};

#endif // DM_MESSAGE_H
