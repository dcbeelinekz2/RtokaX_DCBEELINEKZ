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
#include "eventqueue.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "UDPThread.h"
//---------------------------------------------------------------------------
UDPThread * UDPThread::mPtrIPv4 = NULL;
UDPThread * UDPThread::mPtrIPv6 = NULL;
//---------------------------------------------------------------------------

UDPThread::UDPThread() {
#ifdef _WIN32
    sock = INVALID_SOCKET;
#else
    sock = -1;
#endif

    rcvbuf[0] = '\0';

    threadId = 0;

#ifdef _WIN32
    threadHandle = INVALID_HANDLE_VALUE;
#endif

	bTerminated = false;
}

bool UDPThread::Listen(const int &iAddressFamily) {
    sock = socket(iAddressFamily, SOCK_DGRAM, IPPROTO_UDP);

#ifdef _WIN32
	if(sock == INVALID_SOCKET) {
#else
	if(sock == -1) {
#endif
		AppendLog("[ERR] UDP Socket creation error.");
		return false;
    }

#ifndef _WIN32
    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif

    sockaddr_storage sas;
    memset(&sas, 0, sizeof(sockaddr_storage));
    socklen_t sas_len;

    if(iAddressFamily == AF_INET6) {
        ((struct sockaddr_in6 *)&sas)->sin6_family = AF_INET6;
        ((struct sockaddr_in6 *)&sas)->sin6_port = htons((unsigned short)atoi(clsSettingManager::mPtr->sTexts[SETTXT_UDP_PORT]));
        sas_len = sizeof(struct sockaddr_in6);

        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true && clsServerManager::sHubIP6[0] != '\0') {
#ifdef _WIN32
            win_inet_pton(clsServerManager::sHubIP6, &((struct sockaddr_in6 *)&sas)->sin6_addr);
#else
            inet_pton(AF_INET6, clsServerManager::sHubIP6, &((struct sockaddr_in6 *)&sas)->sin6_addr);
#endif
        } else {
            ((struct sockaddr_in6 *)&sas)->sin6_addr = in6addr_any;

            if(iAddressFamily == AF_INET6 && clsServerManager::bIPv6DualStack == true) {
#ifdef _WIN32
                DWORD dwIPv6 = 0;
                setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&dwIPv6, sizeof(dwIPv6));
#else
                int iIPv6 = 0;
                setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &iIPv6, sizeof(iIPv6));
#endif
            }
        }
    } else {
        ((struct sockaddr_in *)&sas)->sin_family = AF_INET;
        ((struct sockaddr_in *)&sas)->sin_port = htons((unsigned short)atoi(clsSettingManager::mPtr->sTexts[SETTXT_UDP_PORT]));
        sas_len = sizeof(struct sockaddr_in);

        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true && clsServerManager::sHubIP[0] != '\0') {
            ((struct sockaddr_in *)&sas)->sin_addr.s_addr = inet_addr(clsServerManager::sHubIP);
        } else {
            ((struct sockaddr_in *)&sas)->sin_addr.s_addr = INADDR_ANY;
        }
    }

#ifdef _WIN32
    if(bind(sock, (struct sockaddr *)&sas, sas_len) == SOCKET_ERROR) {
		AppendLog("[ERR] UDP Socket bind error: "+string(WSAGetLastError()));
#else
    if(bind(sock, (struct sockaddr *)&sas, sas_len) == -1) {
		AppendLog("[ERR] UDP Socket bind error: "+string(ErrnoStr(errno))+" ("+string(errno)+")");
#endif
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------

UDPThread::~UDPThread() {
#ifdef _WIN32
    if(sock != INVALID_SOCKET) {
		closesocket(sock);

        sock = INVALID_SOCKET;
    }

    if(threadHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(threadHandle);
#else
    if(threadId != 0) {
        Close();
        WaitFor();
#endif
	}
}
//---------------------------------------------------------------------------

#ifdef _WIN32
unsigned __stdcall ExecuteUDP(void* UDPThrd) {
#else
static void* ExecuteUDP(void* UDPThrd) {
#endif
	((UDPThread *)UDPThrd)->Run();
	return 0;
}
//---------------------------------------------------------------------------

void UDPThread::Resume() {
#ifdef _WIN32
	threadHandle = (HANDLE)_beginthreadex(NULL, 0, ExecuteUDP, this, 0, &threadId);
	if(threadHandle == 0) {
#else
	int iRet = pthread_create(&threadId, NULL, ExecuteUDP, this);
	if(iRet != 0) {
#endif
		AppendDebugLog("%s - [ERR] Failed to create new UDPThread\n", 0);
    }
}
//---------------------------------------------------------------------------

void UDPThread::Run() {
    sockaddr_storage sas;
	socklen_t sas_len = sizeof(sockaddr_storage);

	while(bTerminated == false) {
		int len = recvfrom(sock, rcvbuf, 4095, 0, (struct sockaddr *)&sas, &sas_len);

		if(len < 5 || strncmp(rcvbuf, "$SR ", 4) != 0) {
			continue;
		}

		rcvbuf[len] = '\0';

		// added ip check, we don't want fake $SR causing kick of innocent user...
        clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_UDP_SR, rcvbuf, &sas);
    }
}
//---------------------------------------------------------------------------

void UDPThread::Close() {
	bTerminated = true;
#ifdef _WIN32
	closesocket(sock);
#else
	shutdown(sock, SHUT_RDWR);
	close(sock);
#endif
}
//---------------------------------------------------------------------------

void UDPThread::WaitFor() {
#ifdef _WIN32
    WaitForSingleObject(threadHandle, INFINITE);
#else
	if(threadId != 0) {
		pthread_join(threadId, NULL);
        threadId = 0;
	}
#endif
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

UDPThread * UDPThread::Create(const int &iAddressFamily) {
    UDPThread * pUDPThread = new UDPThread();
    if(pUDPThread == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate pUDPThread in UDPThread::Create\n", 0);
        return NULL;
    }

    if(pUDPThread->Listen(iAddressFamily) == true) {
        pUDPThread->Resume();
        return pUDPThread;
    } else {
        delete pUDPThread;
        return NULL;
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void UDPThread::Destroy(UDPThread * pUDPThread) {
    if(pUDPThread != NULL) {
        pUDPThread->Close();
        pUDPThread->WaitFor();
        delete pUDPThread;
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
