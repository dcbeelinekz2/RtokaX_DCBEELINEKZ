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
#ifndef eventqueueH
#define eventqueueH
//---------------------------------------------------------------------------

class clsEventQueue {
private:
    struct event {
        char * sMsg;

        event *prev, *next;

        uint8_t ui128IpHash[16];
        uint8_t ui8Id;
    };

    event *NormalE, *ThreadE;

#ifdef _WIN32
	CRITICAL_SECTION csEventQueue;
#else
	pthread_mutex_t mtxEventQueue;
#endif
public:
    static clsEventQueue * mPtr;

	enum {
        EVENT_RESTART, 
        EVENT_RSTSCRIPTS, 
        EVENT_RSTSCRIPT, 
        EVENT_STOPSCRIPT, 
        EVENT_STOP_SCRIPTING, 
        EVENT_SHUTDOWN, 
        EVENT_REGSOCK_MSG, 
        EVENT_SRVTHREAD_MSG, 
        EVENT_UDP_SR
	};

    event *NormalS, *ThreadS;

    clsEventQueue();
    ~clsEventQueue();

    void AddNormal(uint8_t ui8Id, char * sMsg);
    void AddThread(uint8_t ui8Id, char * sMsg, const sockaddr_storage * sas = NULL);
    void ProcessEvents();
};
//---------------------------------------------------------------------------

#endif
