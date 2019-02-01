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
#ifndef ServerThreadH
#define ServerThreadH
//---------------------------------------------------------------------------

class ServerThread {
private:
    struct AntiConFlood {
        uint8_t ui128IpHash[16];

        uint64_t Time;

        AntiConFlood *prev, *next;

        int16_t hits;
    };

#ifdef _WIN32
    SOCKET server;

    unsigned int threadId;
    uint32_t iSuspendTime;

    HANDLE threadHandle;

    CRITICAL_SECTION csServerThread;
#else
    int server;

    unsigned int iSuspendTime;

    pthread_t threadId;

    pthread_mutex_t mtxServerThread;
#endif

	AntiConFlood *AntiFloodList;

    int iAdressFamily;

	bool bTerminated;
public:
    ServerThread *prev, *next;

    uint16_t ui16Port;

    bool bActive, bSuspended;

	ServerThread(const int &iAddrFamily, const uint16_t &ui16PortNumber);
	~ServerThread();

	void Resume();
	void Run();
	void Close();
	void WaitFor();
	bool Listen(bool bSilent = false);
#ifdef _WIN32
	bool isFlooder(const SOCKET &s, const sockaddr_storage &addr);
#else
	bool isFlooder(const int &s, const sockaddr_storage &addr);
#endif
	void RemoveConFlood(AntiConFlood * cur);
	void ResumeSck();
	void SuspendSck(const uint32_t &iTime);
};
//---------------------------------------------------------------------------

#endif
