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
#ifndef utilityH
#define utilityH
//---------------------------------------------------------------------------
struct BanItem;
struct RangeBanItem;
//---------------------------------------------------------------------------

void Cout(const string & msg);
//---------------------------------------------------------------------------

#ifdef _WIN32
	static void preD(char *pat, int M, int D[]);
	static void suffixes(char *pat, int M, int *suff);
	static void preDD(char *pat, int M, int DD[]);
		
	int BMFind(char *text, int N, char *pat, int M);
#endif

char * Lock2Key(char * cLock);

#ifdef _WIN32
	char * WSErrorStr(const uint32_t &iError);
#else
	const char * ErrnoStr(const uint32_t &iError);
#endif

char * formatBytes(int64_t iBytes);
char * formatBytesPerSecond(int64_t iBytes);
char * formatTime(uint64_t rest);
char * formatSecTime(uint64_t rest);

char * stristr(const char *str1, const char *str2);
char * stristr2(const char *str1, const char *str2);

bool isIP(char * sIP);

uint32_t HashNick(const char * sNick, const size_t &szNickLen);

bool HashIP(const char * sIP, uint8_t * ui128IpHash);
uint16_t GetIpTableIdx(const uint8_t * ui128IpHash);

char * GenerateBanMessage(BanItem * Ban, int32_t &iMsgLen, const time_t &acc_time);
char * GenerateRangeBanMessage(RangeBanItem * RangeBan, int32_t &iMsgLen, const time_t &acc_time);

bool GenerateTempBanTime(const char &cMultiplyer, const uint32_t &iTime, time_t &acc_time, time_t &ban_time);

bool HaveOnlyNumbers(char *sData, const uint16_t &ui16Len);
int GetWlcmMsg(char * sWlcmMsg);

inline size_t Allign256(size_t n) { return ((n+1) & 0xFFFFFF00) + 0x100; }
inline size_t Allign512(size_t n) { return ((n+1) & 0xFFFFFE00) + 0x200; }
inline size_t Allign1024(size_t n) { return ((n+1) & 0xFFFFFC00) + 0x400; }
inline size_t Allign16K(size_t n) { return ((n+1) & 0xFFFFC000) + 0x4000; }
inline size_t Allign128K(size_t n) { return ((n+1) & 0xFFFE0000) + 0x20000; }

#ifdef _WIN32
	string GetMemStat();
#endif

bool CheckSprintf(const int &iRetVal, const size_t &szMax, const char * sMsg); // CheckSprintf(imsgLen, 64, "UdpDebug::New");
bool CheckSprintf1(const int &iRetVal, const size_t &szLenVal, const size_t &szMax, const char * sMsg); // CheckSprintf1(iret, imsgLen, 64, "UdpDebug::New");

void AppendLog(const string & sData, const bool &bScript = false);
void AppendDebugLog(const char * sData, const uint64_t ui64Value);

#ifdef _WIN32
	void GetHeapStats(void *hHeap, DWORD &dwCommitted, DWORD &dwUnCommitted);
#endif

void Memo(const string & sMessage);

#ifdef _WIN32
	char * ExtractFileName(char * sPath);
#endif

bool FileExist(char * sPath);
bool DirExist(char * sPath);

#ifdef _WIN32
	void SetupOsVersion();
	void * LuaAlocator(void * pOld, void * pData, size_t szOldSize, size_t szNewSize);
    INT win_inet_pton(PCTSTR pAddrString, PVOID pAddrBuf);
    void win_inet_ntop(PVOID pAddr, PTSTR pStringBuf, size_t szStringBufSize);
#endif

void CheckForIPv4();
void CheckForIPv6();

bool GetMacAddress(const char * sIP, char * sMac);

void CreateGlobalBuffer();
void DeleteGlobalBuffer();
bool CheckAndResizeGlobalBuffer(const size_t &szWantedSize);
void ReduceGlobalBuffer();

bool HashPassword(char * sPassword, size_t &szPassLen, uint8_t * ui8PassHash);
//---------------------------------------------------------------------------

#endif
