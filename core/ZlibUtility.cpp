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
#include "ZlibUtility.h"
//---------------------------------------------------------------------------
#include "ServerManager.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include <zlib.h>
//---------------------------------------------------------------------------
static const uint32_t ZBUFFERLEN = 131072;
static const uint32_t ZMINLEN = 100;
//---------------------------------------------------------------------------
clsZlibUtility * clsZlibUtility::mPtr = NULL;
//---------------------------------------------------------------------------

clsZlibUtility::clsZlibUtility() {
	// allocate buffer for zlib
#ifdef _WIN32
	sZbuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ZBUFFERLEN);
#else
	sZbuffer = (char *)calloc(ZBUFFERLEN, 1);
#endif
	if(sZbuffer == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sZbuffer in clsZlibUtility::clsZlibUtility\n", (uint64_t)ZBUFFERLEN);
		exit(EXIT_FAILURE);
	}
	memcpy(sZbuffer, "$ZOn|", 5);
    szZbufferSize = ZBUFFERLEN;
}
//---------------------------------------------------------------------------
	
clsZlibUtility::~clsZlibUtility() {
#ifdef _WIN32
    if(sZbuffer != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sZbuffer) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sZbuffer in clsZlibUtility::~clsZlibUtility\n", 0);
        }
    }
#else
	free(sZbuffer);
#endif
}
//---------------------------------------------------------------------------

char * clsZlibUtility::CreateZPipe(const char *sInData, const size_t &szInDataSize, uint32_t &ui32OutDataLen) {
    // prepare Zbuffer
    if(szZbufferSize < szInDataSize + 128) {
        size_t szOldZbufferSize = szZbufferSize;

        szZbufferSize = Allign128K(szInDataSize + 128);

        char * pOldBuf = sZbuffer;
#ifdef _WIN32
        sZbuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szZbufferSize);
#else
		sZbuffer = (char *)realloc(pOldBuf, szZbufferSize);
#endif
        if(sZbuffer == NULL) {
            sZbuffer = pOldBuf;
            szZbufferSize = szOldZbufferSize;
            ui32OutDataLen = 0;

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sZbuffer in clsZlibUtility::CreateZPipe\n", (uint64_t)szZbufferSize);

            return sZbuffer;
        }
    }
    
    z_stream stream;

    // init zlib struct
    memset(&stream, 0 , sizeof(stream));

    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.data_type = Z_TEXT;

    deflateInit(&stream, Z_BEST_COMPRESSION);

    stream.next_in  = (Bytef*)sInData;
    stream.avail_in = (uInt)szInDataSize;

    stream.next_out = (Bytef*)sZbuffer+5;
    stream.avail_out = (uInt)(szZbufferSize-5);

    // compress
    if(deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&stream);
        AppendDebugLog("%s - [ERR] deflate error\n", 0);
        ui32OutDataLen = 0;
        return sZbuffer;
    }
    
    ui32OutDataLen = stream.total_out+5;

    // cleanup zlib
    deflateEnd(&stream);

    if(ui32OutDataLen >= szInDataSize) {
        ui32OutDataLen = 0;
        return sZbuffer;
    }
   
    return sZbuffer;
}
//---------------------------------------------------------------------------

char * clsZlibUtility::CreateZPipe(char *sInData, const size_t &szInDataSize, char *sOutData, size_t &szOutDataLen, size_t &szOutDataSize) {
    if(szInDataSize < ZMINLEN)
        return sOutData;

    // prepare Zbuffer
    if(szZbufferSize < szInDataSize + 128) {
        size_t szOldZbufferSize = szZbufferSize;

        szZbufferSize = Allign128K(szInDataSize + 128);

        char * pOldBuf = sZbuffer;
#ifdef _WIN32
        sZbuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szZbufferSize);
#else
		sZbuffer = (char *)realloc(pOldBuf, szZbufferSize);
#endif
        if(sZbuffer == NULL) {
            sZbuffer = pOldBuf;
            szZbufferSize = szOldZbufferSize;
            szOutDataLen = 0;

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sZbuffer in clsZlibUtility::CreateZPipe\n", (uint64_t)szZbufferSize);

            return sOutData;
        }
    }
    
    z_stream stream;

    // init zlib struct
    memset(&stream, 0 , sizeof(stream));

    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.data_type = Z_TEXT;

    deflateInit(&stream, Z_BEST_COMPRESSION);

    stream.next_in  = (Bytef*)sInData;
    stream.avail_in = (uInt)szInDataSize;

    stream.next_out = (Bytef*)sZbuffer+5;
    stream.avail_out = (uInt)(szZbufferSize-5);

    // compress
    if(deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&stream);
        AppendDebugLog("%s - [ERR] deflate error\n", 0);
        return sOutData;
    }
    
    szOutDataLen = stream.total_out+5;

    // cleanup zlib
    deflateEnd(&stream);

    if(szOutDataLen >= szInDataSize) {
        szOutDataLen = 0;
        return sOutData;
    }
    
    // prepare out buffer
    if(szOutDataSize < szOutDataLen) {
        size_t szOldOutDataSize = szOutDataSize;

        szOutDataSize = Allign1024(szOutDataLen)-1;
        char * pOldBuf = sOutData;
#ifdef _WIN32
        sOutData = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szOutDataSize+1);
#else
		sOutData = (char *)realloc(pOldBuf, szOutDataSize+1);
#endif
        if(sOutData == NULL) {
            sOutData = pOldBuf;
            szOutDataSize = szOldOutDataSize;
            szOutDataLen = 0;

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sOutData in clsZlibUtility::CreateZPipe\n", (uint64_t)(szOutDataSize+1));

            return sOutData;
        }
    }
    
    memcpy(sOutData, sZbuffer, szOutDataLen);
    
    return sOutData;
}
//---------------------------------------------------------------------------

char * clsZlibUtility::CreateZPipe(char *sInData, const unsigned int &uiInDataSize, char *sOutData, unsigned int &uiOutDataLen, unsigned int &uiOutDataSize, size_t (* pAllignFunc)(size_t n)) {
    if(uiInDataSize < ZMINLEN)
        return sOutData;

    // prepare Zbuffer
    if(szZbufferSize < uiInDataSize + 128) {
        size_t szOldZbufferSize = szZbufferSize;

        szZbufferSize = Allign128K(uiInDataSize + 128);

        char * pOldBuf = sZbuffer;
#ifdef _WIN32
        sZbuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szZbufferSize);
#else
		sZbuffer = (char *)realloc(pOldBuf, szZbufferSize);
#endif
        if(sZbuffer == NULL) {
            sZbuffer = pOldBuf;
            szZbufferSize = szOldZbufferSize;
            uiOutDataLen = 0;

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sZbuffer in clsZlibUtility::CreateZPipe\n", (uint64_t)szZbufferSize);

            return sOutData;
        }
    }
    
    z_stream stream;

    // init zlib struct
    memset(&stream, 0 , sizeof (stream));

    stream.zalloc = Z_NULL;
    stream.zfree  = Z_NULL;
    stream.data_type = Z_TEXT;

    deflateInit(&stream, Z_BEST_COMPRESSION);

    stream.next_in  = (Bytef*)sInData;
    stream.avail_in = (uInt)uiInDataSize;

    stream.next_out = (Bytef*)sZbuffer+5;
    stream.avail_out = (uInt)(szZbufferSize-5);

    // compress
    if(deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&stream);
        AppendDebugLog("%s - [ERR] deflate error\n", 0);
        return sOutData;
    }
    
    uiOutDataLen = stream.total_out+5;

    // cleanup zlib
    deflateEnd(&stream);

    if(uiOutDataLen >= uiInDataSize) {
        uiOutDataLen = 0;
        return sOutData;
    }

    // prepare out buffer
    if(uiOutDataSize < uiOutDataLen) {
        unsigned int uiOldOutDataSize = uiOutDataSize;

        uiOutDataSize = (unsigned int)(* pAllignFunc)(uiOutDataLen+1);

        char * pOldBuf = sOutData;
#ifdef _WIN32
        sOutData = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, uiOutDataSize);
#else
		sOutData = (char *)realloc(pOldBuf, uiOutDataSize);
#endif
        if(sOutData == NULL) {
            sOutData = pOldBuf;
            uiOutDataSize = uiOldOutDataSize;
            uiOutDataLen = 0;

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sOutData in clsZlibUtility::CreateZPipe\n", (uint64_t)(uiOutDataSize+1));

            return sOutData;
        }
    }
    
    memcpy(sOutData, sZbuffer, uiOutDataLen);
   
    return sOutData;
}
//---------------------------------------------------------------------------
