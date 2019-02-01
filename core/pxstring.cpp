/*
 * PtokaX - hub server for Direct Connect peer to peer network.

 * Copyright (C) 2002-2005  Ptaczek, Ptaczek at PtokaX dot org
 * Copyright (C) 2004-2013  Petr Kozelka, PPK at PtokaX dot org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#include "stdinc.h"
//---------------------------------------------------------------------------
#include "pxstring.h"
//---------------------------------------------------------------------------
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
static const char * sEmpty = "";
//---------------------------------------------------------------------------

void string::stralloc(const char * sTxt, const size_t &szLen) {
	szDataLen = szLen;

	if(szDataLen == 0) {
		sData = (char *)sEmpty;
		return;
	}

	sData = (char *)malloc(szDataLen+1);
	if(sData == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::stralloc\n", (uint64_t)(szDataLen+1));

        return;
	}

	memcpy(sData, sTxt, szDataLen);
	sData[szDataLen] = '\0';
}
//---------------------------------------------------------------------------

string::string() {
	sData = (char *)sEmpty;
    szDataLen = 0;
}
//---------------------------------------------------------------------------

string::string(const char * sTxt) {
	stralloc(sTxt, strlen(sTxt));
}
//---------------------------------------------------------------------------

string::string(const char * sTxt, const size_t &szLen) {
	stralloc(sTxt, szLen);
}
//---------------------------------------------------------------------------

string::string(const string & sStr) {
    stralloc(sStr.c_str(), sStr.size());
}
//---------------------------------------------------------------------------

string::string(const uint32_t & ui32Number) {
	char tmp[16];
#ifdef _WIN32
	ultoa(ui32Number, tmp, 10);
	stralloc(tmp, strlen(tmp));
#else
	int iLen = sprintf(tmp, "%u", ui32Number);
	stralloc(tmp, iLen);
#endif
}
//---------------------------------------------------------------------------

string::string(const int32_t & i32Number) {
	char tmp[16];
#ifdef _WIN32
	ltoa(i32Number, tmp, 10);
	stralloc(tmp, strlen(tmp));
#else
	int iLen = sprintf(tmp, "%d", i32Number);
	stralloc(tmp, iLen);
#endif
}
//---------------------------------------------------------------------------

string::string(const uint64_t & ui64Number) {
	char tmp[32];
#ifdef _WIN32
	_ui64toa(ui64Number, tmp, 10);
	stralloc(tmp, strlen(tmp));
#else
	int iLen = sprintf(tmp, "%" PRIu64, ui64Number);
	stralloc(tmp, iLen);
#endif
}
//---------------------------------------------------------------------------

string::string(const int64_t & i64Number) {
	char tmp[32];
#ifdef _WIN32
	_i64toa(i64Number, tmp, 10);
	stralloc(tmp, strlen(tmp));
#else
	int iLen = sprintf(tmp, "%" PRId64, i64Number);
	stralloc(tmp, iLen);
#endif
}
//---------------------------------------------------------------------------

string::string(const string & sStr1, const string & sStr2) {
    szDataLen = sStr1.size()+sStr2.size();

    if(szDataLen == 0) {
        sData = (char *)sEmpty;
        return;
    }

    sData = (char *)malloc(szDataLen+1);
    if(sData == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::string(string, string)\n", (uint64_t)(szDataLen+1));

        return;
    }

    memcpy(sData, sStr1.c_str(), sStr1.size());
    memcpy(sData+sStr1.size(), sStr2.c_str(), sStr2.size());
    sData[szDataLen] = '\0';
}
//---------------------------------------------------------------------------

string::string(const char * sTxt, const string & sStr) {
    size_t szLen = strlen(sTxt);
	szDataLen = szLen+sStr.size();

    if(szDataLen == 0) {
        sData = (char *)sEmpty;
        return;
    }

    sData = (char *)malloc(szDataLen+1);
    if(sData == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::string(char, string)\n", (uint64_t)(szDataLen+1));

        return;
    }

    memcpy(sData, sTxt, szLen);
    memcpy(sData+szLen, sStr.c_str(), sStr.size());
    sData[szDataLen] = '\0';
}
//---------------------------------------------------------------------------

string::string(const string & sStr, const char * sTxt) {
    size_t szLen = strlen(sTxt);
	szDataLen = sStr.size()+szLen;

    if(szDataLen == 0) {
        sData = (char *)sEmpty;
        return;
    }

    sData = (char *)malloc(szDataLen+1);
    if(sData == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::string(string, char)\n", (uint64_t)(szDataLen+1));

        return;
    }

    memcpy(sData, sStr.c_str(), sStr.size());
    memcpy(sData+sStr.size(), sTxt, szLen);
    sData[szDataLen] = '\0';
}
//---------------------------------------------------------------------------

string::~string() {
    if(sData != sEmpty) {
        free(sData);
    }
}
//---------------------------------------------------------------------------

size_t string::size() const {
    return szDataLen;
}
//---------------------------------------------------------------------------

char * string::c_str() const {
	return sData;
}
//---------------------------------------------------------------------------

void string::clear() {
    if(sData != sEmpty) {
        free(sData);
    }

	sData = (char *)sEmpty;
    szDataLen = 0;
}
//---------------------------------------------------------------------------

string string::operator+(const char * sTxt) const {
	string result(*this, sTxt);
	return result;
}
//---------------------------------------------------------------------------

string string::operator+(const string & sStr) const {
    string result(*this, sStr);
	return result;
}
//---------------------------------------------------------------------------

string operator+(const char * sTxt, const string & sStr) {
	string result(sTxt, sStr);
	return result;
}
//---------------------------------------------------------------------------

string & string::operator+=(const char * sTxt) {
    size_t szLen = strlen(sTxt);

    char * oldbuf = sData;

    if(sData == sEmpty) {
        sData = (char *)malloc(szDataLen+szLen+1);
    } else {
        sData = (char *)realloc(oldbuf, szDataLen+szLen+1);
    }

    if(sData == NULL) {
        sData = oldbuf;

        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::operator+=(char)\n", (uint64_t)(szDataLen+szLen+1));

        return *this;
    }

    memcpy(sData+szDataLen, sTxt, szLen);
	szDataLen += szLen;
	sData[szDataLen] = '\0';

    return *this;
}
//---------------------------------------------------------------------------

string & string::operator+=(const string & sStr) {
    if(sStr.c_str() == sEmpty) {
        return *this;
    }

    char * oldbuf = sData;

    if(sData == sEmpty) {
        sData = (char *)malloc(szDataLen+sStr.size()+1);
    } else {
        sData = (char *)realloc(oldbuf, szDataLen+sStr.size()+1);
    }

    if(sData == NULL) {
        sData = oldbuf;

        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sData in string::operator+=(string)\n", (uint64_t)(szDataLen+sStr.size()+1));

        return *this;
    }

    memcpy(sData+szDataLen, sStr.c_str(), sStr.size());
	szDataLen += sStr.size();
	sData[szDataLen] = '\0';

    return *this;
}
//---------------------------------------------------------------------------

string & string::operator=(const char * sTxt) {
	if(sData != sEmpty) {
        free(sData);
    }

	stralloc(sTxt, strlen(sTxt));

    return *this;
}
//---------------------------------------------------------------------------

string & string::operator=(const string & sStr) {
    if(sData != sEmpty) {
        free(sData);
    }

	stralloc(sStr.c_str(), sStr.size());

    return *this;
}
//---------------------------------------------------------------------------

bool string::operator==(const char * sTxt) {
    if(szDataLen != strlen(sTxt) || 
        memcmp(sData, sTxt, szDataLen) != 0) {
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------

bool string::operator==(const string & sStr) {
    if(szDataLen != sStr.size() || 
        memcmp(sData, sStr.c_str(), szDataLen) != 0) {
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------
