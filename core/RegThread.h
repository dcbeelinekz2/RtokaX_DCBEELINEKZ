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
#ifndef RegThreadH
#define RegThreadH
//---------------------------------------------------------------------------

class clsRegisterThread {
private:
    struct RegSocket {
        RegSocket();
        ~RegSocket();

        uint64_t iTotalShare;

#ifdef _WIN32
        SOCKET sock;
#else
		int sock;
#endif

        uint32_t iRecvBufLen, iRecvBufSize, iSendBufLen, iTotalUsers;

        uint32_t ui32AddrLen;

		char *sAddress, *sRecvBuf, *sSendBuf, *sSendBufHead;

        RegSocket *prev, *next;    
    };

#ifdef _WIN32
    unsigned int threadId;

    HANDLE threadHandle;
#else
	pthread_t threadId;
#endif

    RegSocket *RegSockListS, *RegSockListE;

    bool bTerminated;

    char sMsg[2048];

	void AddSock(char * sAddress, const size_t &ui32Len);
	bool Receive(RegSocket * Sock);
    static void Add2SendBuf(RegSocket * Sock, char * sData);
    bool Send(RegSocket * Sock);
    void RemoveSock(RegSocket * Sock);
public:
    static clsRegisterThread * mPtr;

    uint32_t iBytesRead, iBytesSent;

	clsRegisterThread();
	~clsRegisterThread();

	void Setup(char * sAddresses, const uint16_t &ui16AddrsLen);
	void Resume();
	void Run();
	void Close();
	void WaitFor();
};
//---------------------------------------------------------------------------

#endif
