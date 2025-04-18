//
// AttrMap.h
//
// Library: XML
// Package: DOM
// Module:  DOM
//
// Definition of the AttrMap class.
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef DB_DOM_AttrMap_INCLUDED
#define DB_DOM_AttrMap_INCLUDED


#include "DBPoco/DOM/NamedNodeMap.h"
#include "DBPoco/XML/XML.h"


namespace DBPoco
{
namespace XML
{


    class Element;


    class XML_API AttrMap : public NamedNodeMap
    // This implementation of NamedNodeMap is
    // returned by Element::attributes()
    {
    public:
        Node * getNamedItem(const XMLString & name) const;
        Node * setNamedItem(Node * arg);
        Node * removeNamedItem(const XMLString & name);
        Node * item(unsigned long index) const;
        unsigned long length() const;

        Node * getNamedItemNS(const XMLString & namespaceURI, const XMLString & localName) const;
        Node * setNamedItemNS(Node * arg);
        Node * removeNamedItemNS(const XMLString & namespaceURI, const XMLString & localName);

        void autoRelease();

    protected:
        AttrMap(Element * pElement);
        ~AttrMap();

    private:
        AttrMap();

        Element * _pElement;

        friend class Element;
    };


}
} // namespace DBPoco::XML


#endif // DB_DOM_AttrMap_INCLUDED
