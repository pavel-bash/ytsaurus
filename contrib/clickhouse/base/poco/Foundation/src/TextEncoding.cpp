//
// TextEncoding.cpp
//
// Library: Foundation
// Package: Text
// Module:  TextEncoding
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "DBPoco/TextEncoding.h"
#include "DBPoco/Exception.h"
#include "DBPoco/String.h"
#include "DBPoco/ASCIIEncoding.h"
#include "DBPoco/UTF32Encoding.h"
#include "DBPoco/UTF16Encoding.h"
#include "DBPoco/UTF8Encoding.h"
#include "DBPoco/RWLock.h"
#include "DBPoco/SingletonHolder.h"
#include <map>


namespace DBPoco {


//
// TextEncodingManager
//


class TextEncodingManager
{
public:
	TextEncodingManager()
	{
		TextEncoding::Ptr pUtf8Encoding(new UTF8Encoding);
		add(pUtf8Encoding, TextEncoding::GLOBAL);

		add(new ASCIIEncoding);
		add(pUtf8Encoding);
		add(new UTF16Encoding);
		add(new UTF32Encoding);
	}

	~TextEncodingManager()
	{
	}

	void add(TextEncoding::Ptr pEncoding)
	{
		add(pEncoding, pEncoding->canonicalName());
	}

	void add(TextEncoding::Ptr pEncoding, const std::string& name)
	{
		RWLock::ScopedLock lock(_lock, true);
	
		_encodings[name] = pEncoding;
	}

	void remove(const std::string& name)
	{
		RWLock::ScopedLock lock(_lock, true);
	
		_encodings.erase(name);
	}
	
	TextEncoding::Ptr find(const std::string& name) const
	{
		RWLock::ScopedLock lock(_lock);
		
		EncodingMap::const_iterator it = _encodings.find(name);
		if (it != _encodings.end())
			return it->second;
		
		for (it = _encodings.begin(); it != _encodings.end(); ++it)
		{
			if (it->second->isA(name))
				return it->second;
		}
		return TextEncoding::Ptr();
	}

private:
	TextEncodingManager(const TextEncodingManager&);
	TextEncodingManager& operator = (const TextEncodingManager&);
	
	typedef std::map<std::string, TextEncoding::Ptr, CILess> EncodingMap;
	
	EncodingMap    _encodings;
	mutable RWLock _lock;
};


//
// TextEncoding
//


const std::string TextEncoding::GLOBAL;


TextEncoding::~TextEncoding()
{
}


int TextEncoding::convert(const unsigned char* bytes) const
{
	return static_cast<int>(*bytes);
}


int TextEncoding::convert(int ch, unsigned char* bytes, int length) const
{
	return 0;
}


int TextEncoding::queryConvert(const unsigned char* bytes, int length) const
{
	return (int) *bytes;
}


int TextEncoding::sequenceLength(const unsigned char* bytes, int length) const
{
	return 1;
}


TextEncoding& TextEncoding::byName(const std::string& encodingName)
{
	TextEncoding* pEncoding = manager().find(encodingName);
	if (pEncoding)
		return *pEncoding;
	else
		throw NotFoundException(encodingName);
}

	
TextEncoding::Ptr TextEncoding::find(const std::string& encodingName)
{
	return manager().find(encodingName);
}


void TextEncoding::add(TextEncoding::Ptr pEncoding)
{
	manager().add(pEncoding, pEncoding->canonicalName());
}


void TextEncoding::add(TextEncoding::Ptr pEncoding, const std::string& name)
{
	manager().add(pEncoding, name);
}


void TextEncoding::remove(const std::string& encodingName)
{
	manager().remove(encodingName);
}


TextEncoding::Ptr TextEncoding::global(TextEncoding::Ptr encoding)
{
	TextEncoding::Ptr prev = find(GLOBAL);
	add(encoding, GLOBAL);
	return prev;
}


TextEncoding& TextEncoding::global()
{
	return byName(GLOBAL);
}


namespace
{
	static SingletonHolder<TextEncodingManager> sh;
}


TextEncodingManager& TextEncoding::manager()
{
	return *sh.get();
}


} // namespace DBPoco
