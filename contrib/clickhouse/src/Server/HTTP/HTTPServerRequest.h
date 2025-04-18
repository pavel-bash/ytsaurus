#pragma once

#include <Interpreters/Context_fwd.h>
#include <IO/ReadBuffer.h>
#include <Server/HTTP/HTTPRequest.h>
#include <Server/HTTP/HTTPContext.h>
#include <Common/ProfileEvents.h>
#include "clickhouse_config.h"

#include <DBPoco/Net/HTTPServerSession.h>

namespace DBPoco::Net { class X509Certificate; }

namespace DB
{

class HTTPServerResponse;
class ReadBufferFromPocoSocket;

class HTTPServerRequest : public HTTPRequest
{
public:
    HTTPServerRequest(HTTPContextPtr context, HTTPServerResponse & response, DBPoco::Net::HTTPServerSession & session, const ProfileEvents::Event & read_event = ProfileEvents::end());

    /// FIXME: it's a little bit inconvenient interface. The rationale is that all other ReadBuffer's wrap each other
    ///        via unique_ptr - but we can't inherit HTTPServerRequest from ReadBuffer and pass it around,
    ///        since we also need it in other places.

    /// Returns the input stream for reading the request body.
    ReadBuffer & getStream()
    {
        DB_poco_check_ptr(stream);
        return *stream;
    }

    bool checkPeerConnected() const;

    bool isSecure() const { return secure; }

    /// Returns the client's address.
    const DBPoco::Net::SocketAddress & clientAddress() const { return client_address; }

    /// Returns the server's address.
    const DBPoco::Net::SocketAddress & serverAddress() const { return server_address; }

#if USE_SSL
    bool havePeerCertificate() const;
    DBPoco::Net::X509Certificate peerCertificate() const;
#endif

private:
    /// Limits for basic sanity checks when reading a header
    enum Limits
    {
        MAX_METHOD_LENGTH = 32,
        MAX_VERSION_LENGTH = 8,
    };

    const size_t max_uri_size;
    const size_t max_fields_number;
    const size_t max_field_name_size;
    const size_t max_field_value_size;

    std::unique_ptr<ReadBuffer> stream;
    DBPoco::Net::SocketImpl * socket;
    DBPoco::Net::SocketAddress client_address;
    DBPoco::Net::SocketAddress server_address;

    bool secure;

    void readRequest(ReadBuffer & in);
};

}
