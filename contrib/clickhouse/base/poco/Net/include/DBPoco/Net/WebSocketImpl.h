//
// WebSocketImpl.h
//
// Library: Net
// Package: WebSocket
// Module:  WebSocketImpl
//
// Definition of the StreamSocketImpl class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Net_WebSocketImpl_INCLUDED
#define DB_Net_WebSocketImpl_INCLUDED


#include "DBPoco/Buffer.h"
#include "DBPoco/Net/StreamSocketImpl.h"
#include "DBPoco/Random.h"


namespace DBPoco
{
namespace Net
{


    class HTTPSession;


    class Net_API WebSocketImpl : public StreamSocketImpl
    /// This class implements a WebSocket, according
    /// to the WebSocket protocol described in RFC 6455.
    {
    public:
        WebSocketImpl(StreamSocketImpl * pStreamSocketImpl, HTTPSession & session, bool mustMaskPayload);
        /// Creates a WebSocketImpl.

        // StreamSocketImpl
        virtual int sendBytes(const void * buffer, int length, int flags);
        /// Sends a WebSocket protocol frame.

        virtual int receiveBytes(void * buffer, int length, int flags);
        /// Receives a WebSocket protocol frame.

        virtual int receiveBytes(DBPoco::Buffer<char> & buffer, int flags);
        /// Receives a WebSocket protocol frame.

        virtual SocketImpl * acceptConnection(SocketAddress & clientAddr);
        virtual void connect(const SocketAddress & address);
        virtual void connect(const SocketAddress & address, const DBPoco::Timespan & timeout);
        virtual void connectNB(const SocketAddress & address);
        virtual void bind(const SocketAddress & address, bool reuseAddress = false);
        virtual void bind(const SocketAddress & address, bool reuseAddress, bool reusePort);
        virtual void bind6(const SocketAddress & address, bool reuseAddress = false, bool ipV6Only = false);
        virtual void bind6(const SocketAddress & address, bool reuseAddress, bool reusePort, bool ipV6Only);
        virtual void listen(int backlog = 64);
        virtual void close();
        virtual void shutdownReceive();
        virtual void shutdownSend();
        virtual void shutdown();
        virtual int sendTo(const void * buffer, int length, const SocketAddress & address, int flags = 0);
        virtual int receiveFrom(void * buffer, int length, SocketAddress & address, int flags = 0);
        virtual void sendUrgent(unsigned char data);
        virtual int available();
        virtual bool secure() const;
        virtual void setSendTimeout(const DBPoco::Timespan & timeout);
        virtual DBPoco::Timespan getSendTimeout();
        virtual void setReceiveTimeout(const DBPoco::Timespan & timeout);
        virtual DBPoco::Timespan getReceiveTimeout();

        // Internal
        int frameFlags() const;
        /// Returns the frame flags of the most recently received frame.

        bool mustMaskPayload() const;
        /// Returns true if the payload must be masked.

        void setMaxPayloadSize(int maxPayloadSize);
        /// Sets the maximum payload size for receiveFrame().
        ///
        /// The default is std::numeric_limits<int>::max().

        int getMaxPayloadSize() const;
        /// Returns the maximum payload size for receiveFrame().
        ///
        /// The default is std::numeric_limits<int>::max().

    protected:
        enum
        {
            FRAME_FLAG_MASK = 0x80,
            MAX_HEADER_LENGTH = 14
        };

        int receiveHeader(char mask[4], bool & useMask);
        int receivePayload(char * buffer, int payloadLength, char mask[4], bool useMask);
        int receiveNBytes(void * buffer, int bytes);
        int receiveSomeBytes(char * buffer, int bytes);
        virtual ~WebSocketImpl();

    private:
        WebSocketImpl();

        StreamSocketImpl * _pStreamSocketImpl;
        int _maxPayloadSize;
        DBPoco::Buffer<char> _buffer;
        int _bufferOffset;
        int _frameFlags;
        bool _mustMaskPayload;
        DBPoco::Random _rnd;
    };


    //
    // inlines
    //
    inline int WebSocketImpl::frameFlags() const
    {
        return _frameFlags;
    }


    inline bool WebSocketImpl::mustMaskPayload() const
    {
        return _mustMaskPayload;
    }


    inline int WebSocketImpl::getMaxPayloadSize() const
    {
        return _maxPayloadSize;
    }


}
} // namespace DBPoco::Net


#endif // DB_Net_WebSocketImpl_INCLUDED
