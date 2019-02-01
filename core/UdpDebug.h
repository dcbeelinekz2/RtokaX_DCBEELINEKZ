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
#ifndef UdpDebugH
#define UdpDebugH
//---------------------------------------------------------------------------
struct User;
//---------------------------------------------------------------------------

class clsUdpDebug {
private:
    struct UdpDbgItem {
        UdpDbgItem();
        ~UdpDbgItem();

#ifdef _WIN32
        SOCKET s;
#else
		int s;
#endif
        sockaddr_storage sas_to;
        int sas_len;

        uint32_t ui32Hash;
        char *Nick;

        UdpDbgItem *prev, *next;
        bool bIsScript;
    };

public:
    static clsUdpDebug * mPtr;

    UdpDbgItem *llist, *ScriptList;

	clsUdpDebug();
	~clsUdpDebug();

	void Broadcast(const char * msg) const;
	void Broadcast(const char * msg, const size_t &szLen) const;
    void Broadcast(const string & msg) const;
	bool New(User *u, const int32_t &port);
	bool New(char * sIP, const uint16_t &usPort, const bool &bAllData, char * sScriptName);
	bool Remove(User * u);
	void Remove(char * sScriptName);
	bool CheckUdpSub(User * u, bool bSndMess = false) const;
	void Send(char * sScriptName, char * sMsg, const size_t &szLen) const;
	void Cleanup();
};
//---------------------------------------------------------------------------

#endif
