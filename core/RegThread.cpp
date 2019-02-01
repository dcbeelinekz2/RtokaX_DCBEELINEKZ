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
#include "RegThread.h"
//---------------------------------------------------------------------------
clsRegisterThread * clsRegisterThread::mPtr = NULL;
//---------------------------------------------------------------------------

clsRegisterThread::RegSocket::RegSocket() {
#ifdef _WIN32
    sock = INVALID_SOCKET;
#else
    sock = -1;
#endif

    sAddress = NULL;
    sRecvBuf = NULL;
    sSendBuf = NULL;
    sSendBufHead = NULL;

    prev = NULL;
    next = NULL;

    iTotalShare = 0;

    iRecvBufLen = 0;
    iRecvBufSize = 0;
    iSendBufLen = 0;
    iTotalUsers = 0;

    ui32AddrLen = 0;
}
//---------------------------------------------------------------------------

clsRegisterThread::RegSocket::~RegSocket() {
    free(sAddress);
    free(sRecvBuf);
    free(sSendBuf);

#ifdef _WIN32
    shutdown(sock, SD_SEND);
    closesocket(sock);
#else
    shutdown(sock, SHUT_RDWR);
    close(sock);
#endif
}
//---------------------------------------------------------------------------

clsRegisterThread::clsRegisterThread() {
    sMsg[0] = '\0';

    bTerminated = false;

    threadId = 0;

#ifdef _WIN32
    threadHandle = INVALID_HANDLE_VALUE;
#endif

    iBytesRead = 0;
    iBytesSent = 0;
       
    RegSockListS = NULL;
    RegSockListE = NULL;
}
//---------------------------------------------------------------------------

clsRegisterThread::~clsRegisterThread() {
#ifndef _WIN32
    if(threadId != 0) {
        Close();
        WaitFor();
    }
#endif

	clsServerManager::ui64BytesRead += (uint64_t)iBytesRead;
    clsServerManager::ui64BytesSent += (uint64_t)iBytesSent;
    
    RegSocket *next = RegSockListS;
        
    while(next != NULL) {
        RegSocket *cur = next;
        next = cur->next;
                       
        delete cur;
    }

#ifdef _WIN32
    if(threadHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(threadHandle);
    }
#endif
}
//---------------------------------------------------------------------------

void clsRegisterThread::Setup(char * sListAddresses, const uint16_t &ui16AddrsLen) {
    // parse all addresses and create individul RegSockets
    char *sAddresses = (char *)malloc(ui16AddrsLen+1);
    if(sAddresses == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sAddresses in clsRegisterThread::Setup\n", (uint64_t)(ui16AddrsLen+1));

        return;
    }

    memcpy(sAddresses, sListAddresses, ui16AddrsLen);
    sAddresses[ui16AddrsLen] = '\0';

    char *sAddress = sAddresses;

    for(uint16_t ui16i = 0; ui16i < ui16AddrsLen; ui16i++) {
        if(sAddresses[ui16i] == ';') {
            sAddresses[ui16i] = '\0';
        } else if(ui16i != ui16AddrsLen-1) {
            continue;
        } else {
            ui16i++;
        }

        AddSock(sAddress, (sAddresses+ui16i)-sAddress);
        sAddress = sAddresses+ui16i+1;
    }

	free(sAddresses);
}
//---------------------------------------------------------------------------

void clsRegisterThread::AddSock(char * sAddress, const size_t &ui32Len) {
    RegSocket * pNewSock = new RegSocket();
    if(pNewSock == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate pNewSock in clsRegisterThread::AddSock\n", 0);
    	return;
    }

    pNewSock->prev = NULL;
    pNewSock->next = NULL;

    pNewSock->sAddress = (char *)malloc(ui32Len+1);
    if(pNewSock->sAddress == NULL) {
		delete pNewSock;

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sAddress in clsRegisterThread::AddSock\n", (uint64_t)(ui32Len+1));

        return;
    }

    pNewSock->ui32AddrLen = (uint32_t)ui32Len;

    memcpy(pNewSock->sAddress, sAddress, pNewSock->ui32AddrLen);
    pNewSock->sAddress[pNewSock->ui32AddrLen] = '\0';
    
    pNewSock->sRecvBuf = (char *)malloc(256);
    if(pNewSock->sRecvBuf == NULL) {
        delete pNewSock;

		AppendDebugLog("%s - [MEM] Cannot allocate 256 bytes for sRecvBuf in clsRegisterThread::AddSock\n", 0);
        return;
    }

    pNewSock->sSendBuf = NULL;
    pNewSock->sSendBufHead = NULL;

    pNewSock->iRecvBufLen = 0;
    pNewSock->iRecvBufSize = 255;
    pNewSock->iSendBufLen = 0;

    pNewSock->iTotalUsers = 0;
    pNewSock->iTotalShare = 0;

    if(RegSockListS == NULL) {
        pNewSock->prev = NULL;
        pNewSock->next = NULL;
        RegSockListS = pNewSock;
        RegSockListE = pNewSock;
    } else {
        pNewSock->prev = RegSockListE;
        pNewSock->next = NULL;
        RegSockListE->next = pNewSock;
        RegSockListE = pNewSock;
    }
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	unsigned __stdcall ExecuteRegisterThread(void* RThread) {
#else
	static void* ExecuteRegisterThread(void* RThread) {
#endif
	((clsRegisterThread *)RThread)->Run();
	return 0;
}
//---------------------------------------------------------------------------

void clsRegisterThread::Resume() {
#ifdef _WIN32
    threadHandle = (HANDLE)_beginthreadex(NULL, 0, ExecuteRegisterThread, this, 0, &threadId);
    if(threadHandle == 0) {
#else
    int iRet = pthread_create(&threadId, NULL, ExecuteRegisterThread, this);
    if(iRet != 0) {
#endif
		AppendDebugLog("%s - [ERR] Failed to create new RegisterThread\n", 0);
    }
}
//---------------------------------------------------------------------------

void clsRegisterThread::Run() {
#ifndef _WIN32
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = 1000000;
#endif

    RegSocket *next = RegSockListS;
    while(bTerminated == false && next != NULL) {
        RegSocket *cur = next;
        next = cur->next;

        char *port = strchr(cur->sAddress, ':');
        if(port != NULL) {
            port[0] = '\0';
            int32_t iPort = atoi(port+1);

            if(iPort >= 0 && iPort <= 65535) {
                port[0] = ':';
                port = NULL;
            }
        }

        struct addrinfo hints;
        memset(&hints, 0, sizeof(addrinfo));

        hints.ai_socktype = SOCK_STREAM;

        if(clsServerManager::bUseIPv6 == true) {
            hints.ai_family = AF_UNSPEC;
        } else {
            hints.ai_family = AF_INET;
        }

        struct addrinfo * pResult = NULL;

        if(::getaddrinfo(cur->sAddress, port != NULL ? port+1 : "2501", &hints, &pResult) != 0 || (pResult->ai_family != AF_INET && pResult->ai_family != AF_INET6)) {
#ifdef _WIN32
			int iError = WSAGetLastError();
			int iMsgLen = sprintf(sMsg, "[REG] RegSock resolve error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else
			int iMsgLen = sprintf(sMsg, "[REG] RegSock resolve error (%s)", cur->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Execute1") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }

            RemoveSock(cur);
            delete cur;

            if(pResult != NULL) {
                freeaddrinfo(pResult);
            }

            continue;
        }

        if(port != NULL) {
            port[0] = ':';
        }

        // Initialise socket we want to use for connect
#ifdef _WIN32
        if((cur->sock = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol)) == INVALID_SOCKET) {
            int iError = WSAGetLastError();
            int iMsgLen = sprintf(sMsg, "[REG] RegSock create error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else
        if((cur->sock = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol)) == -1) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock create error %d. (%s)", errno, cur->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::AddSock1") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }

            RemoveSock(cur);
            delete cur;
            freeaddrinfo(pResult);

            return;
        }

        // Set the receive buffer
#ifdef _WIN32
        int32_t bufsize = 1024;
        if(setsockopt(cur->sock, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
            int iError = WSAGetLastError();
            int iMsgLen = sprintf(sMsg, "[REG] RegSock recv buff error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else
        int bufsize = 1024;
        if(setsockopt(cur->sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) == -1) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock recv buff error %d. (%s)", errno, cur->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::AddSock2") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }

            RemoveSock(cur);
            delete cur;
            freeaddrinfo(pResult);

            return;
        }

        // Set the send buffer
        bufsize = 2048;
#ifdef _WIN32
        if(setsockopt(cur->sock, SOL_SOCKET, SO_SNDBUF, (char *) &bufsize, sizeof(bufsize)) == SOCKET_ERROR) {
            int iError = WSAGetLastError();
            int iMsgLen = sprintf(sMsg, "[REG] RegSock send buff error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else
        if(setsockopt(cur->sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) == -1) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock send buff error %d. (%s)", errno, cur->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::AddSock3") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }

            RemoveSock(cur);
            delete cur;
            freeaddrinfo(pResult);

            return;
        }

        // Set non-blocking mode
#ifdef _WIN32
        uint32_t block = 1;
        if(ioctlsocket(cur->sock, FIONBIO, (unsigned long *)&block) == SOCKET_ERROR) {
            int iError = WSAGetLastError();
            int iMsgLen = sprintf(sMsg, "[REG] RegSock non-block error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else
        int oldFlag = fcntl(cur->sock, F_GETFL, 0);
        if(fcntl(cur->sock, F_SETFL, oldFlag | O_NONBLOCK) == -1) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock non-block error %d. (%s)", errno, cur->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::AddSock4") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }

            RemoveSock(cur);
            delete cur;
            freeaddrinfo(pResult);

            return;
        }

        // Finally, time to connect ! ;)
#ifdef _WIN32
		if(connect(cur->sock, pResult->ai_addr, (int)pResult->ai_addrlen) == SOCKET_ERROR) {
			int iError = WSAGetLastError();
			if(iError != WSAEWOULDBLOCK) {
				int iMsgLen = sprintf(sMsg, "[REG] RegSock connect error %s (%d). (%s)", WSErrorStr(iError), iError, cur->sAddress);
#else  
		if(connect(cur->sock, pResult->ai_addr, (int)pResult->ai_addrlen) == -1) {
			if(errno != EINPROGRESS) {
				int iMsgLen = sprintf(sMsg, "[REG] RegSock connect error %s (%d). (%s)", ErrnoStr(errno), errno, cur->sAddress);
#endif
                if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Execute2") == true) {
                    clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
                }

                RemoveSock(cur);
                delete cur;
                ::freeaddrinfo(pResult);

                continue;
            }
        }

        ::freeaddrinfo(pResult);

#ifdef _WIN32
		::Sleep(1);
#else
        nanosleep(&sleeptime, NULL);
#endif
	}

	uint16_t iLoops = 0;

#ifndef _WIN32
    sleeptime.tv_nsec = 75000000;
#endif

    while(RegSockListS != NULL && bTerminated == false && iLoops < 4000) {   
        iLoops++;
  
        next = RegSockListS;
     
        while(next != NULL) {
            RegSocket *cur = next;
            next = cur->next;

            if(Receive(cur) == false) {
                RemoveSock(cur);
                delete cur;
                continue;
            }

            if(cur->iSendBufLen > 0) {
                if(Send(cur) == false) {
                    RemoveSock(cur);
                    delete cur;
                    continue;
                }
            }
        }
#ifdef _WIN32
		::Sleep(75);
#else
        nanosleep(&sleeptime, NULL);
#endif
	}

    next = RegSockListS;
    while(next != NULL) {
        RegSocket *cur = next;
        next = cur->next;

        if(bTerminated == false) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock timeout. (%s)", cur->sAddress);
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Execute3") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }
        }

        delete cur;
    }

    RegSockListS = NULL;
	RegSockListE = NULL;
}
//---------------------------------------------------------------------------

void clsRegisterThread::Close() {
    bTerminated = true;
}
//---------------------------------------------------------------------------

void clsRegisterThread::WaitFor() {
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

bool clsRegisterThread::Receive(RegSocket * Sock) {
    if(Sock->sRecvBuf == NULL) {
        return true;
    }

#ifdef _WIN32
	u_long iAvailBytes = 0;
	if(ioctlsocket(Sock->sock, FIONREAD, &iAvailBytes) == SOCKET_ERROR) {
		int iError = WSAGetLastError();
	    int iMsgLen = sprintf(sMsg, "[REG] RegSock ioctlsocket(FIONREAD) error %s (%d). (%s)", WSErrorStr(iError), iError, Sock->sAddress);
#else
	int iAvailBytes = 0;
	if(ioctl(Sock->sock, FIONREAD, &iAvailBytes) == -1) {
	    int iMsgLen = sprintf(sMsg, "[REG] RegSock ioctl(FIONREAD) error %s (%d). (%s)", ErrnoStr(errno), errno, Sock->sAddress);
#endif
        if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive0") == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
        }
        return false;
    }

    if(iAvailBytes == 0) {
        // we need to ry receive to catch connection error, or if user closed connection
        iAvailBytes = 16;
    } else if(iAvailBytes > 1024) {
        // receive max. 1024 bytes to receive buffer
        iAvailBytes = 1024;
    }

    if(Sock->iRecvBufSize < Sock->iRecvBufLen+iAvailBytes) {
        size_t szAllignLen = ((Sock->iRecvBufLen+iAvailBytes+1) & 0xFFFFFE00) + 0x200;
        if(szAllignLen > 2048) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock receive buffer overflow. (%s)", Sock->sAddress);
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive5") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }
            return false;
        }

        char * oldbuf = Sock->sRecvBuf;

        Sock->sRecvBuf = (char *)realloc(oldbuf, szAllignLen);
        if(Sock->sRecvBuf == NULL) {
            free(oldbuf);

			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for sRecvBuf in clsRegisterThread::Receive\n", (uint64_t)szAllignLen);

            return false;
        }

		Sock->iRecvBufSize = (uint32_t)(szAllignLen-1);
    }

    int iBytes = recv(Sock->sock, Sock->sRecvBuf+Sock->iRecvBufLen, Sock->iRecvBufSize-Sock->iRecvBufLen, 0);
    
#ifdef _WIN32
    if(iBytes == SOCKET_ERROR) {
        int iError = WSAGetLastError();
        if(iError != WSAEWOULDBLOCK) {                  
			if(iError == WSAENOTCONN) {
#else
    if(iBytes == -1) {
        if(errno != EAGAIN) {
			if(errno == ENOTCONN) {
#endif
				int iErr = 0;
				socklen_t iErrLen = sizeof(iErr);

            	int iRet = getsockopt(Sock->sock, SOL_SOCKET, SO_ERROR, (char *) &iErr, &iErrLen);
#ifdef _WIN32
				if(iRet == SOCKET_ERROR) {
					int iMsgLen = sprintf(sMsg, "[REG] RegSock getsockopt error %s (%d). (%s)", WSErrorStr(iRet), iRet, Sock->sAddress);
#else
				if(iRet == -1) {
					int iMsgLen = sprintf(sMsg, "[REG] RegSock getsockopt error %d. (%s)", iRet, Sock->sAddress);
#endif
					if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive1") == true) {
						clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
					}

					return false;
				} else if(iErr != 0) {
#ifdef _WIN32
					int iMsgLen = sprintf(sMsg, "[REG] RegSock connect error %s (%d). (%s)", WSErrorStr(iErr), iErr, Sock->sAddress);
#else
					int iMsgLen = sprintf(sMsg, "[REG] RegSock connect error %d. (%s)", iErr, Sock->sAddress);
#endif
					if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive2") == true) {
						clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
					}

					return false;
				}
				return true;
			}

#ifdef _WIN32
            int iMsgLen = sprintf(sMsg, "[REG] RegSock recv error %s (%d). (%s)", WSErrorStr(iError), iError, Sock->sAddress);
#else
			int iMsgLen = sprintf(sMsg, "[REG] RegSock recv error %s (%d). (%s)", ErrnoStr(errno), errno, Sock->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive3") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }
            return false;
        } else {
            return true;
        }
    } else if(iBytes == 0) {
        int iMsgLen = sprintf(sMsg, "[REG] RegSock closed connection by server. (%s)", Sock->sAddress);
        if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive4") == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
        }
        return false;
    }
    
    iBytesRead += iBytes;

    Sock->iRecvBufLen += iBytes;
    Sock->sRecvBuf[Sock->iRecvBufLen] = '\0';
    char *sBuffer = Sock->sRecvBuf;

    for(uint32_t ui32i = 0; ui32i < Sock->iRecvBufLen; ui32i++) {
        if(Sock->sRecvBuf[ui32i] == '|') {
            uint32_t ui32CommandLen = (uint32_t)(((Sock->sRecvBuf+ui32i)-sBuffer)+1);
            if(strncmp(sBuffer, "$Lock ", 6) == 0) {
                sockaddr_in addr;
				socklen_t addrlen = sizeof(addr);
#ifdef _WIN32
                if(getsockname(Sock->sock, (struct sockaddr *) &addr, &addrlen) == SOCKET_ERROR) {
                    int iError = WSAGetLastError();
                    int iMsgLen = sprintf(sMsg, "[REG] RegSock local port error %s (%d). (%s)", WSErrorStr(iError), iError, Sock->sAddress);
#else
                if(getsockname(Sock->sock, (struct sockaddr *) &addr, &addrlen) == -1) {
                    int iMsgLen = sprintf(sMsg, "[REG] RegSock local port error %d. (%s)", errno, Sock->sAddress);
#endif
                    if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Receive6") == true) {
                        clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
                    }
                    return false;
                }

                uint16_t ui16port =  (uint16_t)ntohs(addr.sin_port);
                char cMagic = (char) ((ui16port&0xFF)+((ui16port>>8)&0xFF));

                // strip the Lock data
                char *temp;
                if((temp = strchr(sBuffer+6, ' ')) != NULL) {
                    temp[0] = '\0';
                }

                // Compute the key
                memcpy(sMsg, "$Key ", 5);
                sMsg[5] = '\0';
                size_t szLen = temp-sBuffer;
                char v;

                // first make the crypting stuff
                for(size_t szi = 6; szi < szLen; szi++) {
                    if(szi == 6) {
                        v = sBuffer[szi] ^ sBuffer[szLen] ^ sBuffer[szLen-1] ^ cMagic;
                    } else {
                        v = sBuffer[szi] ^ sBuffer[szi-1];
                    }

                    // Swap nibbles (0xF0 = 11110000, 0x0F = 00001111)
                    v = (char)(((v << 4) & 0xF0) | ((v >> 4) & 0x0F));

                    switch(v) {
                        case 0:
                            strcat(sMsg, "/%DCN000%/");
                            break;
                        case 5:
                            strcat(sMsg, "/%DCN005%/");
                            break;
                        case 36:
                            strcat(sMsg, "/%DCN036%/");
                            break;
                        case 96:
                            strcat(sMsg, "/%DCN096%/");
                            break;
                        case 124:
                            strcat(sMsg, "/%DCN124%/");
                            break;
                        case 126:
                            strcat(sMsg, "/%DCN126%/");
                            break;
                        default:
                            strncat(sMsg, (char *)&v, 1);
                            break;
                    }
                }

                Sock->iTotalUsers = clsServerManager::ui32Logged;
                Sock->iTotalShare = clsServerManager::ui64TotalShare;

                strcat(sMsg, "|");
                clsSettingManager::mPtr->GetText(SETTXT_HUB_NAME, sMsg);
                strcat(sMsg, "|");
                clsSettingManager::mPtr->GetText(SETTXT_HUB_ADDRESS, sMsg);
                uint16_t ui16FirstPort = clsSettingManager::mPtr->GetFirstPort();
                if(ui16FirstPort != 411) {
                    strcat(sMsg, ":");
                    strcat(sMsg, string(ui16FirstPort).c_str());
                }
                strcat(sMsg, "|");
                clsSettingManager::mPtr->GetText(SETTXT_HUB_DESCRIPTION, sMsg);
                strcat(sMsg, "     |");
                szLen = strlen(sMsg);
                *(sMsg+szLen-4) = 'x';
                *(sMsg+szLen-3) = '.';
                *(sMsg+szLen-5) = 'p';
                *(sMsg+szLen-6) = '.';
                if(clsSettingManager::mPtr->GetBool(SETBOOL_ANTI_MOGLO) == true) {
                    *(sMsg+szLen-2) = (char)160;
                }
                strcat(sMsg, string(Sock->iTotalUsers).c_str());
                strcat(sMsg, "|");
                strcat(sMsg, string(Sock->iTotalShare).c_str());
                strcat(sMsg, "|");
                Add2SendBuf(Sock, sMsg);

                free(Sock->sRecvBuf);
                Sock->sRecvBuf = NULL;
                Sock->iRecvBufLen = 0;
                Sock->iRecvBufSize = 0;
                return true;
            }
            sBuffer += ui32CommandLen;
        }
    }

    Sock->iRecvBufLen -= (uint32_t)(sBuffer-Sock->sRecvBuf);

    if(Sock->iRecvBufLen == 0) {
        Sock->sRecvBuf[0] = '\0';
    } else if(Sock->iRecvBufLen != 1) {
        memmove(Sock->sRecvBuf, sBuffer, Sock->iRecvBufLen);
        Sock->sRecvBuf[Sock->iRecvBufLen] = '\0';
    } else {
        if(sBuffer[0] == '|') {
            Sock->sRecvBuf[0] = '\0';
            Sock->iRecvBufLen = 0;
        } else {
            Sock->sRecvBuf[0] = sBuffer[0];
            Sock->sRecvBuf[1] = '\0';
        }
    }
    return true;
}
//---------------------------------------------------------------------------

void clsRegisterThread::Add2SendBuf(RegSocket * Sock, char * sData) {
    size_t szLen = strlen(sData);
    
    Sock->sSendBuf = (char *)malloc(szLen+1);
    if(Sock->sSendBuf == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sSendBuf in clsRegisterThread::Add2SendBuf\n", (uint64_t)(szLen+1));

        return;
    }

    Sock->sSendBufHead = Sock->sSendBuf;

    memcpy(Sock->sSendBuf, sData, szLen);
    Sock->iSendBufLen = (uint32_t)szLen;
    Sock->sSendBuf[Sock->iSendBufLen] = '\0';
}
//---------------------------------------------------------------------------

bool clsRegisterThread::Send(RegSocket * Sock) {
    // compute length of unsent data
    uint32_t iOffset = (uint32_t)(Sock->sSendBufHead - Sock->sSendBuf);
#ifdef _WIN32
    int32_t iLen = Sock->iSendBufLen - iOffset;
#else
	int iLen = Sock->iSendBufLen - iOffset;
#endif

    int iBytes = send(Sock->sock, Sock->sSendBufHead, iLen, 0);

#ifdef _WIN32
    if(iBytes == SOCKET_ERROR) {
    	int iError = WSAGetLastError();
        if(iError != WSAEWOULDBLOCK) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock send error %s (%d). (%s)", WSErrorStr(iError), iError, Sock->sAddress);
#else
    if(iBytes == -1) {
        if(errno != EAGAIN) {
            int iMsgLen = sprintf(sMsg, "[REG] RegSock send error %s (%d). (%s)", ErrnoStr(errno), errno, Sock->sAddress);
#endif
            if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Send1") == true) {
                clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
            }
            return false;
        } else {
            return true;
        }
    }

    iBytesSent += iBytes;

	if(iBytes < iLen) {
        Sock->sSendBufHead += iBytes;
		return true;
	} else {
		int iMsgLen = sprintf(sMsg, "[REG] Hub is registered on %s hublist (Users: %u, Share: %" PRIu64 ")", Sock->sAddress, clsServerManager::ui32Logged, clsServerManager::ui64TotalShare);
        if(CheckSprintf(iMsgLen, 2048, "clsRegisterThread::Send2") == true) {
            clsEventQueue::mPtr->AddThread(clsEventQueue::EVENT_REGSOCK_MSG, sMsg);
        }
        // Here is only one send, and when is all send then is time to delete this sock... false do it for us ;)
		return false;
	}
}
//---------------------------------------------------------------------------

void clsRegisterThread::RemoveSock(RegSocket * Sock) {
    if(Sock->prev == NULL) {
        if(Sock->next == NULL) {
            RegSockListS = NULL;
            RegSockListE = NULL;
        } else {
            Sock->next->prev = NULL;
            RegSockListS = Sock->next;
        }
    } else if(Sock->next == NULL) {
        Sock->prev->next = NULL;
        RegSockListE = Sock->prev;
    } else {
        Sock->prev->next = Sock->next;
        Sock->next->prev = Sock->prev;
    }
}
//---------------------------------------------------------------------------
