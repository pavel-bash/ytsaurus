//
// HTTPSessionInstantiator.h
//
// Library: Net
// Package: HTTPClient
// Module:  HTTPSessionInstantiator
//
// Definition of the HTTPSessionInstantiator class.
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_Net_HTTPSessionInstantiator_INCLUDED
#define DB_Net_HTTPSessionInstantiator_INCLUDED


#include "DBPoco/Net/HTTPSession.h"
#include "DBPoco/Net/Net.h"
#include "DBPoco/URI.h"


namespace DBPoco
{
namespace Net
{


    class HTTPClientSession;


    class Net_API HTTPSessionInstantiator
    /// A factory for HTTPClientSession objects.
    ///
    /// Creates a HTTP session for a given URI.
    /// A HTTPSessionInstantiator is not used directly.
    /// Instances are registered with a HTTPSessionFactory,
    /// and used through it.
    {
    public:
        HTTPSessionInstantiator();
        /// Creates the HTTPSessionInstantiator.

        virtual ~HTTPSessionInstantiator();
        /// Destroys the HTTPSessionInstantiator.

        virtual HTTPClientSession * createClientSession(const DBPoco::URI & uri);
        /// Creates a HTTPClientSession for the given URI.

        static void registerInstantiator();
        /// Registers the instantiator with the global HTTPSessionFactory.

        static void unregisterInstantiator();
        /// Unregisters the factory with the global HTTPSessionFactory.

    protected:
        void setProxy(const std::string & host, DBPoco::UInt16 port);
        /// Sets the proxy host and port.
        /// Called by HTTPSessionFactory.

        const std::string & proxyHost() const;
        /// Returns the proxy post.

        DBPoco::UInt16 proxyPort() const;
        /// Returns the proxy port.

        void setProxyCredentials(const std::string & username, const std::string & password);
        /// Sets the username and password for proxy authorization (Basic auth only).

        const std::string & proxyUsername() const;
        /// Returns the username for proxy authorization.

        const std::string & proxyPassword() const;
        /// Returns the password for proxy authorization.

    private:
        std::string _proxyHost;
        DBPoco::UInt16 _proxyPort;
        std::string _proxyUsername;
        std::string _proxyPassword;

        friend class HTTPSessionFactory;
    };


    //
    // inlines
    //
    inline const std::string & HTTPSessionInstantiator::proxyHost() const
    {
        return _proxyHost;
    }


    inline DBPoco::UInt16 HTTPSessionInstantiator::proxyPort() const
    {
        return _proxyPort;
    }


    inline const std::string & HTTPSessionInstantiator::proxyUsername() const
    {
        return _proxyUsername;
    }


    inline const std::string & HTTPSessionInstantiator::proxyPassword() const
    {
        return _proxyPassword;
    }


}
} // namespace DBPoco::Net


#endif // DB_Net_HTTPSessionInstantiator_INCLUDED
