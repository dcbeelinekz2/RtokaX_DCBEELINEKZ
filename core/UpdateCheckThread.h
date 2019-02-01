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
#ifndef UpdateCheckThreadH
#define UpdateCheckThreadH
//---------------------------------------------------------------------------

class clsUpdateCheckThread {
private:
    SOCKET sSocket;

	uint32_t ui32FileLen;

    uint32_t ui32RecvBufLen, ui32RecvBufSize;
    uint32_t ui32BytesRead, ui32BytesSent;

    HANDLE hThread;

    char * sRecvBuf;
    
    bool bOk, bData, bTerminated;

	char sMsg[2048];

    static void Message(char * sMessage, const size_t &szLen);
    bool Receive();
    bool SendHeader();
public:
    static clsUpdateCheckThread * mPtr;

	clsUpdateCheckThread();
	~clsUpdateCheckThread();

    void Resume();
    void Run();
	void Close();
	void WaitFor();
};
//---------------------------------------------------------------------------

#endif
