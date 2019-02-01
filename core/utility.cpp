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
#include "utility.h"
//---------------------------------------------------------------------------
#include "hashBanManager.h"
#include "LanguageManager.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "UdpDebug.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
	#include "../gui.win/GuiUtil.h"
    #include "../gui.win/MainWindowPageUsersChat.h"
#endif
//---------------------------------------------------------------------------
#include <skein.h>
//---------------------------------------------------------------------------
static const int MAX_PAT_SIZE = 64;
static const int MAX_ALPHABET_SIZE = 255;
//---------------------------------------------------------------------------

#ifdef _WIN32
void Cout(const string &msg) {
	if(clsServerManager::hConsole != NULL) {
		WriteConsole(clsServerManager::hConsole, (msg+"\n").c_str(), (DWORD)msg.size()+1, 0, 0);
    }
}
#else
void Cout(const string &/*msg*/) {
	return;
}
#endif
//---------------------------------------------------------------------------

#ifdef _WIN32
	// Boyer-Moore string matching algo.
	// returns :
	// offset in bytes or -1 if no match
	int BMFind(char *text, int N, char *pat, int M) {
	   int i, j, DD[MAX_PAT_SIZE], D[MAX_ALPHABET_SIZE];
	
	   /* Predzpracovani */
	   preDD(pat, M, DD);
	   preD(pat, M, D);
	
	   /* Vyhledavani */
	   /* Searching */
	   i = 0;
	   while (i <= N-M) {
	      for(j = M-1; j >= 0  &&  pat[j] == text[j+i]; --j);
	      if (j < 0) {
	         return i;
	         //i += DD[0];
	      }
	      else
	         i += DD[j] > (D[text[j + i]] - M + 1 + j) ? DD[j] : (D[text[j + i]] - M + 1 + j);
	   }
	   return -1;
	}
//---------------------------------------------------------------------------
	
	void preD(char *pat, int M, int D[]) {
	   int i;
	   for(i = 0; i < MAX_ALPHABET_SIZE; ++i)
	      D[i] = M;
	   for(i = 0; i < M - 1; ++i)
	      D[pat[i]] = M - i - 1;
	}
//---------------------------------------------------------------------------
	
	/* Funkce ulozi do suff[i] delku nejdelsiho podretezce,
	 * ktery konci na pozici pat[i]
	 * a soucasne je priponou pat
	 */
	void suffixes(char *pat, int M, int *suff) {
	   int f = 0, g, i;
	
	   suff[M - 1] = M;
	   g = M - 1;
	   for(i = M - 2; i >= 0; --i) {
	      if (i > g && suff[i + M - 1 - f] < i - g)
	         suff[i] = suff[i + M - 1 - f];
	      else {
	         if (i < g)
	            g = i;
	         f = i;
	         while (g >= 0 && pat[g] == pat[g + M - 1 - f])
	            --g;
	         suff[i] = f - g;
	      }
	   }
	}
//---------------------------------------------------------------------------
	
	void preDD(char *pat, int M, int DD[]) {
	   int i, j, suff[MAX_PAT_SIZE];
	
	   suffixes(pat, M, suff);
	
	   for(i = 0; i < M; ++i)
	      DD[i] = M;
	   j = 0;
	   for(i = M - 1; i >= -1; --i)
	      if (suff[i] == i+1  ||  i == -1)
	         for( ; j < M-1-i; ++j)
	            if (DD[j] == M)
	               DD[j] = M-1-i;
	   for(i = 0; i <= M-2; ++i)
	      DD[M - 1 - suff[i]] = M-1-i;
	}
#endif
//---------------------------------------------------------------------------

char * Lock2Key(char * sLock) {
    static char cKey[128];
    cKey[0] = '\0';

    // $Lock EXTENDEDPROTOCOL33MTL6@h5AIad^P2UoPv?fZU]ivM6 Pk=PtokaX|

    sLock = sLock+6; // set begin after $Lock_
    sLock[46] = '\0'; // set end before Pk
    
    uint8_t v;
    
	// first make the crypting stuff
	for(uint8_t ui8i = 0; ui8i < 46; ui8i++) {
        if(ui8i == 0) {
            v = (uint8_t)(sLock[0] ^ sLock[45] ^ sLock[44] ^ 5);
        } else {
            v = sLock[ui8i] ^ sLock[ui8i-1];
        }
        
        // swap nibbles (0xF0 = 11110000, 0x0F = 00001111)

        v = (uint8_t)(((v << 4) & 0xF0) | ((v >> 4) & 0x0F));
        
    	switch(v) {
            case   0:
                strcat(cKey, "/%DCN000%/");
                break;
    		case   5:
                strcat(cKey, "/%DCN005%/");
                break;
    		case  36:
                strcat(cKey, "/%DCN036%/");
                break;
			case  96:
                strcat(cKey, "/%DCN096%/");
                break;
    		case 124:
                strcat(cKey, "/%DCN124%/");
                break;
    		case 126:
                strcat(cKey, "/%DCN126%/");
                break;
    		default:
                strncat(cKey, (char *)&v, 1);
                break;
    	}
    }
    return cKey;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	char * WSErrorStr(const uint32_t &iError) {
		static char errStrings[][64] = {
			{"0"},{"1"},{"2"},{"3"},
			{"WSAEINTR"},
			{"5"},{"6"},{"7"},{"8"},
			{"WSAEBADF"},
			{"10"},{"11"},{"12"},
			{"WSAEACCES"},
			{"WSAEFAULT"},
			{"15"},{"16"},{"17"},{"18"},{"19"},{"20"},{"21"},
			{"WSAEINVAL"},
			{"23"},
			{"WSAEMFILE"},
			{"25"},{"26"},{"27"},{"28"},{"29"},{"30"},{"31"},{"32"},{"33"},{"34"},
	        {"WSAEWOULDBLOCK"},
	        {"WSAEINPROGRESS"},   // This error is returned if any Windows Sockets API function is called while a blocking function is in progress.
	        {"WSAEALREADY"},
	        {"WSAENOTSOCK"},
	        {"WSAEDESTADDRREQ"},
	        {"WSAEMSGSIZE"},
	        {"WSAEPROTOTYPE"},
	        {"WSAENOPROTOOPT"},
	        {"WSAEPROTONOSUPPORT"},
	        {"WSAESOCKTNOSUPPORT"},
	        {"WSAEOPNOTSUPP"},
	        {"WSAEPFNOSUPPORT"},
	        {"WSAEAFNOSUPPORT"},
	        {"WSAEADDRINUSE"},
	        {"WSAEADDRNOTAVAIL"},
	        {"WSAENETDOWN"}, // This error may be reported at any time if the Windows Sockets implementation detects an underlying failure.
	        {"WSAENETUNREACH"},
	        {"WSAENETRESET"},
	        {"WSAECONNABORTED"},
	        {"WSAECONNRESET"},
	        {"WSAENOBUFS"},
	        {"WSAEISCONN"},
	        {"WSAENOTCONN"},
	        {"WSAESHUTDOWN"},
	        {"WSAETOOMANYREFS"},
	        {"WSAETIMEDOUT"},
	        {"WSAECONNREFUSED"},
	        {"WSAELOOP"},
	        {"WSAENAMETOOLONG"},
	        {"WSAEHOSTDOWN"},
	        {"WSAEHOSTUNREACH"}, // 10065
	        {"WSASYSNOTREADY"}, // 10091
	        {"WSAVERNOTSUPPORTED"}, // 10092
	        {"WSANOTINITIALISED"}, // 10093
	        {"WSAHOST_NOT_FOUND"}, // 11001
	        {"WSATRY_AGAIN"}, // 11002
	        {"WSANO_RECOVERY"}, // 11003
	        {"WSANO_DATA"} // 11004
		};
	
		switch(iError) {
			case 10091 :
		    	return errStrings[66]; // WSASYSNOTREADY
			case 10092 :
		    	return errStrings[67]; // WSAVERNOTSUPPORTED
			case 10093 :
		    	return errStrings[68]; // WSANOTINITIALISED
			case 11001 :
		    	return errStrings[69]; // WSAHOST
			case 11002 :
		    	return errStrings[70]; // WSATRY
			case 11003 :
		    	return errStrings[71]; // WSANO
			case 11004 :
		    	return errStrings[72]; // WSANO
		    default :
		    	return errStrings[iError-10000];
	    }
	}
#else
	const char * ErrnoStr(const uint32_t &iError) {
		static const char *errStrings[] = {
	        "UNDEFINED", 
	        "EADDRINUSE", 
	        "ECONNRESET", 
	        "ETIMEDOUT", 
	        "ECONNREFUSED", 
	        "EHOSTUNREACH", 
		};
	
		switch(iError) {
			case 98:
		    	return errStrings[1];
			case 104:
		    	return errStrings[2];
			case 110:
		    	return errStrings[3];
			case 111:
		    	return errStrings[4];
			case 113:
		    	return errStrings[5];
		    default :
		    	return errStrings[0];
	    }
	}
#endif
//---------------------------------------------------------------------------

char * formatBytes(int64_t iBytes) {
    static const char *unit[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB", " ", " ", " ", " ", " ", " ", " "};
    static char sBytes[128];

   	if(iBytes < 1024) {
        int iLen = sprintf(sBytes, "%d %s", (int32_t)(iBytes&0xffffffff), unit[0]);
        if(CheckSprintf(iLen, 128, "formatBytes1") == false) {
            sBytes[0] = '\0';
        }
   	} else {
       	long double ldBytes = (long double)iBytes;
        uint8_t iter = 0;
       	for(; ldBytes > 1024; iter++)
           	ldBytes /= 1024;
           	
   	    int iLen = sprintf(sBytes, "%0.2Lf %s", ldBytes, unit[iter]);
   	    if(CheckSprintf(iLen, 128, "formatBytes2") == false) {
            sBytes[0] = '\0';
        }
    }

    return sBytes;
}
//---------------------------------------------------------------------------

char * formatBytesPerSecond(int64_t iBytes) {
    static const char *secondunit[] = {"B/s", "kB/s", "MB/s", "GB/s", "TB/s", "PB/s", "EB/s", "ZB/s", "YB/s", " ", " ", " ", " ", " ", " ", " "};
    static char sBytes[128];

   	if(iBytes < 1024) {
        int iLen = sprintf(sBytes, "%d %s", (int32_t)(iBytes&0xffffffff), secondunit[0]);
        if(CheckSprintf(iLen, 128, "formatBytesPerSecond1") == false) {
            sBytes[0] = '\0';
        }
   	} else {
       	long double ldBytes = (long double)iBytes;
        uint8_t iter = 0;
       	for(; ldBytes > 1024; iter++)
           	ldBytes /= 1024;
           	
   	    int iLen = sprintf(sBytes, "%0.2Lf %s", ldBytes, secondunit[iter]);
   	    if(CheckSprintf(iLen, 128, "formatBytesPerSecond2") == false) {
            sBytes[0] = '\0';
        }
    }
	
    return sBytes;
}
//---------------------------------------------------------------------------

char * formatTime(uint64_t rest) {
    static char time[256];
    time[0] = '\0';

	char buf[128];
	uint8_t i = 0;
    uint64_t n = rest / 525600;
	rest -= n*525600;

	if(n != 0) {
		int iLen = sprintf(buf, "%" PRIu64 " %s", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_YEARS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_YEAR_LWR]);
		if(CheckSprintf(iLen, 128, "formatTime1") == false) {
            time[0] = '\0';
            return time;
        }
		strcat(time, buf);
		i++;
	}

    n = rest / 43200;
	rest -= n*43200;

	if(n != 0) {       
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_MONTHS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_MONTH_LWR]);
		if(CheckSprintf(iLen, 128, "formatTime3") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
		i++;
	}

	n = rest / 1440;
	rest -= n*1440;

	if(n != 0) {       
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_DAYS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_DAY_LWR]);
		if(CheckSprintf(iLen, 128, "formatTime5") == false) {
            time[0] = '\0';
            return time;
        }
		
		strcat(time, buf);
		i++;
	}

    n = rest / 60;
	rest -= n*60;

	if(n != 0) {	
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_HOURS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_HOUR_LWR]);
		if(CheckSprintf(iLen, 128, "formatTime7") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
		i++;
	}

	if(rest != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", rest, clsLanguageManager::mPtr->sTexts[LAN_MIN_LWR]);
		if(CheckSprintf(iLen, 128, "formatTime9") == false) {
            time[0] = '\0';
            return time;
        }
		strcat(time, buf);
	}

	return time;
}
//---------------------------------------------------------------------------

char * formatSecTime(uint64_t rest) {
    static char time[256];
    time[0] = '\0';

	char buf[128];
	uint8_t i = 0;
    uint64_t n = rest / 31536000;
	rest -= n*31536000;

	if(n != 0) {
		int iLen = sprintf(buf, "%" PRIu64 " %s", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_YEARS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_YEAR_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime1") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
		i++;
	}

    n = rest / 2592000;
	rest -= n*2592000;

	if(n != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_MONTHS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_MONTH_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime3") == false) {
            time[0] = '\0';
            return time;
        }
		
		strcat(time, buf);
		i++;
	}

	n = rest / 86400;
	rest -= n*86400;

	if(n != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_DAYS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_DAY_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime5") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
		i++;
	}

    n = rest / 3600;
	rest -= n*3600;

	if(n != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, n > 1 ? clsLanguageManager::mPtr->sTexts[LAN_HOURS_LWR] : clsLanguageManager::mPtr->sTexts[LAN_HOUR_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime7") == false) {
            time[0] = '\0';
            return time;
        }
		
		strcat(time, buf);
		i++;
	}

	n = rest / 60;
	rest -= n*60;

	if(n != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", n, clsLanguageManager::mPtr->sTexts[LAN_MIN_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime9") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
		i++;
	}

	if(rest != 0) {
		int iLen = sprintf(buf, "%s%" PRIu64 " %s", i > 0 ? " " : "", rest, clsLanguageManager::mPtr->sTexts[LAN_SEC_LWR]);
		if(CheckSprintf(iLen, 128, "formatSecTime10") == false) {
            time[0] = '\0';
            return time;
        }

		strcat(time, buf);
	}
	return time;
}
//---------------------------------------------------------------------------

char* stristr(const char *str1, const char *str2) {
	char *s1, *s2;
	char *cp = (char *)str1;
	if(*str2 == 0)
		return (char *)str1;
	while(*cp != 0) {
		s1 = cp;
		s2 = (char *) str2;
		while(*s1 != 0 && *s2 != 0 && ((*s1-*s2) == 0 ||
			(*s1-tolower(*s2)) == 0 || (*s1-toupper(*s2)) == 0))
				s1++, s2++;
		if(*s2 == 0) {
			return cp;
		}
		cp++;
	}
	return NULL;
}
//---------------------------------------------------------------------------

char* stristr2(const char *str1, const char *str2) {
	char *s1, *s2;
	char *cp = (char *)str1;
	if(*str2 == 0)
		return (char *)str1;
	while(*cp != 0) {
		s1 = cp;
		s2 = (char *) str2;
		while(*s1 != 0 && *s2 != 0 && ((*s1-*s2) == 0 ||
			(tolower(*s1)-*s2) == 0))
				s1++, s2++;
		if(*s2 == 0) {
			return cp;
		}
		cp++;
	}
	return NULL;
}
//---------------------------------------------------------------------------

// check IP string.
// false - no ip
// true - valid ip
bool isIP(char * sIP) {
    if(clsServerManager::bUseIPv6 == true && strchr(sIP, '.') == NULL) {
        uint8_t ui128IpHash[16];
#ifdef _WIN32
        if(win_inet_pton(sIP, ui128IpHash) != 1) {
#else
        if(inet_pton(AF_INET6, sIP, ui128IpHash) != 1) {
#endif
            return false;
        }
    } else {
        uint32_t ui32IpHash = inet_addr(sIP);

        if(ui32IpHash == INADDR_NONE) {
            return false;
        }
    }

	return true;
}
//---------------------------------------------------------------------------

uint32_t HashNick(const char * sNick, const size_t &szNickLen) {
	unsigned char c;
    uint32_t h = 5381;

	for(size_t szi = 0; szi < szNickLen; szi++) {
        c = (unsigned char)tolower(sNick[szi]);
        h += (h << 5);
        h ^= c;
    }

	return (h+1);
}
//---------------------------------------------------------------------------

bool HashIP(const char * sIP, uint8_t * ui128IpHash) {
    if(clsServerManager::bUseIPv6 == true && strchr(sIP, '.') == NULL) {
#ifdef _WIN32
        if(win_inet_pton(sIP, ui128IpHash) != 1) {
#else
        if(inet_pton(AF_INET6, sIP, ui128IpHash) != 1) {
#endif
            return false;
        }
    } else {
        uint32_t ui32IpHash = inet_addr(sIP);

        if(ui32IpHash == INADDR_NONE) {
            return false;
        }

        memset(ui128IpHash, 0, 16);

        ui128IpHash[10] = 255;
        ui128IpHash[11] = 255;

        memcpy(ui128IpHash+12, &ui32IpHash, 4);
    }

	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

uint16_t GetIpTableIdx(const uint8_t * ui128IpHash) {
	unsigned char c;
    uint32_t h = 5381;

	for(uint8_t ui8i = 0; ui8i < 16; ui8i++) {
        c = (unsigned char)ui128IpHash[ui8i];
        h += (h << 5);
        h ^= c;
    }

    h += 1;

    uint16_t ui16Idx = 0;
    memcpy(&ui16Idx, &h, 2);

	return ui16Idx;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

char * GenerateBanMessage(BanItem * Ban, int32_t &iMsgLen, const time_t &acc_time) {
    static char banmsg[2048], banmsg1[512];

    if(((Ban->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == true) {
        iMsgLen = sprintf(banmsg, "<%s> %s.", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_PERM_BANNED]);
        if(CheckSprintf(iMsgLen, 2048, "GenerateBanMessage1") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
    } else {
        iMsgLen = sprintf(banmsg, "<%s> %s: %s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_TEMP_BANNED],
            formatSecTime(Ban->tempbanexpire-acc_time));
        if(CheckSprintf(iMsgLen, 2048, "GenerateBanMessage2") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_IP] == true && Ban->sIp[0] != '\0') {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_IP], Ban->sIp);
        if(CheckSprintf(iLen, 512, "GenerateBanMessage3") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_NICK] == true && Ban->sNick != NULL) {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_NICK], Ban->sNick);
        if(CheckSprintf(iLen, 512, "GenerateBanMessage4") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_REASON] == true && Ban->sReason != NULL) {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], Ban->sReason);
        if(CheckSprintf(iLen, 512, "GenerateBanMessage5") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_BY] == true && Ban->sBy != NULL) {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], Ban->sBy);
        if(CheckSprintf(iLen, 512, "GenerateBanMessage6") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->sTexts[SETTXT_MSG_TO_ADD_TO_BAN_MSG] != NULL) {
        int iLen = sprintf(banmsg1, "\n%s|", clsSettingManager::mPtr->sTexts[SETTXT_MSG_TO_ADD_TO_BAN_MSG]);
        if(CheckSprintf(iLen, 512, "GenerateBanMessage7") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    } else {
        banmsg[iMsgLen] = '|';
        iMsgLen++;
        banmsg[iMsgLen] = '\0';
    }

    if(((Ban->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_PERM_BAN_REDIR] == true) {
            iMsgLen += (int)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_PERM_BAN_REDIR_ADDRESS];
            strcat(banmsg, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_PERM_BAN_REDIR_ADDRESS]);
        }
    } else {
        if(clsSettingManager::mPtr->bBools[SETBOOL_TEMP_BAN_REDIR] == true && clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS] != NULL) {
            iMsgLen += (int)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS];
            strcat(banmsg, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS]);
        }
    }

    return banmsg;
}
//---------------------------------------------------------------------------

char * GenerateRangeBanMessage(RangeBanItem * RangeBan, int32_t &iMsgLen, const time_t &acc_time) {
    static char banmsg[2048], banmsg1[512];

    if(((RangeBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == true) {
        iMsgLen = sprintf(banmsg, "<%s> %s.", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_PERM_BANNED]);
        if(CheckSprintf(iMsgLen, 2048, "GenerateRangeBanMessage1") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
    } else {
        iMsgLen = sprintf(banmsg, "<%s> %s: %s", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_SORRY_TEMP_BANNED],
            formatSecTime(RangeBan->tempbanexpire-acc_time));
        if(CheckSprintf(iMsgLen, 2048, "GenerateRangeBanMessage2") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_RANGE] == true) {
        int iLen = sprintf(banmsg1, "\n%s: %s-%s", clsLanguageManager::mPtr->sTexts[LAN_RANGE], RangeBan->sIpFrom, RangeBan->sIpTo);
        if(CheckSprintf(iLen, 512, "GenerateRangeBanMessage3") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_REASON] == true && RangeBan->sReason != NULL) {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_REASON], RangeBan->sReason);
        if(CheckSprintf(iLen, 512, "GenerateRangeBanMessage4") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_BAN_MSG_SHOW_BY] == true && RangeBan->sBy != NULL) {
        int iLen = sprintf(banmsg1, "\n%s: %s", clsLanguageManager::mPtr->sTexts[LAN_BANNED_BY], RangeBan->sBy);
        if(CheckSprintf(iLen, 512, "GenerateRangeBanMessage5") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    }

    if(clsSettingManager::mPtr->sTexts[SETTXT_MSG_TO_ADD_TO_BAN_MSG] != NULL) {
        int iLen = sprintf(banmsg1, "\n%s|", clsSettingManager::mPtr->sTexts[SETTXT_MSG_TO_ADD_TO_BAN_MSG]);
        if(CheckSprintf(iLen, 512, "GenerateRangeBanMessage6") == false) {
            banmsg[0] = '\0';
            iMsgLen = 0;
            return banmsg;
        }
        iMsgLen += iLen;
        strcat(banmsg, banmsg1);
    } else {
        banmsg[iMsgLen] = '|';
        iMsgLen++;
        banmsg[iMsgLen] = '\0';
    }

    if(((RangeBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_PERM_BAN_REDIR] == true) {
            iMsgLen += (int)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_PERM_BAN_REDIR_ADDRESS];
            strcat(banmsg, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_PERM_BAN_REDIR_ADDRESS]);
        }
    } else {
        if(clsSettingManager::mPtr->bBools[SETBOOL_TEMP_BAN_REDIR] == true && clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS] != NULL) {
            iMsgLen += (int)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS];
            strcat(banmsg, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_TEMP_BAN_REDIR_ADDRESS]);
        }
    }
    return banmsg;
}
//---------------------------------------------------------------------------

bool GenerateTempBanTime(const char &cMultiplyer, const uint32_t &iTime, time_t &acc_time, time_t &ban_time) {
    time(&acc_time);
    struct tm *tm = localtime(&acc_time);

    switch(cMultiplyer) {
        case 'm':
            tm->tm_min += iTime;
            break;
        case 'h':
            tm->tm_hour += iTime;
            break;
        case 'd':
            tm->tm_mday += iTime;
            break;
        case 'w':
            tm->tm_mday += iTime*7;
            break;
        case 'M':
            tm->tm_mon += iTime;
            break;
        case 'Y':
            tm->tm_year += iTime;
            break;
        default:
            return false;
    }

    tm->tm_isdst = -1;

    ban_time = mktime(tm);

    if(ban_time == (time_t)-1) {
        return false;
    }

    return true;
}
//---------------------------------------------------------------------------

bool HaveOnlyNumbers(char *sData, const uint16_t &ui16Len) {
    for(uint16_t ui16i = 0; ui16i < ui16Len; ui16i++) {
        if(isdigit(sData[ui16i]) == 0)
            return false;
    }
    return true;
}
//---------------------------------------------------------------------------

int GetWlcmMsg(char * sWlcmMsg) {
	int iLen =  sprintf(sWlcmMsg, "%s%" PRIu64 " %s, %" PRIu64 " %s, %" PRIu64 " %s / %s: %u)|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_NAME_WLCM],
        clsServerManager::ui64Days, clsLanguageManager::mPtr->sTexts[LAN_DAYS_LWR], clsServerManager::ui64Hours, clsLanguageManager::mPtr->sTexts[LAN_HOURS_LWR], clsServerManager::ui64Mins, clsLanguageManager::mPtr->sTexts[LAN_MINUTES_LWR],
        clsLanguageManager::mPtr->sTexts[LAN_USERS], clsServerManager::ui32Logged);
    if(CheckSprintf(iLen, 1024, "GetWlcmMsg2") == false) {
        sWlcmMsg[0] = '\0';
        return 0;
    }

    return iLen;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	string GetMemStat() {
		string sStat = "";

	    PROCESS_MEMORY_COUNTERS pmc;
	    pmc.cb = sizeof(pmc);

		typedef BOOL (WINAPI *PGPMI)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
		PGPMI pGPMI = (PGPMI)GetProcAddress(LoadLibrary("psapi.dll"), "GetProcessMemoryInfo");

        if(pGPMI != NULL) {
			pGPMI(GetCurrentProcess(), &pmc, sizeof(pmc));
					   
            sStat += "\r\nMem usage (Peak): "+string(formatBytes(pmc.WorkingSetSize))+ " ("+string(formatBytes(pmc.PeakWorkingSetSize))+")";
            sStat += "\r\nVM size (Peak): "+string(formatBytes(pmc.PagefileUsage))+ " ("+string(formatBytes(pmc.PeakPagefileUsage))+")";
        }

		return sStat;
	}
#endif
//---------------------------------------------------------------------------

bool CheckSprintf(const int &iRetVal, const size_t &szMax, const char * sMsg) {
    if(iRetVal > 0) {
		if(szMax != 0 && iRetVal >= (int)szMax) {
			string sDbgstr = "%s - [ERR] sprintf high value "+string(iRetVal)+"/"+string((uint64_t)szMax)+" in "+string(sMsg)+"\n";
            AppendDebugLog(sDbgstr.c_str(), 0);
            return false;
        }
    } else {
		string sDbgstr = "%s - [ERR] sprintf low value "+string(iRetVal)+" in "+string(sMsg)+"\n";
		AppendDebugLog(sDbgstr.c_str(), 0);
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------

bool CheckSprintf1(const int &iRetVal, const size_t &szLenVal, const size_t &szMax, const char * sMsg) {
    if(iRetVal > 0) {
        if(szMax != 0 && szLenVal >= szMax) {
			string sDbgstr = "%s - [ERR] sprintf high value "+string((uint64_t)szLenVal)+"/"+string((uint64_t)szMax)+" in "+string(sMsg)+"\n";
			AppendDebugLog(sDbgstr.c_str(), 0);
            return false;
        }
    } else {
		string sDbgstr = "%s - [ERR] sprintf low value "+string(iRetVal)+" in "+string(sMsg)+"\n";
		AppendDebugLog(sDbgstr.c_str(), 0);
        return false;
    }
    return true;
}
//---------------------------------------------------------------------------

void AppendLog(const string & sData, const bool &bScript/* == false*/) {
	FILE * fw;

	if(bScript == false) {
#ifdef _WIN32
        fw = fopen((clsServerManager::sPath + "\\logs\\system.log").c_str(), "a");
#else
		fw = fopen((clsServerManager::sPath + "/logs/system.log").c_str(), "a");
#endif
    } else {
#ifdef _WIN32
        fw = fopen((clsServerManager::sPath + "\\logs\\script.log").c_str(), "a");
#else
		fw = fopen((clsServerManager::sPath + "/logs/script.log").c_str(), "a");
#endif
    }

	if(fw != NULL) {
	   time_t acc_time;
	   time(&acc_time);

	   struct tm * acc_tm;
	   acc_tm = localtime(&acc_time);

	   char sBuf[64];
	   strftime(sBuf, 64, "%c", acc_tm);

	   fprintf(fw, "%s - %s\n", sBuf, sData.c_str());

	   fclose(fw);
	}

    if(clsUdpDebug::mPtr != NULL && bScript == false) {
        if(sData.size() < 1000) {
            static char msg[1024];
            int imsgLen = sprintf(msg, "[LOG] %s", sData.c_str());
            if(CheckSprintf(imsgLen, 1024, "AppendLog1") == true) {
                clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
            }
        } else {
#ifdef _WIN32
            char * sMSG = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, sData.size()+64);
#else
			char * sMSG = (char *)malloc(sData.size()+64);
#endif
            if(sMSG == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sMSG in AppendLog\n", (uint64_t)(sData.size()+64));
                return;
            }

            int imsgLen = sprintf(sMSG, "[LOG] %s", sData.c_str());
            if(CheckSprintf(imsgLen, sData.size()+64, "AppendLog2") == true) {
                clsUdpDebug::mPtr->Broadcast(sMSG, imsgLen);
            }

#ifdef _WIN32
            if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sMSG) == 0) {
    			AppendDebugLog("%s - [MEM] Cannot deallocate MSG in AppendLog\n", 0);
            }
#else
			free(sMSG);
#endif
        }
    }
}
//---------------------------------------------------------------------------

void AppendDebugLog(const char * sData, const uint64_t ui64Value) {
#ifdef _WIN32
	FILE * fw = fopen((clsServerManager::sPath + "\\logs\\debug.log").c_str(), "a");
#else
	FILE * fw = fopen((clsServerManager::sPath + "/logs/debug.log").c_str(), "a");
#endif

	if(fw == NULL) {
		return;
	}

	time_t acc_time;
	time(&acc_time);

	struct tm * acc_tm;
	acc_tm = localtime(&acc_time);

	char sBuf[64];
	strftime(sBuf, 64, "%c", acc_tm);

    fprintf(fw, sData, sBuf, ui64Value); // "%s - %" PRIu64 "\n"

	fclose(fw);
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	void GetHeapStats(void *hHeap, DWORD &dwCommitted, DWORD &dwUnCommitted) {
	    PROCESS_HEAP_ENTRY *lpEntry;
	
	    lpEntry = (PROCESS_HEAP_ENTRY *)calloc(1, sizeof(PROCESS_HEAP_ENTRY));
	    if(lpEntry == NULL) {
            return;
        }

	    lpEntry->lpData = NULL;
	
	    while(HeapWalk((HANDLE)hHeap, (PROCESS_HEAP_ENTRY *)lpEntry) != 0) {
	        if((lpEntry->wFlags & PROCESS_HEAP_REGION)) {
	            dwCommitted += lpEntry->Region.dwCommittedSize;
	            dwUnCommitted += lpEntry->Region.dwUnCommittedSize;
	        }
	    }
	
	    free(lpEntry);
	}
	//---------------------------------------------------------------------------

	char * ExtractFileName(char * sPath) {
		char * sName = strrchr(sPath, '\\');
	
		if(sName != NULL) {
			return sName;
		} else {
	        return sPath;
	    }
	}
#endif
//---------------------------------------------------------------------------

#ifdef _BUILD_GUI
	void Memo(const string &sMessage) {
        RichEditAppendText(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::REDT_CHAT], sMessage.c_str());
    }
#else
	void Memo(const string &/*sMessage*/) {
	    // ...
	}
#endif
//---------------------------------------------------------------------------

bool FileExist(char * sPath) {
#ifdef _WIN32
	DWORD code = GetFileAttributes(sPath);
	if(code != INVALID_FILE_ATTRIBUTES && code != FILE_ATTRIBUTE_DIRECTORY) {
#else
    struct stat st;
	if(stat(sPath, &st) == 0 && S_ISDIR(st.st_mode) == 0) {
#endif
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------

bool DirExist(char * sPath) {
#ifdef _WIN32
	DWORD code = GetFileAttributes(sPath);
	if(code != INVALID_FILE_ATTRIBUTES && code == FILE_ATTRIBUTE_DIRECTORY) {
#else
    struct stat st;
	if(stat(sPath, &st) == 0 && S_ISDIR(st.st_mode)) {
#endif
		return true;
	}

	return false;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
	void SetupOsVersion() {
		OSVERSIONINFOEX ver;
		memset(&ver, 0, sizeof(OSVERSIONINFOEX));
		ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

		if(::GetVersionEx((OSVERSIONINFO*)&ver) == 0) {
			clsServerManager::sOS = "Windows (unknown version)";

			return;
		}

		if(ver.dwPlatformId != VER_PLATFORM_WIN32_NT) {
			clsServerManager::sOS = "Windows 9x/ME";
	    } else if(ver.dwMajorVersion == 6) {
            if(ver.dwMinorVersion == 3) {
                clsServerManager::sOS = "Windows 8.1";
            } else if(ver.dwMinorVersion == 2) {
                clsServerManager::sOS = "Windows 8";
            } else if(ver.dwMinorVersion == 1) {
	           if(ver.wProductType == VER_NT_WORKSTATION) {
	               clsServerManager::sOS = "Windows 7";
	           } else {
	               clsServerManager::sOS = "Windows 2008 R2";
	           }
            } else {
	           if(ver.wProductType == VER_NT_WORKSTATION) {
	               clsServerManager::sOS = "Windows Vista";
	           } else {
	               clsServerManager::sOS = "Windows 2008";
	           }
            }
		} else if(ver.dwMajorVersion == 5) {
	        if(ver.dwMinorVersion == 2) {
                if(ver.wProductType != VER_NT_WORKSTATION) {
				    clsServerManager::sOS = "Windows 2003";
				    return;
                } else if(ver.wProductType == VER_NT_WORKSTATION) {
                    SYSTEM_INFO si;
                    memset(&si, 0, sizeof(SYSTEM_INFO));

                    typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
                    PGNSI pGNSI = (PGNSI)::GetProcAddress(::GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo");
                    if(pGNSI != NULL) {
                        pGNSI(&si);
                    } else {
                        GetSystemInfo(&si);
                    }

                    if(si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64) {
                        clsServerManager::sOS = "Windows XP x64";
                        return;
                    }
                }

                // should not happen, but for sure...
                clsServerManager::sOS = "Windows 2003/XP64";
			} else if(ver.dwMinorVersion == 1) {
				clsServerManager::sOS = "Windows XP";
			} else if(ver.dwMinorVersion == 0) {
				clsServerManager::sOS = "Windows 2000";
			}
		} else if(ver.dwMajorVersion == 4) {
			clsServerManager::sOS = "Windows NT4";
		}
	}
//---------------------------------------------------------------------------

    void * LuaAlocator(void * /*pOld*/, void * pData, size_t /*szOldSize*/, size_t szNewSize) {
        if(szNewSize == 0) {
            if(pData != NULL) {
                ::HeapFree(clsServerManager::hLuaHeap, 0, pData);
            }

            return NULL;
        } else {
            if(pData != NULL) {
                return ::HeapReAlloc(clsServerManager::hLuaHeap, 0, pData, szNewSize);
            } else  {
                return ::HeapAlloc(clsServerManager::hLuaHeap, 0, szNewSize);
            }
        }
    }
#endif
//---------------------------------------------------------------------------

#ifdef _WIN32
typedef INT (WSAAPI * pInetPton)(INT, PCTSTR, PVOID);
pInetPton MyInetPton = NULL;
typedef PCTSTR (WSAAPI *pInetNtop)(INT, PVOID, PTSTR, size_t);
pInetNtop MyInetNtop = NULL;
#endif
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void CheckForIPv4() {
#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(sock == INVALID_SOCKET) {
        int iError = WSAGetLastError();
        if(iError == WSAEAFNOSUPPORT) {
#else
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1) {
        if(errno == EAFNOSUPPORT) {
#endif
            clsServerManager::bUseIPv4 = false;
            return;
        }
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void CheckForIPv6() {
#ifdef _WIN32
    SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

    if(sock == INVALID_SOCKET) {
        int iError = WSAGetLastError();
        if(iError == WSAEAFNOSUPPORT) {
#else
    int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(sock == -1) {
        if(errno == EAFNOSUPPORT) {
#endif
            clsServerManager::bUseIPv6 = false;
            return;
        }
    }

#ifdef _WIN32
    DWORD dwIPv6 = 0;
    if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&dwIPv6, sizeof(dwIPv6)) != SOCKET_ERROR) {
#else
    int iIPv6 = 0;
    if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &iIPv6, sizeof(iIPv6)) != -1) {
#endif
        clsServerManager::bIPv6DualStack = true;
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

#ifdef _WIN32
    HINSTANCE hWs2_32 = ::LoadLibrary("Ws2_32.dll");

    MyInetPton = (pInetPton)::GetProcAddress(hWs2_32, "inet_pton");
    MyInetNtop = (pInetNtop)::GetProcAddress(hWs2_32, "inet_ntop");

    ::FreeLibrary(hWs2_32);
#endif
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#ifdef _WIN32
INT win_inet_pton(PCTSTR pAddrString, PVOID pAddrBuf) {
    if(MyInetPton != NULL) {
        return MyInetPton(AF_INET6, pAddrString, pAddrBuf);
    } else {
        sockaddr_storage sas_addr;
        socklen_t sas_len = sizeof(sockaddr_storage);
        memset(&sas_addr, 0, sas_len);

        if(::WSAStringToAddressA((LPSTR)pAddrString, AF_INET6, NULL, (struct sockaddr *)&sas_addr, &sas_len) == 0) {
            memcpy(pAddrBuf, &((struct sockaddr_in6 *)&sas_addr)->sin6_addr.s6_addr, 16);
            return 1;
        } else {
            return 0;
        }
    }
}
//---------------------------------------------------------------------------

void win_inet_ntop(PVOID pAddr, PTSTR pStringBuf, size_t szStringBufSize) {
    if(MyInetNtop != NULL) {
        MyInetNtop(AF_INET6, pAddr, pStringBuf, szStringBufSize);
    } else {
        struct sockaddr_in6 sin6;
        socklen_t sin6_len = sizeof(sockaddr_in6);

        memset(&sin6, 0, sin6_len);
        sin6.sin6_family = AF_INET6;
        memcpy(&sin6.sin6_addr, pAddr, 16);

        ::WSAAddressToStringA((struct sockaddr *)&sin6, sin6_len, NULL, pStringBuf, (LPDWORD)&szStringBufSize);
    }
}
//---------------------------------------------------------------------------
#endif

bool GetMacAddress(const char * sIP, char * sMac) {
#ifdef _WIN32
    uint32_t uiIP = ::inet_addr(sIP);

    MIB_IPNETTABLE * pINT = (MIB_IPNETTABLE *)new char[131072];
    if(pINT == NULL) {
        return false;
    }

    ULONG ulSize = 131072;
    DWORD dwRes = ::GetIpNetTable(pINT, &ulSize, TRUE);
    if(dwRes == NO_ERROR) {
        for(DWORD dwi = 0; dwi < pINT->dwNumEntries; dwi++) {
            if(pINT->table[dwi].dwAddr == uiIP && pINT->table[dwi].dwType != MIB_IPNET_TYPE_INVALID) {
                sprintf(sMac, "%02x:%02x:%02x:%02x:%02x:%02x", pINT->table[dwi].bPhysAddr[0], pINT->table[dwi].bPhysAddr[1], pINT->table[dwi].bPhysAddr[2],
                    pINT->table[dwi].bPhysAddr[3], pINT->table[dwi].bPhysAddr[4], pINT->table[dwi].bPhysAddr[5]);
                delete []pINT;
                return true;
            }
        }
    }
    delete []pINT;
#else
    FILE *fp = fopen("/proc/net/arp", "r");
    if(fp != NULL) {
        uint16_t ui16IpLen = (uint16_t)strlen(sIP);
        char buf[1024];
        while(fgets(buf, 1024, fp) != NULL) {
            if(strncmp(buf, sIP, ui16IpLen) == 0 && buf[ui16IpLen] == ' ') {
                bool bLastCharSpace = true;
                uint8_t ui8NonSpaces = 0;
                while(buf[ui16IpLen] != '\0') {
                    if(buf[ui16IpLen] == ' ') {
                        bLastCharSpace = true;
                    } else {
                        if(bLastCharSpace == true) {
                            bLastCharSpace = false;
                            ui8NonSpaces++;
                        }
                    }

                    if(ui8NonSpaces == 3) {
                        strncpy(sMac, buf + ui16IpLen, 17);
                        sMac[17] = '\0';
                        fclose(fp);
                        return true;
                    }

                    ui16IpLen++;
                }
            }
        }

        fclose(fp);
    }
#endif

    return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void CreateGlobalBuffer() {
    clsServerManager::szGlobalBufferSize = 131072;
#ifdef _WIN32
    clsServerManager::sGlobalBuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, clsServerManager::szGlobalBufferSize);
#else
    clsServerManager::sGlobalBuffer = (char *)calloc(clsServerManager::szGlobalBufferSize, 1);
#endif
    if(clsServerManager::sGlobalBuffer == NULL) {
		AppendDebugLog("%s - [MEM] Cannot create clsServerManager::sGlobalBuffer\n", 0);
		exit(EXIT_FAILURE);
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void DeleteGlobalBuffer() {
#ifdef _WIN32
    if(clsServerManager::sGlobalBuffer != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)clsServerManager::sGlobalBuffer) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsServerManager::sGlobalBuffer\n", 0);
        }
    }
#else
	free(clsServerManager::sGlobalBuffer);
#endif
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool CheckAndResizeGlobalBuffer(const size_t &szWantedSize) {
    if(clsServerManager::szGlobalBufferSize >= szWantedSize) {
        return true;
    }

    size_t szOldSize = clsServerManager::szGlobalBufferSize;
    char * sOldBuf = clsServerManager::sGlobalBuffer;

    clsServerManager::szGlobalBufferSize = Allign128K(szWantedSize);

#ifdef _WIN32
    clsServerManager::sGlobalBuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldBuf, clsServerManager::szGlobalBufferSize);
#else
    clsServerManager::sGlobalBuffer = (char *)realloc(sOldBuf, clsServerManager::szGlobalBufferSize);
#endif
    if(clsServerManager::sGlobalBuffer == NULL) {
        clsServerManager::sGlobalBuffer = sOldBuf;

		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in CheckAndResizeGlobalBuffer for clsServerManager::sGlobalBuffer\n", (uint64_t)clsServerManager::szGlobalBufferSize);

        clsServerManager::szGlobalBufferSize = szOldSize;
        return false;
	}

    return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void ReduceGlobalBuffer() {
    if(clsServerManager::szGlobalBufferSize == 131072) {
        return;
    }

    size_t szOldSize = clsServerManager::szGlobalBufferSize;
    char * sOldBuf = clsServerManager::sGlobalBuffer;

    clsServerManager::szGlobalBufferSize = 131072;

#ifdef _WIN32
    clsServerManager::sGlobalBuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldBuf, clsServerManager::szGlobalBufferSize);
#else
    clsServerManager::sGlobalBuffer = (char *)realloc(sOldBuf, clsServerManager::szGlobalBufferSize);
#endif
    if(clsServerManager::sGlobalBuffer == NULL) {
        clsServerManager::sGlobalBuffer = sOldBuf;

		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in ReduceGlobalBuffer for clsServerManager::sGlobalBuffer\n", (uint64_t)clsServerManager::szGlobalBufferSize);

        clsServerManager::szGlobalBufferSize = szOldSize;
        return;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

bool HashPassword(char * sPassword, size_t &szPassLen, uint8_t * ui8PassHash) {
    Skein1024_Ctxt_t ctx1024;

    if(Skein1024_Init(&ctx1024, 512) == SKEIN_SUCCESS) {
        if(Skein1024_Update(&ctx1024, (u08b_t *)sPassword, szPassLen) == SKEIN_SUCCESS) {
            if(Skein1024_Final(&ctx1024, ui8PassHash) == SKEIN_SUCCESS) {
                return true;
            }
        }
    }

    return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
