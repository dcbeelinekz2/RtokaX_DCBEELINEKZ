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
#include "ServerManager.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "DcCommands.h"
#include "eventqueue.h"
#include "GlobalDataQueue.h"
#include "hashBanManager.h"
#include "hashUsrManager.h"
#include "hashRegManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ProfileManager.h"
#include "serviceLoop.h"
#include "SettingManager.h"
#include "UdpDebug.h"
#include "utility.h"
#include "ZlibUtility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "HubCommands.h"
#include "IP2Country.h"
#include "LuaScript.h"
#include "RegThread.h"
#include "ResNickManager.h"
#include "ServerThread.h"
#include "TextFileManager.h"
//#include "TLSManager.h"
#include "UDPThread.h"
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
    #include "../gui.win/MainWindow.h"
    #include "../gui.win/MainWindowPageScripts.h"
#endif
//---------------------------------------------------------------------------
#ifndef _WIN32
    #include "regtmrinc.h"
#endif
//---------------------------------------------------------------------------
static ServerThread * ServersE = NULL;
//---------------------------------------------------------------------------

#ifdef _WIN32
    UINT_PTR clsServerManager::sectimer = 0;
    UINT_PTR clsServerManager::regtimer = 0;

	HANDLE clsServerManager::hConsole = NULL, clsServerManager::hLuaHeap = NULL, clsServerManager::hPtokaXHeap = NULL, clsServerManager::hRecvHeap = NULL, clsServerManager::hSendHeap = NULL;
	string clsServerManager::sLuaPath = "", clsServerManager::sOS = "";
#else
    static int SIGSECTMR = SIGRTMIN+1;
	static timer_t sectimer, regtimer;
#endif

string clsServerManager::sPath = "", clsServerManager::sScriptPath = "", clsServerManager::sTitle = "";
size_t clsServerManager::szGlobalBufferSize = 0;
char * clsServerManager::sGlobalBuffer = NULL;
bool clsServerManager::bCmdAutoStart = false, clsServerManager::bCmdNoAutoStart = false, clsServerManager::bCmdNoTray = false, clsServerManager::bUseIPv4 = true,
    clsServerManager::bUseIPv6 = true, clsServerManager::bIPv6DualStack = false;

double clsServerManager::daCpuUsage[60];
double clsServerManager::dCpuUsage = 0;

uint64_t clsServerManager::ui64ActualTick = 0, clsServerManager::ui64TotalShare = 0;
uint64_t clsServerManager::ui64BytesRead = 0, clsServerManager::ui64BytesSent = 0, clsServerManager::ui64BytesSentSaved = 0;
uint64_t clsServerManager::ui64LastBytesRead = 0, clsServerManager::ui64LastBytesSent = 0;
uint64_t clsServerManager::ui64Mins = 0, clsServerManager::ui64Hours = 0, clsServerManager::ui64Days = 0;

#ifndef _WIN32
	uint32_t clsServerManager::ui32CpuCount = 0;
#endif

uint32_t clsServerManager::ui32aUploadSpeed[60], clsServerManager::ui32aDownloadSpeed[60];
uint32_t clsServerManager::ui32Joins = 0, clsServerManager::ui32Parts = 0, clsServerManager::ui32Logged = 0, clsServerManager::ui32Peak = 0;
uint32_t clsServerManager::ui32ActualBytesRead = 0, clsServerManager::ui32ActualBytesSent = 0;
uint32_t clsServerManager::ui32AverageBytesRead = 0, clsServerManager::ui32AverageBytesSent = 0;

ServerThread * clsServerManager::ServersS = NULL;

time_t clsServerManager::tStartTime = 0;

bool clsServerManager::bServerRunning = false, clsServerManager::bServerTerminated = false, clsServerManager::bIsRestart = false, clsServerManager::bIsClose = false;

#ifdef _WIN32
	#ifndef _BUILD_GUI
	    bool clsServerManager::bService = false;
	#else
        HINSTANCE clsServerManager::hInstance;
        HWND clsServerManager::hWndActiveDialog;
	#endif
#else
	bool clsServerManager::bDaemon = false;
#endif

char clsServerManager::sHubIP[16], clsServerManager::sHubIP6[46];

uint8_t clsServerManager::ui8SrCntr = 0, clsServerManager::ui8MinTick = 0;
//---------------------------------------------------------------------------

#ifdef _WIN32
void clsServerManager::OnSecTimer() {
	FILETIME tmpa, tmpb, kernelTimeFT, userTimeFT;
	GetProcessTimes(GetCurrentProcess(), &tmpa, &tmpb, &kernelTimeFT, &userTimeFT);
	int64_t kernelTime = kernelTimeFT.dwLowDateTime | (((int64_t)kernelTimeFT.dwHighDateTime) << 32);
	int64_t userTime = userTimeFT.dwLowDateTime | (((int64_t)userTimeFT.dwHighDateTime) << 32);
	double dcpuSec = double(kernelTime + userTime) / double(10000000I64);
	dCpuUsage = dcpuSec - daCpuUsage[ui8MinTick];
	daCpuUsage[ui8MinTick] = dcpuSec;
#else
static void SecTimerHandler(int /*sig*/) {
    struct rusage rs;

    getrusage(RUSAGE_SELF, &rs);

	double dcpuSec = double(rs.ru_utime.tv_sec) + (double(rs.ru_utime.tv_usec)/1000000) + 
	double(rs.ru_stime.tv_sec) + (double(rs.ru_stime.tv_usec)/1000000);
	clsServerManager::dCpuUsage = dcpuSec - clsServerManager::daCpuUsage[clsServerManager::ui8MinTick];
	clsServerManager::daCpuUsage[clsServerManager::ui8MinTick] = dcpuSec;
#endif

	if(++clsServerManager::ui8MinTick == 60) {
		clsServerManager::ui8MinTick = 0;
    }

#ifdef _WIN32
	if(clsServerManager::bServerRunning == false) {
		return;
	}
#endif

    clsServerManager::ui64ActualTick++;

	clsServerManager::ui32ActualBytesRead = (uint32_t)(clsServerManager::ui64BytesRead - clsServerManager::ui64LastBytesRead);
	clsServerManager::ui32ActualBytesSent = (uint32_t)(clsServerManager::ui64BytesSent - clsServerManager::ui64LastBytesSent);
	clsServerManager::ui64LastBytesRead = clsServerManager::ui64BytesRead;
	clsServerManager::ui64LastBytesSent = clsServerManager::ui64BytesSent;

	clsServerManager::ui32AverageBytesSent -= clsServerManager::ui32aUploadSpeed[clsServerManager::ui8MinTick];
	clsServerManager::ui32AverageBytesRead -= clsServerManager::ui32aDownloadSpeed[clsServerManager::ui8MinTick];

	clsServerManager::ui32aUploadSpeed[clsServerManager::ui8MinTick] = clsServerManager::ui32ActualBytesSent;
	clsServerManager::ui32aDownloadSpeed[clsServerManager::ui8MinTick] = clsServerManager::ui32ActualBytesRead;

	clsServerManager::ui32AverageBytesSent += clsServerManager::ui32aUploadSpeed[clsServerManager::ui8MinTick];
	clsServerManager::ui32AverageBytesRead += clsServerManager::ui32aDownloadSpeed[clsServerManager::ui8MinTick];

#ifdef _BUILD_GUI
    clsMainWindow::mPtr->UpdateStats();
    clsMainWindowPageScripts::mPtr->UpdateMemUsage();
#endif
}
//---------------------------------------------------------------------------

#ifdef _WIN32
    void clsServerManager::OnRegTimer() {
	    if(clsSettingManager::mPtr->bBools[SETBOOL_AUTO_REG] == true && clsSettingManager::mPtr->sTexts[SETTXT_REGISTER_SERVERS] != NULL) {
			// First destroy old hublist reg thread if any
	        if(clsRegisterThread::mPtr != NULL) {
	            clsRegisterThread::mPtr->Close();
	            clsRegisterThread::mPtr->WaitFor();
	            delete clsRegisterThread::mPtr;
	            clsRegisterThread::mPtr = NULL;
	        }
	        
	        // Create hublist reg thread
	        clsRegisterThread::mPtr = new clsRegisterThread();
	        if(clsRegisterThread::mPtr == NULL) {
	        	AppendDebugLog("%s - [MEM] Cannot allocate clsRegisterThread::mPtr in ServerOnRegTimer\n", 0);
	        	return;
	        }
	        
	        // Setup hublist reg thread
	        clsRegisterThread::mPtr->Setup(clsSettingManager::mPtr->sTexts[SETTXT_REGISTER_SERVERS], clsSettingManager::mPtr->ui16TextsLens[SETTXT_REGISTER_SERVERS]);
	        
	        // Start the hublist reg thread
	    	clsRegisterThread::mPtr->Resume();
	    }
	}
#endif
//---------------------------------------------------------------------------

void clsServerManager::Initialize() {
    setlocale(LC_ALL, "");
#ifdef _WIN32
	time_t acctime;
	time(&acctime);
	srand((uint32_t)acctime);

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

    hPtokaXHeap = HeapCreate(HEAP_NO_SERIALIZE, 0x100000, 0);

	if(DirExist((clsServerManager::sPath+"\\cfg").c_str()) == false) {
		CreateDirectory((clsServerManager::sPath+"\\cfg").c_str(), NULL);
	}
	if(DirExist((clsServerManager::sPath+"\\logs").c_str()) == false) {
		CreateDirectory((clsServerManager::sPath+"\\logs").c_str(), NULL);
	}
	if(DirExist((clsServerManager::sPath+"\\scripts").c_str()) == false) {
		CreateDirectory((clsServerManager::sPath+"\\scripts").c_str(), NULL);
    }
	if(DirExist((clsServerManager::sPath+"\\texts").c_str()) == false) {
		CreateDirectory((clsServerManager::sPath+"\\texts").c_str(), NULL);
    }

	clsServerManager::sScriptPath = clsServerManager::sPath + "\\scripts\\";

	clsServerManager::sLuaPath = clsServerManager::sPath + "/";

	char * sLuaPath = clsServerManager::sLuaPath.c_str();
	for(size_t szi = 0; szi < clsServerManager::sPath.size(); szi++) {
		if(sLuaPath[szi] == '\\') {
			sLuaPath[szi] = '/';
		}
	}

    SetupOsVersion();
#else
    time_t acctime;
    time(&acctime);
    srandom(acctime);

	if(DirExist((clsServerManager::sPath+"/logs").c_str()) == false) {
		if(mkdir((clsServerManager::sPath+"/logs").c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
            if(bDaemon == true) {
                syslog(LOG_USER | LOG_ERR, "Creating  of logs directory failed!\n");
            } else {
                printf("Creating  of logs directory failed!");
            }
        }
	}
	if(DirExist((clsServerManager::sPath+"/cfg").c_str()) == false) {
		if(mkdir((clsServerManager::sPath+"/cfg").c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
            AppendLog("Creating of cfg directory failed!");
        }
	}
	if(DirExist((clsServerManager::sPath+"/scripts").c_str()) == false) {
		if(mkdir((clsServerManager::sPath+"/scripts").c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
            AppendLog("Creating of scripts directory failed!");
        }
    }
	if(DirExist((clsServerManager::sPath+"/texts").c_str()) == false) {
		if(mkdir((clsServerManager::sPath+"/texts").c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP) == -1) {
            AppendLog("Creating of texts directory failed!");
        }
    }

	clsServerManager::sScriptPath = clsServerManager::sPath + "/scripts/";

    // get cpu count
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if(fp != NULL) {
        char buf[1024];
        while(fgets(buf, 1024, fp) != NULL) {
            if(strncasecmp (buf, "model name", 10) == 0 || strncmp (buf, "Processor", 9) == 0 || strncmp (buf, "cpu model", 9) == 0) {
                ui32CpuCount++;
            }
        }
    
        fclose(fp);
    }

    if(ui32CpuCount == 0) {
        ui32CpuCount = 1;
    }
#endif
    CreateGlobalBuffer();

    CheckForIPv6();

	clsReservedNicksManager::mPtr = new clsReservedNicksManager();
	if(clsReservedNicksManager::mPtr == NULL) {
	    AppendDebugLog("%s - [MEM] Cannot allocate clsReservedNicksManager::mPtr in ServerInitialize\n", 0);
	    exit(EXIT_FAILURE);
	}

    ui64ActualTick = ui64TotalShare = 0;
	ui64BytesRead = ui64BytesSent = ui64BytesSentSaved = 0;

	ui32ActualBytesRead = ui32ActualBytesSent = ui32AverageBytesRead = ui32AverageBytesSent = 0;

    ui32Joins = ui32Parts = ui32Logged = ui32Peak = 0;

    ServersS = NULL;
    ServersE = NULL;

    tStartTime = 0;

    ui64Mins = ui64Hours = ui64Days = 0;

    bServerRunning = bIsRestart = bIsClose = false;

    sHubIP[0] = '\0';
    sHubIP6[0] = '\0';

    ui8SrCntr = 0;

    clsZlibUtility::mPtr = new clsZlibUtility();
    if(clsZlibUtility::mPtr == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate clsZlibUtility::mPtr in ServerInitialize\n", 0);
    	exit(EXIT_FAILURE);
    }

    ui8MinTick = 0;

    ui64LastBytesRead = ui64LastBytesSent = 0;

	for(uint8_t ui8i = 0 ; ui8i < 60; ui8i++) {
		daCpuUsage[ui8i] = 0;
		ui32aUploadSpeed[ui8i] = 0;
		ui32aDownloadSpeed[ui8i] = 0;
	}

	dCpuUsage = 0.0;

	clsSettingManager::mPtr = new clsSettingManager();
    if(clsSettingManager::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsSettingManager::mPtr in ServerInitialize\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsLanguageManager::mPtr = new clsLanguageManager();
    if(clsLanguageManager::mPtr == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsLanguageManager::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    clsLanguageManager::mPtr->Load();

    clsProfileManager::mPtr = new clsProfileManager();
    if(clsProfileManager::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsProfileManager::mPtr in ServerInitialize\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsRegManager::mPtr = new clsRegManager();
    if(clsRegManager::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsRegManager::mPtr in ServerInitialize\n", 0);
    	exit(EXIT_FAILURE);
    }

    // Load registered users
	clsRegManager::mPtr->Load();

    clsBanManager::mPtr = new clsBanManager();
    if(clsBanManager::mPtr == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsBanManager::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    // load banlist
	clsBanManager::mPtr->Load();

    clsTextFilesManager::mPtr = new clsTextFilesManager();
    if(clsTextFilesManager::mPtr == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsTextFilesManager::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    clsUdpDebug::mPtr = new clsUdpDebug();
    if(clsUdpDebug::mPtr == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsUdpDebug::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    clsScriptManager::mPtr = new clsScriptManager();
    if(clsScriptManager::mPtr == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsScriptManager::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

#ifdef _BUILD_GUI
    clsMainWindow::mPtr = new clsMainWindow();

    if(clsMainWindow::mPtr == NULL || clsMainWindow::mPtr->CreateEx() == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate clsMainWindow::mPtr in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }
#endif

	clsSettingManager::mPtr->UpdateAll();

#ifdef _WIN32
    sectimer = SetTimer(NULL, 0, 1000, NULL);

	if(sectimer == 0) {
		AppendDebugLog("%s - [ERR] Cannot startsectimer in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    regtimer = 0;
#else
    struct sigaction sigactsec;
    sigactsec.sa_handler = SecTimerHandler;
    sigemptyset(&sigactsec.sa_mask);
    sigactsec.sa_flags = 0;

    if(sigaction(SIGSECTMR, &sigactsec, NULL) == -1) {
		AppendDebugLog("%s - [ERR] Cannot create sigaction SIGSECTMR in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    struct sigevent sigevsec;
    sigevsec.sigev_notify = SIGEV_SIGNAL;
    sigevsec.sigev_signo = SIGSECTMR;

    int iRet = timer_create(CLOCK_REALTIME, &sigevsec, &sectimer);
    
	if(iRet == -1) {
		AppendDebugLog("%s - [ERR] Cannot create sectimer in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }

    struct sigevent sigevreg;
    sigevreg.sigev_notify = SIGEV_SIGNAL;
    sigevreg.sigev_signo = SIGREGTMR;

    iRet = timer_create(CLOCK_REALTIME, &sigevreg, &regtimer);
    
	if(iRet == -1) {
		AppendDebugLog("%s - [ERR] Cannot create regtimer in ServerInitialize\n", 0);
        exit(EXIT_FAILURE);
    }
#endif
}
//---------------------------------------------------------------------------

bool clsServerManager::Start() {
    time(&tStartTime);

#ifndef _WIN32
    struct itimerspec sectmrspec;
    sectmrspec.it_interval.tv_sec = 1;
    sectmrspec.it_interval.tv_nsec = 0;
    sectmrspec.it_value.tv_sec = 1;
    sectmrspec.it_value.tv_nsec = 0;

    int iRet = timer_settime(sectimer, 0, &sectmrspec, NULL);
    if(iRet == -1) {
		AppendDebugLog("%s - [ERR] Cannot start sectimer in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }
#endif

    clsSettingManager::mPtr->UpdateAll();

    clsTextFilesManager::mPtr->RefreshTextFiles();

#ifdef _BUILD_GUI
    clsMainWindow::mPtr->EnableStartButton(FALSE);
#endif

    ui64ActualTick = ui64TotalShare = 0;

    ui64BytesRead = ui64BytesSent = ui64BytesSentSaved = 0;

	ui32ActualBytesRead = ui32ActualBytesSent = ui32AverageBytesRead = ui32AverageBytesSent = 0;

    ui32Joins = ui32Parts = ui32Logged = ui32Peak = 0;

    ui64Mins = ui64Hours = ui64Days = 0;

    ui8SrCntr = 0;

    sHubIP[0] = '\0';
    sHubIP6[0] = '\0';

    if(clsSettingManager::mPtr->bBools[SETBOOL_RESOLVE_TO_IP] == true) {
        if(isIP(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS]) == false) {
#ifdef _BUILD_GUI
            clsMainWindow::mPtr->SetStatusValue((string(clsLanguageManager::mPtr->sTexts[LAN_RESOLVING_HUB_ADDRESS], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RESOLVING_HUB_ADDRESS])+"...").c_str());
#endif

            struct addrinfo hints;
            memset(&hints, 0, sizeof(addrinfo));

            if(bUseIPv6 == true) {
                hints.ai_family = AF_UNSPEC;
            } else {
                hints.ai_family = AF_INET;
            }

            struct addrinfo *res;

            if(::getaddrinfo(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS], NULL, &hints, &res) != 0 ||
                (res->ai_family != AF_INET && res->ai_family != AF_INET6)) {
#ifdef _WIN32
            	int err = WSAGetLastError();
	#ifdef _BUILD_GUI
				::MessageBox(clsMainWindow::mPtr->m_hWnd,(string(clsLanguageManager::mPtr->sTexts[LAN_RESOLVING_OF_HOSTNAME], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RESOLVING_OF_HOSTNAME])+
					" '"+string(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS])+"' "+string(clsLanguageManager::mPtr->sTexts[LAN_HAS_FAILED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_HAS_FAILED])+
					".\n"+string(clsLanguageManager::mPtr->sTexts[LAN_ERROR_CODE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_ERROR_CODE])+": "+
					string(WSErrorStr(err))+" ("+string(err)+")\n\n"+
					string(clsLanguageManager::mPtr->sTexts[LAN_CHECK_THE_ADDRESS_PLEASE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_CHECK_THE_ADDRESS_PLEASE])+".").c_str(),
					clsLanguageManager::mPtr->sTexts[LAN_ERROR], MB_OK|MB_ICONERROR);
                clsMainWindow::mPtr->EnableStartButton(TRUE);
	#else
                AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_RESOLVING_OF_HOSTNAME], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RESOLVING_OF_HOSTNAME])+
					" '"+string(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS])+"' "+string(clsLanguageManager::mPtr->sTexts[LAN_HAS_FAILED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_HAS_FAILED])+
					".\n"+string(clsLanguageManager::mPtr->sTexts[LAN_ERROR_CODE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_ERROR_CODE])+": "+
					string(WSErrorStr(err))+" ("+string(err)+")\n\n"+
					string(clsLanguageManager::mPtr->sTexts[LAN_CHECK_THE_ADDRESS_PLEASE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_CHECK_THE_ADDRESS_PLEASE])+".");
	#endif
#else
                AppendLog(string(clsLanguageManager::mPtr->sTexts[LAN_RESOLVING_OF_HOSTNAME], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RESOLVING_OF_HOSTNAME])+
					" '"+string(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS])+"' "+string(clsLanguageManager::mPtr->sTexts[LAN_HAS_FAILED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_HAS_FAILED])+
					".\n"+string(clsLanguageManager::mPtr->sTexts[LAN_CHECK_THE_ADDRESS_PLEASE], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_CHECK_THE_ADDRESS_PLEASE])+".");
#endif
                return false;
            } else {
				Memo("*** "+string(clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS], (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_HUB_ADDRESS])+" "+
					string(clsLanguageManager::mPtr->sTexts[LAN_RESOLVED_SUCCESSFULLY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RESOLVED_SUCCESSFULLY])+".");

                if(bUseIPv6 == true) {
                    struct addrinfo *next = res;
                    while(next != NULL) {
                        if(next->ai_family == AF_INET) {
                            if(((sockaddr_in *)(next->ai_addr))->sin_addr.s_addr != INADDR_ANY) {
                                strcpy(sHubIP, inet_ntoa(((sockaddr_in *)(next->ai_addr))->sin_addr));
                            }
                        } else if(next->ai_family == AF_INET6) {
#ifdef _WIN32
                            win_inet_ntop(&((struct sockaddr_in6 *)next->ai_addr)->sin6_addr, sHubIP6, 46);
#else
                            inet_ntop(AF_INET6, &((struct sockaddr_in6 *)next->ai_addr)->sin6_addr, sHubIP6, 46);
#endif
                        }

                        next = next->ai_next;
                    }
                } else if(((sockaddr_in *)(res->ai_addr))->sin_addr.s_addr != INADDR_ANY) {
                    strcpy(sHubIP, inet_ntoa(((sockaddr_in *)(res->ai_addr))->sin_addr));
                }

                if(sHubIP[0] != '\0') {
                    string msg = "*** "+string(sHubIP);
                    if(sHubIP6[0] != '\0') {
                        msg += " / "+string(sHubIP6);
                    }

				    Memo(msg);
                } else if(sHubIP6[0] != '\0') {
				    Memo("*** "+string(sHubIP6));
                }

				freeaddrinfo(res);
            }
        } else {
            strcpy(sHubIP, clsSettingManager::mPtr->sTexts[SETTXT_HUB_ADDRESS]);
        }
    } else {
        if(clsSettingManager::mPtr->sTexts[SETTXT_IPV4_ADDRESS] != NULL) {
            strcpy(sHubIP, clsSettingManager::mPtr->sTexts[SETTXT_IPV4_ADDRESS]);
        } else {
            sHubIP[0] = '\0';
        }

        if(clsSettingManager::mPtr->sTexts[SETTXT_IPV6_ADDRESS] != NULL) {
            strcpy(sHubIP6, clsSettingManager::mPtr->sTexts[SETTXT_IPV6_ADDRESS]);
        } else {
            sHubIP6[0] = '\0';
        }
    }

    for(uint8_t ui8i = 0; ui8i < 25; ui8i++) {
        if(clsSettingManager::mPtr->iPortNumbers[ui8i] == 0) {
            break;
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true || (bUseIPv6 == true && bIPv6DualStack == false)) {
            if(bUseIPv6 == true) {
                CreateServerThread(AF_INET6, clsSettingManager::mPtr->iPortNumbers[ui8i]);
            }

            CreateServerThread(AF_INET, clsSettingManager::mPtr->iPortNumbers[ui8i]);
        } else {
            CreateServerThread(bUseIPv6 == true ? AF_INET6 : AF_INET, clsSettingManager::mPtr->iPortNumbers[ui8i]);
        }
    }

	if(ServersS == NULL) {
#ifdef _BUILD_GUI
		::MessageBox(clsMainWindow::mPtr->m_hWnd, clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_TCP_PORT_SPECIFIED], clsLanguageManager::mPtr->sTexts[LAN_ERROR], MB_OK|MB_ICONERROR);
        clsMainWindow::mPtr->EnableStartButton(TRUE);
#else
		AppendLog(clsLanguageManager::mPtr->sTexts[LAN_NO_VALID_TCP_PORT_SPECIFIED]);
#endif
        return false;
    }

    AppendLog("Serving started");

//  if(tlsenabled == true) {
/*        TLSManager = new TLSMan();
        if(TLSManager == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot allocate TLSManager in ServerStart\n", 0);
        	exit(EXIT_FAILURE);
        }*/
//  }

    clsIpP2Country::mPtr = new clsIpP2Country();
    if(clsIpP2Country::mPtr == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate clsIpP2Country::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsEventQueue::mPtr = new clsEventQueue();
    if(clsEventQueue::mPtr == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate clsEventQueue::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsHashManager::mPtr = new clsHashManager();
    if(clsHashManager::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsHashManager::mPtr in ServerStart\n", 0);
        exit(EXIT_FAILURE);
    }

    clsUsers::mPtr = new clsUsers();
	if(clsUsers::mPtr == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate clsUsers::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsGlobalDataQueue::mPtr = new clsGlobalDataQueue();
    if(clsGlobalDataQueue::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsGlobalDataQueue::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    clsDcCommands::mPtr = new clsDcCommands();
    if(clsDcCommands::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsDcCommands::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    // add botname to reserved nicks
    clsReservedNicksManager::mPtr->AddReservedNick(clsSettingManager::mPtr->sTexts[SETTXT_BOT_NICK]);
    clsSettingManager::mPtr->UpdateBot();

    // add opchat botname to reserved nicks
    clsReservedNicksManager::mPtr->AddReservedNick(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
    clsSettingManager::mPtr->UpdateOpChat();

    clsReservedNicksManager::mPtr->AddReservedNick(clsSettingManager::mPtr->sTexts[SETTXT_ADMIN_NICK]);

    if((uint16_t)atoi(clsSettingManager::mPtr->sTexts[SETTXT_UDP_PORT]) != 0) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true || (bUseIPv6 == true && bIPv6DualStack == false)) {
            if(bUseIPv6 == true) {
                UDPThread::mPtrIPv6 = UDPThread::Create(AF_INET6);
            }

            UDPThread::mPtrIPv4 = UDPThread::Create(AF_INET);
        } else {
            UDPThread::mPtrIPv6 = UDPThread::Create(bUseIPv6 == true ? AF_INET6 : AF_INET);
        }
    }
    
    if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == true) {
		clsScriptManager::mPtr->Start();
    }

    clsServiceLoop::mPtr = new clsServiceLoop();
    if(clsServiceLoop::mPtr == NULL) {
    	AppendDebugLog("%s - [MEM] Cannot allocate clsServiceLoop::mPtr in ServerStart\n", 0);
    	exit(EXIT_FAILURE);
    }

    // Start the server socket threads
    ServerThread *next = ServersS;
    while(next != NULL) {
        ServerThread *cur = next;
        next = cur->next;

		cur->Resume();
    }

    bServerRunning = true;

    // Call lua_Main
	clsScriptManager::mPtr->OnStartup();

#ifdef _BUILD_GUI
    clsMainWindow::mPtr->SetStatusValue((string(clsLanguageManager::mPtr->sTexts[LAN_RUNNING], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_RUNNING])+"...").c_str());
    clsMainWindow::mPtr->SetStartButtonText(clsLanguageManager::mPtr->sTexts[LAN_STOP_HUB]);
    clsMainWindow::mPtr->EnableStartButton(TRUE);
    clsMainWindow::mPtr->EnableGuiItems(TRUE);
#endif

    //Start the HubRegistration timer
    if(clsSettingManager::mPtr->bBools[SETBOOL_AUTO_REG] == true) {
#ifdef _WIN32
		regtimer = SetTimer(NULL, 0, 901000, NULL);

        if(regtimer == 0) {
#else
        struct itimerspec regtmrspec;
        regtmrspec.it_interval.tv_sec = 901;
        regtmrspec.it_interval.tv_nsec = 0;
        regtmrspec.it_value.tv_sec = 901;
        regtmrspec.it_value.tv_nsec = 0;
    
        iRet = timer_settime(regtimer, 0, &regtmrspec, NULL);
        if(iRet == -1) {
#endif

			AppendDebugLog("%s - [ERR] Cannot start regtimer in ServerStart\n", 0);
        	exit(EXIT_FAILURE);
        }
    }

    return true;
}
//---------------------------------------------------------------------------

void clsServerManager::Stop() {
#ifdef _BUILD_GUI
    clsMainWindow::mPtr->EnableStartButton(FALSE);
#endif
#ifndef _WIN32
    struct itimerspec sectmrspec;
    sectmrspec.it_interval.tv_sec = 0;
    sectmrspec.it_interval.tv_nsec = 0;
    sectmrspec.it_value.tv_sec = 0;
    sectmrspec.it_value.tv_nsec = 0;

    int iRet = timer_settime(sectimer, 0, &sectmrspec, NULL);
    if(iRet == -1) {
		AppendDebugLog("%s - [ERR] Cannot stop sectimer in ServerStop\n", 0);
    	exit(EXIT_FAILURE);
    }
#endif

    char msg[1024];
    int iret = sprintf(msg, "Serving stopped (UL: %" PRIu64 " [%" PRIu64 "], DL: %" PRIu64 ")", ui64BytesSent, ui64BytesSentSaved, ui64BytesRead);
    if(CheckSprintf(iret, 1024, "ServerMan::StopServer") == true) {
        AppendLog(msg);
    }

	//Stop the HubRegistration timer
	if(clsSettingManager::mPtr->bBools[SETBOOL_AUTO_REG] == true) {
#ifdef _WIN32
        if(KillTimer(NULL, regtimer) == 0) {
#else
        struct itimerspec regtmrspec;
        regtmrspec.it_interval.tv_sec = 0;
        regtmrspec.it_interval.tv_nsec = 0;
        regtmrspec.it_value.tv_sec = 0;
        regtmrspec.it_value.tv_nsec = 0;
    
        iRet = timer_settime(regtimer, 0, &regtmrspec, NULL);
        if(iRet == -1) {
#endif
    		AppendDebugLog("%s - [ERR] Cannot stop regtimer in ServerStop\n", 0);
        	exit(EXIT_FAILURE);
        }
    }

    ServerThread *next = ServersS;
    while(next != NULL) {
        ServerThread *cur = next;
        next = cur->next;

		cur->Close();
		cur->WaitFor();

		delete cur;
    }

    ServersS = NULL;
    ServersE = NULL;

	// stop the main hub loop
	if(clsServiceLoop::mPtr != NULL) {
		bServerTerminated = true;
	} else {
		FinalStop(false);
    }
}
//---------------------------------------------------------------------------

void clsServerManager::FinalStop(const bool &bDeleteServiceLoop) {
    if(bDeleteServiceLoop == true) {
		delete clsServiceLoop::mPtr;
		clsServiceLoop::mPtr = NULL;
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_ENABLE_SCRIPTING] == true) {
		clsScriptManager::mPtr->Stop();
    }

    UDPThread::Destroy(UDPThread::mPtrIPv6);
    UDPThread::mPtrIPv6 = NULL;

    UDPThread::Destroy(UDPThread::mPtrIPv4);
    UDPThread::mPtrIPv4 = NULL;

	// delete userlist field
	if(clsUsers::mPtr != NULL) {
		clsUsers::mPtr->DisconnectAll();
		delete clsUsers::mPtr;
		clsUsers::mPtr = NULL;
    }

	delete clsDcCommands::mPtr;
    clsDcCommands::mPtr = NULL;

	// delete hashed userlist manager
    delete clsHashManager::mPtr;
    clsHashManager::mPtr = NULL;

	delete clsGlobalDataQueue::mPtr;
    clsGlobalDataQueue::mPtr = NULL;

    if(clsRegisterThread::mPtr != NULL) {
        clsRegisterThread::mPtr->Close();
        clsRegisterThread::mPtr->WaitFor();
        delete clsRegisterThread::mPtr;
        clsRegisterThread::mPtr = NULL;
    }

	delete clsEventQueue::mPtr;
    clsEventQueue::mPtr = NULL;

	delete clsIpP2Country::mPtr;
    clsIpP2Country::mPtr = NULL;

/*	if(TLSManager != NULL) {
		delete TLSManager;
        TLSManager = NULL;
    }*/

	//userstat  // better here ;)
//    sqldb->FinalizeAllVisits();

#ifdef _BUILD_GUI
    clsMainWindow::mPtr->SetStatusValue((string(clsLanguageManager::mPtr->sTexts[LAN_STOPPED], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_STOPPED])+".").c_str());
    clsMainWindow::mPtr->SetStartButtonText(clsLanguageManager::mPtr->sTexts[LAN_START_HUB]);
    clsMainWindow::mPtr->EnableStartButton(TRUE);
    clsMainWindow::mPtr->EnableGuiItems(FALSE);
#endif

    ui8SrCntr = 0;
    ui32Joins = ui32Parts = ui32Logged = 0;

    clsUdpDebug::mPtr->Cleanup();

#ifdef _WIN32
    HeapCompact(GetProcessHeap(), 0);
    HeapCompact(hPtokaXHeap, 0);
#endif

    bServerRunning = false;

    if(bIsRestart == true) {
        bIsRestart = false;

		// start hub
#ifdef _BUILD_GUI
        if(Start() == false) {
            clsMainWindow::mPtr->SetStatusValue((string(clsLanguageManager::mPtr->sTexts[LAN_READY], (size_t)clsLanguageManager::mPtr->ui16TextsLens[LAN_READY])+".").c_str());
        }
#else
		if(Start() == false) {
            AppendLog("[ERR] Server start failed in ServerFinalStop");
            exit(EXIT_FAILURE);
        }
#endif
    } else if(bIsClose == true) {
		FinalClose();
    }
}
//---------------------------------------------------------------------------

void clsServerManager::FinalClose() {
#ifdef _WIN32
    KillTimer(NULL, sectimer);
#else
    timer_delete(sectimer);
    timer_delete(regtimer);
#endif

	clsBanManager::mPtr->Save(true);

    clsProfileManager::mPtr->SaveProfiles();

    clsRegManager::mPtr->Save();

	clsScriptManager::mPtr->SaveScripts();

	clsSettingManager::mPtr->Save();

    delete clsScriptManager::mPtr;
	clsScriptManager::mPtr = NULL;

    delete clsTextFilesManager::mPtr;
    clsTextFilesManager::mPtr = NULL;

    delete clsProfileManager::mPtr;
    clsProfileManager::mPtr = NULL;

    delete clsUdpDebug::mPtr;
    clsUdpDebug::mPtr = NULL;

    delete clsRegManager::mPtr;
    clsRegManager::mPtr = NULL;

    delete clsBanManager::mPtr;
    clsBanManager::mPtr = NULL;

    delete clsZlibUtility::mPtr;
    clsZlibUtility::mPtr = NULL;

    delete clsLanguageManager::mPtr;
    clsLanguageManager::mPtr = NULL;

    delete clsSettingManager::mPtr;
    clsSettingManager::mPtr = NULL;

    delete clsReservedNicksManager::mPtr;
    clsReservedNicksManager::mPtr = NULL;

#ifdef _BUILD_GUI
    clsMainWindow::mPtr->SaveGuiSettings();
#endif

    DeleteGlobalBuffer();

#ifdef _WIN32
	HeapDestroy(hPtokaXHeap);
	
	WSACleanup();
	
	::PostMessage(NULL, WM_USER+1, 0, 0);
#endif
}
//---------------------------------------------------------------------------

void clsServerManager::UpdateServers() {
    // Remove servers for ports we don't want use anymore
    ServerThread *next = ServersS;
    while(next != NULL) {
        ServerThread *cur = next;
        next = cur->next;

        bool bFound = false;

        for(uint8_t ui8i = 0; ui8i < 25; ui8i++) {
            if(clsSettingManager::mPtr->iPortNumbers[ui8i] == 0) {
                break;
            }

            if(cur->ui16Port == clsSettingManager::mPtr->iPortNumbers[ui8i]) {
                bFound = true;
                break;
            }
        }

        if(bFound == false) {
            if(cur->prev == NULL) {
                if(cur->next == NULL) {
                    ServersS = NULL;
                    ServersE = NULL;
                } else {
                    cur->next->prev = NULL;
                    ServersS = cur->next;
                }
            } else if(cur->next == NULL) {
                cur->prev->next = NULL;
                ServersE = cur->prev;
            } else {
                cur->prev->next = cur->next;
                cur->next->prev = cur->prev;
            }

            cur->Close();
        	cur->WaitFor();

        	delete cur;
        }
    }

    // Add servers for ports that not running
    for(uint8_t ui8i = 0; ui8i < 25; ui8i++) {
        if(clsSettingManager::mPtr->iPortNumbers[ui8i] == 0) {
            break;
        }

        bool bFound = false;

        ServerThread *next = ServersS;
        while(next != NULL) {
            ServerThread *cur = next;
            next = cur->next;

            if(cur->ui16Port == clsSettingManager::mPtr->iPortNumbers[ui8i]) {
                bFound = true;
                break;
            }
        }

        if(bFound == false) {
            if(clsSettingManager::mPtr->bBools[SETBOOL_BIND_ONLY_SINGLE_IP] == true || (bUseIPv6 == true && bIPv6DualStack == false)) {
                if(bUseIPv6 == true) {
                    CreateServerThread(AF_INET6, clsSettingManager::mPtr->iPortNumbers[ui8i], true);
                }
                CreateServerThread(AF_INET, clsSettingManager::mPtr->iPortNumbers[ui8i], true);
            } else {
                CreateServerThread(bUseIPv6 == true ? AF_INET6 : AF_INET, clsSettingManager::mPtr->iPortNumbers[ui8i], true);
            }
        }
    }
}
//---------------------------------------------------------------------------

void clsServerManager::ResumeAccepts() {
	if(bServerRunning == false) {
        return;
    }

    ServerThread *next = ServersS;
    while(next != NULL) {
        ServerThread *cur = next;
        next = cur->next;

        cur->ResumeSck();
    }
}
//---------------------------------------------------------------------------

void clsServerManager::SuspendAccepts(const uint32_t &iTime) {
	if(bServerRunning == false) {
        return;
    }

    if(iTime != 0) {
        clsUdpDebug::mPtr->Broadcast("[SYS] Suspending listening threads to " + string(iTime) + " seconds.");
    } else {
        clsUdpDebug::mPtr->Broadcast("[SYS] Suspending listening threads.");
    }

    ServerThread *next = ServersS;
    while(next != NULL) {
        ServerThread *cur = next;
        next = cur->next;

        cur->SuspendSck(iTime);
    }
}
//---------------------------------------------------------------------------

void clsServerManager::UpdateAutoRegState() {
    if(bServerRunning == false) {
        return;
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_AUTO_REG] == true) {
#ifdef _WIN32
        regtimer = SetTimer(NULL, 0, 901000, NULL);

        if(regtimer == 0) {
#else
        struct itimerspec regtmrspec;
        regtmrspec.it_interval.tv_sec = 901;
        regtmrspec.it_interval.tv_nsec = 0;
        regtmrspec.it_value.tv_sec = 901;
        regtmrspec.it_value.tv_nsec = 0;
    
        int iRet = timer_settime(regtimer, 0, &regtmrspec, NULL);
        if(iRet == -1) {
#endif
			AppendDebugLog("%s - [ERR] Cannot start regtimer in ServerUpdateAutoRegState\n", 0);
            exit(EXIT_FAILURE);
        }
    } else {
#ifdef _WIN32
        if(KillTimer(NULL, regtimer) == 0) {
#else
        struct itimerspec regtmrspec;
        regtmrspec.it_interval.tv_sec = 0;
        regtmrspec.it_interval.tv_nsec = 0;
        regtmrspec.it_value.tv_sec = 0;
        regtmrspec.it_value.tv_nsec = 0;
    
        int iRet = timer_settime(regtimer, 0, &regtmrspec, NULL);
        if(iRet == -1) {
#endif
    		AppendDebugLog("%s - [ERR] Cannot stop regtimer in ServerUpdateAutoRegState\n", 0);
        	exit(EXIT_FAILURE);
        }
    }
}
//---------------------------------------------------------------------------

void clsServerManager::CreateServerThread(const int &iAddrFamily, const uint16_t &ui16PortNumber, const bool &bResume/* = false*/) {
	ServerThread * pServer = new ServerThread(iAddrFamily, ui16PortNumber);
    if(pServer == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate pServer in ServerCreateServerThread\n", 0);
        exit(EXIT_FAILURE);
    }

	if(pServer->Listen() == true) {
		if(ServersE == NULL) {
            ServersS = pServer;
            ServersE = pServer;
        } else {
            pServer->prev = ServersE;
            ServersE->next = pServer;
            ServersE = pServer;
        }
    } else {
        delete pServer;
    }

    if(bResume == true) {
        pServer->Resume();
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
