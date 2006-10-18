/*
 * Socket.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 * Copyright (C) 2005 by Christoph Mueller (christoph.mueller@vis.uni-stuttgart.de). All rights reserved.
 */

#ifndef VISLIB_SOCKET_H_INCLUDED
#define VISLIB_SOCKET_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */

#ifdef _WIN32
#include <Winsock2.h>
#else /* _WIN32 */
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define SOCKET int
#define INVALID_SOCKET (-1)
#endif /* !_WIN32 */

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32")
#endif /* _MSC_VER */


#include "vislib/SocketAddress.h"
#include "vislib/types.h"


namespace vislib {
namespace net {

    /**
     * This is a raw socket wrapper class that allows platform-independent use
     * of socket functionality on Windows and Linux systems.
     *
     * @author Christoph Mueller
     */
    class Socket {

    public:

        /** The supported protocols. */
        enum Protocol {
            PROTOCOL_IP = IPPROTO_IP,				// IPv4.
            PROTOCOL_HOPOPTS = IPPROTO_HOPOPTS,		// IPv6 hop-by-hop options.
            PROTOCOL_ICMP = IPPROTO_ICMP,           // Control message protocol.
            PROTOCOL_IGMP = IPPROTO_IGMP,           // Internet group mgmnt.
            //PROTOCOL_GGP = IPPROTO_GGP,             // Gateway^2 (deprecated).
#ifdef _WIN32
            PROTOCOL_IPV4 = IPPROTO_IPV4,           // IPv4.
#endif /* _WIN32 */
            PROTOCOL_TCP = IPPROTO_TCP,             // TCP.
            PROTOCOL_PUP = IPPROTO_PUP,             // PUP.
            PROTOCOL_UDP = IPPROTO_UDP,             // User Datagram Protocol.
            PROTOCOL_IDP = IPPROTO_IDP,             // XNS IDP.
            PROTOCOL_IPV6 = IPPROTO_IPV6,           // IPv6.
            PROTOCOL_ROUTING = IPPROTO_ROUTING,     // IPv6 routing header.
            PROTOCOL_FRAGMENT = IPPROTO_FRAGMENT,	// IPv6 fragmentation hdr.
            PROTOCOL_ESP = IPPROTO_ESP,             // IPsec ESP header.
            PROTOCOL_AH = IPPROTO_AH,               // IPsec AH.
            PROTOCOL_ICMPV6 = IPPROTO_ICMPV6,       // ICMPv6.
            PROTOCOL_NONE = IPPROTO_NONE,           // IPv6 no next header.
            PROTOCOL_DSTOPTS = IPPROTO_DSTOPTS,     // IPv6 destination options.
            //PROTOCOL_ND = IPPROTO_ND,             // UNOFFICIAL net disk prot.
            //PROTOCOL_ICLFXBM = IPPROTO_ICLFXBM,
            PROTOCOL_RAW = IPPROTO_RAW              // Raw IP packet.
        };


        /** The supported protocol families. */
        enum ProtocolFamily {
            FAMILY_UNSPEC = PF_UNSPEC,		// Unspecified.
            //FAMILY_UNIX = PF_UNIX,
            FAMILY_INET = PF_INET,			// UDP, TCP etc. version 4.
            //FAMILY_IMPLINK = PF_IMPLINK,
            //FAMILY_PUP = PF_PUP,
            //FAMILY_CHAOS = PF_CHAOS,
            //FAMILY_NS = PF_NS,
            //FAMILY_IPX = PF_IPX,
            //FAMILY_ISO = PF_ISO,
            //FAMILY_OSI = PF_OSI,
            //FAMILY_ECMA = PF_ECMA,
            //FAMILY_DATAKIT = PF_DATAKIT,
            //FAMILY_CCITT = PF_CCITT,
            //FAMILY_SNA = PF_SNA,
            //FAMILY_DECnet = PF_DECnet,
            //FAMILY_DLI = PF_DLI,
            //FAMILY_LAT = PF_LAT,
            //FAMILY_HYLINK = PF_HYLINK,
            //FAMILY_APPLETALK = PF_APPLETALK,
            //FAMILY_VOICEVIEW = PF_VOICEVIEW,
            //FAMILY_FIREFOX = PF_FIREFOX,
            //FAMILY_UNKNOWN1 = PF_UNKNOWN1,
            //FAMILY_BAN = PF_BAN,
            //FAMILY_ATM = PF_ATM,
            FAMILY_INET6 = PF_INET6			// Internet version 6.
        };


        /** 
         * Flag that describes what types of operation will no longer be 
         * allowed when calling Shutdown() on a socket. 
         */
        enum ShutdownManifest {
#ifdef _WIN32
            BOTH = SD_BOTH,                 // Shutdown send and receive.
            RECEIVE = SD_RECEIVE,           // Shutdown receive only.
            SEND = SD_SEND                  // Shutdown sent only
#else /* _WIN32 */
            BOTH = SHUT_RDWR,
            RECEIVE = SHUT_RD,
            SEND = SHUT_WR
#endif /* _WIN32 */
        };


        /** The supported socket types. */
        enum Type {
            TYPE_STREAM = SOCK_STREAM,      // Stream socket.
            TYPE_DGRAM = SOCK_DGRAM,        // Datagram socket.
            TYPE_RAW = SOCK_RAW,            // Raw socket.
            TYPE_RDM = SOCK_RDM,            // Reliably-delivered message.
            TYPE_SEQPACKET = SOCK_SEQPACKET	// Sequenced packet stream.
        };


        /**
         * Cleanup after use of sockets. This method does nothing on Linux. On
         * Windows, it calls WSACleanup. Call this method after you finished 
         * using sockets for releasing the resources associated with the 
         * Winsock-DLL.
         *
         * @throws SocketException In case of an error.
         */
        static void Cleanup(void);

        /**
         * Initialise the use of sockets. This method does nothing on Linux. On
         * Windows, this method calls WSAStartup and initialises the 
         * Winsock-DLL. You must call this method once before using any sockets.
         * Call Cleanup() after you finished the use of sockets.
         *
         * @throws SocketException In case of an error.
         */
        static void Startup(void);

        /**
         * Create an invalid socket. Call Create() on the new object to create 
         * a new socket.
         */
        inline Socket(void) : handle(INVALID_SOCKET) {}

        /**
         * Create socket wrapper from an existing handle.
         *
         * @param handle The socket handle.
         */
        explicit inline Socket(SOCKET handle) : handle(handle) {}

        /**
         * Create a copy of 'rhs'.
         *
         * @param rhs The object to be cloned.
         */
        inline Socket(const Socket& rhs) : handle(rhs.handle) {}

        /** Dtor. */
        virtual ~Socket(void);

        /**
         * Permit incoming connection attempt on the socket.
         *
         * @param outConnAddr Optional pointer to a SocketAddress that receives 
         *                    the address of the connecting entity, as known to 
         *                    the communications layer. The exact format of the 
         *                    address is determined by the address family 
         *                    established when the socket was created. This 
         *                    parameter can be NULL.
         *
         * @return The accepted socket.
         *
         * @throws SocketException If the operation fails
         */
        virtual Socket Accept(SocketAddress *outConnAddr = NULL);

        /**
         * Bind the socket to the specified address.
         *
         * @param address The address to bind.
         *
         * @throws
         */
        virtual void Bind(const SocketAddress& address);

        /**
         * Close the socket. If the socket is not open, i. e. not valid, this 
         * method succeeds, too.
         *
         * @
         */
        virtual void Close(void);

        /**
         * Connect to the specified socket address using this socket.
         *
         * @param address The address to connect to.
         */
        virtual void Connect(const SocketAddress& address);

        /**
         * Create a new socket.
         *
         * @param protocolFamily
         * @param type
         * @param protocol
         *
         * @throws
         */
        virtual void Create(const ProtocolFamily protocolFamily, const Type type, 
            const Protocol protocol);

        /**
         * Retrieve status of transmission and receipt of broadcast messages on 
         * the socket.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetBroadcast(void) const {
            return this->getOption(SOL_SOCKET, SO_BROADCAST);
        }

        /**
         * Answer whether sockets delay the acknowledgment of a connection until
         * after the WSAAccept condition function is called. This method returns
         * always false on Linux.
         *
         * @return The current value of the option, false on Linux.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetConditionalAccept(void) const {
#ifdef _WIN32
            return this->getOption(SOL_SOCKET, SO_CONDITIONAL_ACCEPT);
#else /* _WIN32 */
            return false;
#endif /* _WIN32 */
        }

        /**
         * Answer whether debugging information is recorded.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetDebug(void) const {
            return this->getOption(SOL_SOCKET, SO_DEBUG);
        }

        /**
         * Answer whether routing is disabled.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetDontRoute(const bool enable) const {
            return this->getOption(SOL_SOCKET, SO_DONTROUTE);
        }

        /**
         * Answer whether keep-alives are sent.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetKeepAlive(void) const {
            return this->getOption(SOL_SOCKET, SO_DONTROUTE);
        }

        /**
         * Answer the linger state of the socket
         *
         * @param outLinger Receives the current state.
         *
         * @throws SocketException If the operation fails.
         */
        inline void GetLinger(struct linger& outLinger) const {
            SIZE_T size = sizeof(struct linger);
            this->GetOption(SOL_SOCKET, SO_LINGER, &outLinger, size);
        }

        /**
         * Answer whether OOB data are received in the normal data stream.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetOOBInline(void) const {
            return this->getOption(SOL_SOCKET, SO_OOBINLINE);
        }

        /**
         * Retrieve a socket option.
         *
         * @param level            Level at which the option is defined.
         * @param optName          Socket option for which the value is to be
         *                         retrieved. 
         * @param outValue         Pointer to the buffer in which the value for 
         *                         the requested option is to be returned. 
         * @param inOutValueLength The size of 'outValue'. When the method 
         *                         returns, this variable will contain the 
         *                         number of bytes actually retrieved.
         */
		void GetOption(const INT level, const INT optName, void *outValue,
			SIZE_T& inOutValueLength) const;

        /**
         * Answer the total per-socket buffer space reserved for receives.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline INT GetRcvBuf(void) const {
            INT retval;
            SIZE_T size = sizeof(INT);
            this->GetOption(SOL_SOCKET, SO_RCVBUF, &retval, size);
            return retval;
        }

        /**
         * Answer whether it is allowed that the socket is bound to an address
         * that is already in use.
         *
         * @return The current value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetReuseAddr(void) const {
            return this->getOption(SOL_SOCKET, SO_REUSEADDR);
        }

        /**
         * Answer whether a socket is bound for exclusive access. Returns false 
         * on Linux unconditionally.
         *
         * @return The current value of the option, false on Linux.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool GetExclusiveAddrUse(void) const {
#ifdef _WIN32
            return this->getOption(SOL_SOCKET, SO_EXCLUSIVEADDRUSE);
#else /* _WIN32 */
            return false;
#endif /* _WIN32 */
        }

        /**
         * Answer the total per-socket buffer space reserved for sends. 
         *
         * @return The buffer size for send operations in bytes.
         *
         * @throws SocketException If the operation fails.
         */
        inline INT GetSndBuf(void) const {
            INT retval;
            SIZE_T size = sizeof(INT);
            this->GetOption(SOL_SOCKET, SO_SNDBUF, &retval, size);
            return retval;
        }

        /**
         * Answer whether the socket is valid. Only use sockets that return true
         * in this method.
         *
         * @return true, if the socket is valid, false otherwise.
         */
        inline bool IsValid(void) const {
            return (this->handle != INVALID_SOCKET);
        }

        /**
         * Place the socket in a state in which it is listening for an incoming 
         * connection.
         *
         * @param backlog Maximum length of the queue of pending connections.
         *
         * @throws SocketException If the operation fails
         */
        virtual void Listen(const INT backlog = SOMAXCONN);

        /**
         * Receives 'cntBytes' from the socket and saves them to the memory 
         * designated by 'outData'. 'outData' must be large enough to receive at
         * least 'cntBytes'. 
         *
         * @param outData      The buffer to receive the data. The caller must
         *                     allocate this memory and remains owner.
         * @param cntBytes     The number of bytes to receive.
         * @param flags        The flags that specify the way in which the call 
         *                     is made.
         * @param forceReceive If this flag is set, the method will not return
         *                     until 'cntBytes' have been read.
         *
         * @return The number of bytes actually received.
         *
         * @throws SocketException If the operation fails.
         */
        virtual SIZE_T Receive(void *outData, const SIZE_T cntBytes, 
            const INT flags = 0, const bool forceReceive = false);

        /**
         * Receives a datagram from 'fromAddr' and stores it to 'outData'. 
         * 'outData' must be large enough to receive at least 'cntBytes'. 
         *
         * @param outData      The buffer to receive the data. The caller must
         *                     allocate this memory and remains owner.
         * @param cntBytes     The number of bytes to receive.
         * @param outFromAddr  The socket address the datagram was received 
         *                     from. This variable is only valid upon successful
         *                     return from the method.
         * @param flags        The flags that specify the way in which the call 
         *                     is made.
         * @param forceReceive If this flag is set, the method will not return
         *                     until 'cntBytes' have been read.
         *
         * @return The number of bytes actually received.
         *
         * @throws SocketException If the operation fails.
         */
        virtual SIZE_T Receive(void *outData, const SIZE_T cntBytes,
            SocketAddress& outFromAddr, const INT flags = 0, 
            const bool forceReceive = false);

        ///**
        // * Receives one object of type T to 'outData'. The method does not 
        // * return until a full object of type T has been read.
        // *
        // * Note: Be careful when communicating objects or structures to
        // * systems that have a different aligmnent!
        // *
        // * @param outData The variable that receives the data from the socket.
        // * @param flags   The flags that specify the way in which the call is 
        // *                made.
        // *
        // * @throws SocketException If the operation fails.
        // */
        //template<class T> inline void Receive(T& outData, const INT flags = 0) {
        //    return this->Receive(&outData, sizeof(T), flags, true);
        //}

        /**
         * Send 'cntBytes' from the location designated by 'data' using this 
         * socket.
         *
         * @param data      The data to be sent. The caller remains owner of the
         *                  memory.
         * @param cntBytes  The number of bytes to be sent. 'data' must contain
         *                  at least this number of bytes.
         * @param flags     The flags that specify the way in which the call is 
         *                  made.
         * @param forceSend If this flag is set, the method will not return 
         *                  until 'cntBytes' have been sent.
         *
         * @return The number of bytes acutally sent.
         *
         * @throws SocketException If the operation fails.
         */
        virtual SIZE_T Send(const void *data, const SIZE_T cntBytes, 
            const INT flags = 0, const bool forceSend = false);

        /**
         * Send a datagram of 'cntBytes' bytes from the location designated by 
         * 'data' using this socket to the socket 'toAddr'.
         *
         * @param data      The data to be sent. The caller remains owner of the
         *                  memory.
         * @param cntBytes  The number of bytes to be sent. 'data' must contain
         *                  at least this number of bytes.
         * @param toAddr    Socket address of the destination host.
         * @param flags     The flags that specify the way in which the call is 
         *                  made.
         * @param forceSend If this flag is set, the method will not return 
         *                  until 'cntBytes' have been sent.
         *
         * @return The number of bytes acutally sent.
         *
         * @throws SocketException If the operation fails.
         */
        virtual SIZE_T Send(const void *data, const SIZE_T cntBytes, 
            const SocketAddress& toAddr, const INT flags = 0, 
            const bool forceSend = false);

        ///**
        // * Sends an object of type T.
        // *
        // * Note: Be careful when communicating objects or structures to
        // * systems that have a different aligmnent!
        // *
        // * @param data  The object to be sent.
        // * @param flags The flags that specify the way in which the call is 
        // *              made.
        // *
        // * @throws SocketException If the operation fails.
        // */
        //template<class T> inline void Send(const T& data, const INT flags = 0) {
        //    return this->Send(&data, sizeof(T), flags, true);
        //}

        /**
         * Enable or disable transmission and receipt of broadcast messages on 
         * the socket.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetBroadcast(const bool enable) {
            this->setOption(SOL_SOCKET, SO_BROADCAST, enable);
        }

        /**
         * Enables or disables sockets to delay the acknowledgment of a 
         * connection until after the WSAAccept condition function is called.
         * This method has no effect on Linux.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetConditionalAccept(const bool enable) {
#ifdef _WIN32
            this->setOption(SOL_SOCKET, SO_CONDITIONAL_ACCEPT, enable);
#endif /* _WIN32 */
        }

        /**
         * Enable or disable recording of debugging information.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetDebug(const bool enable) {
            this->setOption(SOL_SOCKET, SO_DEBUG, enable);
        }

        /**
         * Enables or disables routing.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetDontRoute(const bool enable) {
            this->setOption(SOL_SOCKET, SO_DONTROUTE, enable);
        }

        /**
         * Enables or disables sending keep-alives.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetKeepAlive(const bool enable) {
            this->setOption(SOL_SOCKET, SO_DONTROUTE, enable);
        }

        /**
         * Enables or disables lingering on close if unsent data is present.
         *
         * @param enable     The new activation state of the option.
         * @parma lingerTime The linger time in seconds
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetLinger(const bool enable, const SHORT lingerTime) {
            struct linger l = { enable, lingerTime };
            this->SetOption(SOL_SOCKET, SO_LINGER, &l, sizeof(struct linger));
        }

        /**
         * Enable or disable receive of OOB data in the normal data stream.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetOOBInline(const bool enable) {
            this->setOption(SOL_SOCKET, SO_OOBINLINE, enable);
        }

        /**
         * Set a socket option.
         *
         * @param level       Level at which the option is defined.
         * @param optName     Socket option for which the value is to be set.
         * @param value       Pointer to the buffer in which the value for the 
         *                    requested option is specified. 
         * @param valueLength Size of 'value'.
         *
         * @throws SocketException If the operation fails.
         */
        virtual void SetOption(const INT level, const INT optName, 
            const void *value, const SIZE_T valueLength);

        /**
         * Specifies the total per-socket buffer space reserved for receives.
         * This is unrelated to SO_MAX_MSG_SIZE or the size of a TCP window. 
         *
         * @param size The buffer size for receive operations in bytes.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetRcvBuf(const INT size) {
            this->SetOption(SOL_SOCKET, SO_RCVBUF, &size, sizeof(INT));
        }

        /**
         * Allows the socket to be bound to an address that is already in use.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetReuseAddr(const bool enable) {
            this->setOption(SOL_SOCKET, SO_REUSEADDR, enable);
        }

        /**
         * Enable or disable a socket to be bound for exclusive access. Does 
         * not require administrative privilege. 
         *
         * This option has no effect but on Windows.
         *
         * @param enable The new activation state of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetExclusiveAddrUse(const bool enable) {
#ifdef _WIN32
            this->setOption(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, enable);
#endif /* _WIN32 */
        }

        /**
         * Specifies the total per-socket buffer space reserved for sends. 
         * This is unrelated to SO_MAX_MSG_SIZE or the size of a TCP window.
         *
         * @param size The buffer size for send operations in bytes.
         *
         * @throws SocketException If the operation fails.
         */
        inline void SetSndBuf(const INT size) {
            this->SetOption(SOL_SOCKET, SO_SNDBUF, &size, sizeof(INT));
        }

        /**
         * Disable send or receive operations on the socket.
         *
         * @param how What to shutdown.
         *
         * @throws SocketException If the operation fails.
         */
        virtual void Shutdown(const ShutdownManifest how);

        /**
         * Assignment operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return *this.
         */
        Socket& operator =(const Socket& rhs);

    protected:

        /**
         * Answer a boolean socket option.
         *
         * @param level   Level at which the option is defined.
         * @param optName Socket option for which the value is to be retrieved.
         *
         * @return The value of the option.
         *
         * @throws SocketException If the operation fails.
         */
        inline bool getOption(const INT level, const INT optName) const {
            INT value = 0;
            SIZE_T valueSize = sizeof(INT);
            this->GetOption(level, optName, &value, valueSize);
            return (value != 0);
        }

        /**
         * Set a boolean socket option.
         *
         * @param level   Level at which the option is defined.
         * @param optName Socket option for which the value is to be set.
         * @param value   The new value of the option.
         *
         * @throws SocketException If the operation fails.
         */
		inline void setOption(const INT level, const INT optName, 
                const bool value) {
			INT tmp = value ? 1 : 0;
			return this->SetOption(level, optName, &tmp, sizeof(INT));
		}

        /** The socket handle. */
        SOCKET handle;
    };

} /* end namespace net */
} /* end namespace vislib */

#endif /* VISLIB_SOCKET_H_INCLUDED */
