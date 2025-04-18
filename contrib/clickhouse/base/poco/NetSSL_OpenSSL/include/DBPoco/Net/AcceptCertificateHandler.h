//
// AcceptCertificateHandler.h
//
// Library: NetSSL_OpenSSL
// Package: SSLCore
// Module:  AcceptCertificateHandler
//
// Definition of the AcceptCertificateHandler class.
//
// Copyright (c) 2006-2009, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_NetSSL_AcceptCertificateHandler_INCLUDED
#define DB_NetSSL_AcceptCertificateHandler_INCLUDED


#include "DBPoco/Net/InvalidCertificateHandler.h"
#include "DBPoco/Net/NetSSL.h"


namespace DBPoco
{
namespace Net
{


    class NetSSL_API AcceptCertificateHandler : public InvalidCertificateHandler
    /// A AcceptCertificateHandler is invoked whenever an error
    /// occurs verifying the certificate. It always accepts
    /// the certificate.
    ///
    /// Should be using for testing purposes only.
    {
    public:
        AcceptCertificateHandler(bool handleErrorsOnServerSide);
        /// Creates the AcceptCertificateHandler

        virtual ~AcceptCertificateHandler();
        /// Destroys the AcceptCertificateHandler.

        void onInvalidCertificate(const void * pSender, VerificationErrorArgs & errorCert);
        /// Receives the questionable certificate in parameter errorCert. If one wants to accept the
        /// certificate, call errorCert.setIgnoreError(true).
    };


}
} // namespace DBPoco::Net


#endif // DB_NetSSL_AcceptCertificateHandler_INCLUDED
