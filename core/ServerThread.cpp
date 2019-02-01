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
#include "LanguageManager.h"
#include "ServerManager.h"
#include "serviceLoop.h"
#include "SettingManager.h"
#include "UdpDebug.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "ServerThread.h"
//---------------------------------------------------------------------------

ServerThread::ServerThread(const int &iAddrFamily, const uint16_t &ui16PortNumber) {
#ifdef _WIN32
    server = INVALID_SOCKET;
#else
    server = -1;
#endif

	bActive = false;
	bSuspended = false;
	bTerminated = false;

    threadId = 0;
	iSuspendTime = 0;

    iAdressFamily = iAddrFamily;
    ui16Port = ui16PortNumber;

#ifdef _WIN32
    threadHandle = INVALID_HANDLE_VALUE;

	InitializeCriticalSection(&csServerThread);
#else
	pthread_mutex_init(&mtxServerThread, NULL);
#endif

    AntiFloodList = NULL;

    prev = NULL;
    next = NULL;
}
//---------------------------------------------------------------------------

ServerThread::~ServerThread() {
#ifdef _WIN32
    DeleteCriticalSection(&csServerThread);
#else
    if(threadId != 0) {
        Close();
        WaitFor();
    }

    pthread_mutex_destroy(&mtxServerThread);
#endif
        
    AntiConFlood *acfnext = AntiFloodList;
        
    while(acfnext != NULL) {
        AntiConFlood *acfcur = acfnext;
        acfnext = acfcur->next;
		delete acfcur;
    }

#ifdef _WIN32
    if(threadHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(threadHandle);
    }
#endif
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	unsigned __stdcall ExecuteServerThread(void* SrvThread) {
#else
	static void* ExecuteServerThread(void* SrvThread) {
#endif
	((ServerThread *)SrvThread)->Run();
	return 0;
}
//---------------------------------------------------------------------------

void ServerThread::Resume() {
#ifdef _WIN32
    threadHandle = (HANDLE)_beginthreadex(NULL, 0, ExecuteServerThread, this, 0, &threadId);
    if(threadHandle == 0) {
#else
    int iRet = pthread_create(&threadId, NULL, ExecuteServerThread, this);
    if(iRet != 0) {
#endif
		AppendDebugLog("%s - [ERR] Failed to create new ServerThread\n", 0);
    }
}
//---------------------------------------------------------------------------

void ServerThread::Run() {
    bActive = true;
#ifdef _WIN32
    SOCKET s;
#else
	int s;
#endif
    sockaddr_storage addr;
	socklen_t len = sizeof(addr);

#ifndef _WIN32
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 1000000;
#endif

	while(bTerminated == false) {
		s = accept(server, (struct sockaddr *)&addr, &len);

		if(iSuspendTime == 0) {
			if(bTerminated == true) {
#ifdef _WIN32
				if(s != INVALID_SOCKET)
					shutdown(s, SD_SEND);
                        
				closesocket(s);
#else
                if(s != -1)
                    shutdown(s, SHUT_RDWR);
                        
				close(s);
#endif
				continue;
			}

#ifdef _WIN32
			if(s == INVALID_SOCKET) {
				if(WSAEWOULDBLOCK != WSAGetLastError()) {
#else
            if(s == -1) {
                if(errno != EWOULDBLOCK) {
                    if(errno == EMFILE) { // max opened file descriptors limit reached
                        sleep(1); // longer sleep give us better chance to have free file descriptor available on next accept call
                    } else {
#endif
						clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG, 
                            ("[ERR] accept() for port "+string(ui16Port)+" has returned error.").c_str());
                    }
#ifndef _WIN32
				}
#endif
			} else {
				if(isFlooder(s, addr) == true) {
#ifdef _WIN32
					shutdown(s, SD_SEND);
					closesocket(s);
#else
                    shutdown(s, SHUT_RDWR);
                    close(s);
#endif
				}

#ifdef _WIN32
				::Sleep(1);
#else
                nanosleep(&sleeptime, NULL);
#endif
			}
		} else {
			uint32_t iSec = 0;
			while(bTerminated == false) {
				if(iSuspendTime > iSec) {
#ifdef _WIN32
					::Sleep(1000);
#else
					sleep(1);
#endif
					if(bSuspended == false) {
						iSec++;
					}
					continue;
				}

#ifdef _WIN32
				EnterCriticalSection(&csServerThread);
#else
				pthread_mutex_lock(&mtxServerThread);
#endif
				iSuspendTime = 0;
#ifdef _WIN32
				LeaveCriticalSection(&csServerThread);
#else
				pthread_mutex_unlock(&mtxServerThread);
#endif

				if(Listen(true) == true) {
					clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG, 
						("[SYS] Server socket for port "+string(ui16Port)+" sucessfully recovered from suspend state.").c_str());
				} else {
					Close();
				}
				break;
			}
		}
	}

    bActive = false;
}
//---------------------------------------------------------------------------

void ServerThread::Close() {
    bTerminated = true;
#ifdef _WIN32
	closesocket(server);
#else
    shutdown(server, SHUT_RDWR);
	close(server);
#endif
}
//---------------------------------------------------------------------------

void ServerThread::WaitFor() {
#ifdef _WIN32
    WaitForSingleObject(threadHandle, INFINITE);
#else
	if(threadId != 0) {
		pthread_join(threadId, NULL);
        threadId = 0;
	}
#endif
}
//---------------------------------------------------------------------------

bool ServerThread::Listen(bool bSilent/* = false*/) {
    server = socket(iAdressFamily, SOCK_STREAM, IPPROTO_TCP);
#ifdef _WIN32
    if(server == INVALID_SOCKET) {
#else
    if(server == -1) {
#endif
        if(bSilent == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG, 
#ifdef _WIN32
                ("[ERR] Unable to create server socket for port "+string(ui16Port)+" ! ErrorCode "+string(WSAGetLastError())).c_str());
#else
				("[ERR] Unable to create server socket for port "+string(ui16Port)+" ! ErrorCode "+string(errno)).c_str());
#endif
		} else {
#ifdef _BUILD_GUI
            ::MessageBox(NULL, (string(clsLanguageManager::mPtr->sTexts[LAN_UNB_CRT_SRVR_SCK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_UNB_CRT_SRVR_SCK]) + " " +
				string(ui16Port) + " ! " + clsLanguageManager::mPtr->sTexts[LAN_ERROR_CODE] + " " + string(WSAGetLastError())).c_str(), clsServerManager::sTitle.c_str(), MB_OK | MB_ICONERROR);
#else
            AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_UNB_CRT_SRVR_SCK], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_UNB_CRT_SRVR_SCK]) + " " +
				string(ui16Port) + " ! " + clsLanguageManager::mPtr->sTexts[LAN_ERROR_CODE] + " " + string(errno));
#endif
        }
        return false;
    }

#ifndef _WIN32
    int on = 1;
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        if(bSilent == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG,
                ("[ERR] Server socket setsockopt error: " + string(errno)+" for port: "+string(ui16Port)).c_str());
        } else {
            AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_SRV_SCKOPT_ERR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SRV_SCKOPT_ERR])+
                ": " + string(ErrnoStr(errno))+" (" + string(errno)+") "+
                string(clsLanguageManager::mPtr->sTexts[LAN_FOR_PORT_LWR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FOR_PORT_LWR])+": "+string(ui16Port));
        }
        close(server);
        return false;
    }
#endif

    // set the socket properties
    sockaddr_storage sas;
    memset(&sas, 0, sizeof(sockaddr_storage));
    socklen_t sas_len;

    if(iAdressFamily == AF_INET6) {
        ((struct sockaddr_in6 *)&sas)->sin6_family = AF_INET6;
        ((struct sockaddr_in6 *)&sas)->sin6_port = htons(ui16Port);
        sas_len = sizeof(struct sockaddr_in6);

        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true && clsServerManager::sHubIP6[0] != '\0') {
#ifdef _WIN32
            win_inet_pton(clsServerManager::sHubIP6, &((struct sockaddr_in6 *)&sas)->sin6_addr);
#else
            inet_pton(AF_INET6, clsServerManager::sHubIP6, &((struct sockaddr_in6 *)&sas)->sin6_addr);
#endif
        } else {
            ((struct sockaddr_in6 *)&sas)->sin6_addr = in6addr_any;

            if(iAdressFamily == AF_INET6 && clsServerManager::bIPv6DualStack == true) {
#ifdef _WIN32
                DWORD dwIPv6 = 0;
                setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&dwIPv6, sizeof(dwIPv6));
#else
                int iIPv6 = 0;
                setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &iIPv6, sizeof(iIPv6));
#endif
            }
        }
    } else {
        ((struct sockaddr_in *)&sas)->sin_family = AF_INET;
        ((struct sockaddr_in *)&sas)->sin_port = htons(ui16Port);
        sas_len = sizeof(struct sockaddr_in);

        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true && clsServerManager::sHubIP[0] != '\0') {
            ((struct sockaddr_in *)&sas)->sin_addr.s_addr = inet_addr(clsServerManager::sHubIP);
        } else {
            ((struct sockaddr_in *)&sas)->sin_addr.s_addr = INADDR_ANY;
        }
    }

    // bind it
#ifdef _WIN32
    if(bind(server, (struct sockaddr *)&sas, sas_len) == SOCKET_ERROR) {
    	int err = WSAGetLastError();
#else
	if(bind(server, (struct sockaddr *)&sas, sas_len) == -1) {
#endif
        if(bSilent == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG, 
#ifdef _WIN32
                ("[ERR] Server socket bind error: " + string(WSErrorStr(err))+" ("+string(err)+") for port: "+string(ui16Port)).c_str());
#else
				("[ERR] Server socket bind error: " + string(ErrnoStr(errno))+" ("+string(errno)+") for port: "+string(ui16Port)).c_str());
#endif
		} else {
#ifdef _BUILD_GUI
			::MessageBox(NULL, (string(clsLanguageManager::mPtr->sTexts[LAN_SRV_BIND_ERR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SRV_BIND_ERR]) +
				": " + string(WSErrorStr(err)) + " (" + string(err) + ") " + clsLanguageManager::mPtr->sTexts[LAN_FOR_PORT_LWR] + ": " + string(ui16Port)).c_str(), clsServerManager::sTitle.c_str(), MB_OK | MB_ICONERROR);
#else
            AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_SRV_BIND_ERR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SRV_BIND_ERR])+
#ifdef _WIN32
				": " + string(WSErrorStr(err))+" (" + string(err)+") "+
#else
				": " + string(ErrnoStr(errno))+" (" + string(errno)+") "+
#endif
				string(clsLanguageManager::mPtr->sTexts[LAN_FOR_PORT_LWR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FOR_PORT_LWR])+": "+string(ui16Port));
#endif
        }
#ifdef _WIN32
		closesocket(server);
#else
        close(server);
#endif
        return false;
    }

    // set listen mode
#ifdef _WIN32
    if(listen(server, 512) == SOCKET_ERROR) {
	    int err = WSAGetLastError();
#else
	if(listen(server, 512) == -1) {
#endif
        if(bSilent == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_SRVTHREAD_MSG, 
#ifdef _WIN32
                ("[ERR] Server socket listen() error: " + string(WSErrorStr(err))+" ("+string(err)+") for port: "+string(ui16Port)).c_str());
#else
				("[ERR] Server socket listen() error: " + string(errno)+" for port: "+string(ui16Port)).c_str());
#endif
        } else {
#ifdef _BUILD_GUI
            ::MessageBox(NULL, (string(clsLanguageManager::mPtr->sTexts[LAN_SRV_LISTEN_ERR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SRV_LISTEN_ERR]) +
				": " + string(WSErrorStr(err)) + " (" + string(err) + ") " + clsLanguageManager::mPtr->sTexts[LAN_FOR_PORT_LWR] + ": " + string(ui16Port)).c_str(), clsServerManager::sTitle.c_str(), MB_OK | MB_ICONERROR);
#else
            AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_SRV_LISTEN_ERR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_SRV_LISTEN_ERR])+
				": " + string(errno)+" "+
				string(clsLanguageManager::mPtr->sTexts[LAN_FOR_PORT_LWR], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_FOR_PORT_LWR])+": "+string(ui16Port));
#endif
        }
#ifdef _WIN32
		closesocket(server);
#else
		close(server);
#endif
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
bool ServerThread::isFlooder(const SOCKET &s, const sockaddr_storage &addr) {
#else
bool ServerThread::isFlooder(const int &s, const sockaddr_storage &addr) {
#endif
    uint8_t ui128IpHash[16];
    memset(ui128IpHash, 0, 16);

    if(addr.ss_family == AF_INET6) {
        memcpy(ui128IpHash, &((struct sockaddr_in6 *)&addr)->sin6_addr, 16);
    } else {
        ui128IpHash[10] = 255;
        ui128IpHash[11] = 255;
        memcpy(ui128IpHash, &((struct sockaddr_in *)&addr)->sin_addr.s_addr, 4);
    }

    int16_t iConDefloodCount = clsSettingManager::mPtr->GetShort(SETSHORT_NEW_CONNECTIONS_COUNT);
    int16_t iConDefloodTime = clsSettingManager::mPtr->GetShort(SETSHORT_NEW_CONNECTIONS_TIME);
   
    AntiConFlood *nxt = AntiFloodList;
	while(nxt != NULL) {
		AntiConFlood *cur = nxt;
		nxt = cur->next;

    	if(memcmp(ui128IpHash, cur->ui128IpHash, 16) == 0) {
            if(cur->Time+((uint64_t)iConDefloodTime) >= clsServerManager::ui64ActualTick) {
                cur->hits++;
                if(cur->hits > iConDefloodCount) {
                    return true;
                } else {
                    clsServiceLoop::mPtr->AcceptSocket(s, addr);
                    return false;
                }
            } else {
                RemoveConFlood(cur);
                delete cur;
            }
        } else if(cur->Time+((uint64_t)iConDefloodTime) < clsServerManager::ui64ActualTick) {
            RemoveConFlood(cur);
            delete cur;
        }
    }

    AntiConFlood * pNewItem = new AntiConFlood();
    if(pNewItem == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate pNewItem  in theLoop::isFlooder\n", 0);
    	return true;
    }

    memcpy(pNewItem->ui128IpHash, ui128IpHash, 16);

    pNewItem->Time = clsServerManager::ui64ActualTick;

    pNewItem->prev = NULL;
    pNewItem->next = AntiFloodList;

    pNewItem->hits = 1;

    if(AntiFloodList != NULL) {
        AntiFloodList->prev = pNewItem;
    }
    AntiFloodList = pNewItem;

    clsServiceLoop::mPtr->AcceptSocket(s, addr);

    return false;
}
//---------------------------------------------------------------------------

void ServerThread::RemoveConFlood(AntiConFlood *cur) {
    if(cur->prev == NULL) {
        if(cur->next == NULL) {
            AntiFloodList = NULL;
        } else {
            cur->next->prev = NULL;
            AntiFloodList = cur->next;
        }
    } else if(cur->next == NULL) {
        cur->prev->next = NULL;
    } else {
        cur->prev->next = cur->next;
        cur->next->prev = cur->prev;
    }
}
//---------------------------------------------------------------------------

void ServerThread::ResumeSck() {
    if(bActive == true) {
#ifdef _WIN32
        EnterCriticalSection(&csServerThread);
#else
		pthread_mutex_lock(&mtxServerThread);
#endif
        bSuspended = false;
        iSuspendTime = 0;
#ifdef _WIN32
        LeaveCriticalSection(&csServerThread);
#else
		pthread_mutex_unlock(&mtxServerThread);
#endif
    }
}
//---------------------------------------------------------------------------

void ServerThread::SuspendSck(const uint32_t &iTime) {
    if(bActive == true) {
#ifdef _WIN32
        EnterCriticalSection(&csServerThread);
#else
		pthread_mutex_lock(&mtxServerThread);
#endif
        if(iTime != 0) {
            iSuspendTime = iTime;
        } else {
            bSuspended = true;
            iSuspendTime = 1;            
        }
#ifdef _WIN32
        LeaveCriticalSection(&csServerThread);
    	closesocket(server);
#else
        pthread_mutex_unlock(&mtxServerThread);
    	close(server);
#endif
    }
}
//---------------------------------------------------------------------------
