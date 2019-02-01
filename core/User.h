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
#ifndef UserH
#define UserH
//---------------------------------------------------------------------------

struct UserBan {
    char * sMessage;

    uint32_t ui32Len, ui32NickHash;

    UserBan();
    ~UserBan();

    static UserBan * CreateUserBan(char * sMess, const uint32_t &iMessLen, const uint32_t &ui32Hash);
};
//---------------------------------------------------------------------------

struct LoginLogout {
    LoginLogout();
    ~LoginLogout();

    uint64_t logonClk, ui64IPv4CheckTick;

    uint32_t iToCloseLoops, iUserConnectedLen;

    char * pBuffer;

    UserBan *uBan;
};
//---------------------------------------------------------------------------

struct PrcsdUsrCmd {
    enum PrcsdCmdsIds {
        CTM_MCTM_RCTM_SR_TO,
        SUPPORTS,
        LOGINHELLO,
        GETPASS,
        CHAT,
        TO_OP_CHAT
    };

    uint32_t iLen;
    char * sCommand;
    PrcsdUsrCmd *next;
    void * ptr;
    unsigned char cType;
};
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
struct User; // needed for next struct, and next struct must be defined before user :-/
//---------------------------------------------------------------------------

struct PrcsdToUsrCmd {
    uint32_t iLen, iPmCount, iLoops, iToNickLen;
    char * sCommand, * ToNick;
    PrcsdToUsrCmd *next;
    User *To;
};
//---------------------------------------------------------------------------
struct QzBuf; // for send queue
//---------------------------------------------------------------------------

struct User {
	User();
	~User();

	bool MakeLock();
	bool DoRecv();

	void SendChar(const char * cText, const size_t &szTextLen);
	void SendCharDelayed(const char * cText, const size_t &szTextLen);
	void SendTextDelayed(const string & sText);

    bool PutInSendBuf(const char * Text, const size_t &szTxtLen);
    bool Try2Send();

    void SetIP(char * sNewIP);
    void SetNick(char * sNewNick, const uint8_t &ui8NewNickLen);
    void SetMyInfoOriginal(char * sNewMyInfo, const uint16_t &ui16NewMyInfoLen);
    void SetVersion(char * sNewVer);
    void SetLastChat(char * sNewData, const size_t &szLen);
    void SetLastPM(char * sNewData, const size_t &szLen);
    void SetLastSearch(char * sNewData, const size_t &szLen);
    void SetBuffer(char * sKickMsg, size_t szLen = 0);
    void FreeBuffer();

    void Close(bool bNoQuit = false);

    void Add2Userlist();
    void AddUserList();

    bool GenerateMyInfoLong();
    bool GenerateMyInfoShort();

    static void FreeInfo(char * sInfo, const char * sName);

    void HasSuspiciousTag();

    bool ProcessRules();

    void AddPrcsdCmd(const unsigned char &cType, char * sCommand, const size_t &szCommandLen, User * to, const bool &bIsPm = false);

    void AddMeOrIPv4Check();

    static char * SetUserInfo(char * sOldData, uint8_t &ui8OldDataLen, char * sNewData, size_t &szNewDataLen, const char * sDataName);

    void RemFromSendBuf(const char * sData, const uint32_t &iLen, const uint32_t &iSbLen);

    static void DeletePrcsdUsrCmd(PrcsdUsrCmd * pCommand);

    enum UserStates {
        STATE_SOCKET_ACCEPTED,
        STATE_KEY_OR_SUP,
        STATE_VALIDATE,
        STATE_VERSION_OR_MYPASS,
        STATE_GETNICKLIST_OR_MYINFO,
        STATE_IPV4_CHECK,
        STATE_ADDME,
        STATE_ADDME_1LOOP,
        STATE_ADDME_2LOOP,
        STATE_ADDED,
        STATE_CLOSING,
        STATE_REMME
    };

    //  u->ui32BoolBits |= BIT_PRCSD_MYINFO;   <- set to 1
    //  u->ui32BoolBits &= ~BIT_PRCSD_MYINFO;  <- set to 0
    //  (u->ui32BoolBits & BIT_PRCSD_MYINFO) == BIT_PRCSD_MYINFO    <- test if is 1/true
    enum UserBits {
    	BIT_HASHED                     = 0x1,
    	BIT_ERROR                      = 0x2,
    	BIT_OPERATOR                   = 0x4,
    	BIT_GAGGED                     = 0x8,
    	BIT_GETNICKLIST                = 0x10,
    	BIT_IPV4_ACTIVE                = 0x20,
    	BIT_OLDHUBSTAG                 = 0x40,
    	BIT_TEMP_OPERATOR              = 0x80,
    	BIT_PINGER                     = 0x100,
    	BIT_BIG_SEND_BUFFER            = 0x200,
    	BIT_HAVE_SUPPORTS              = 0x400,
    	BIT_HAVE_BADTAG                = 0x800,
    	BIT_HAVE_GETNICKLIST           = 0x1000,
    	BIT_HAVE_BOTINFO               = 0x2000,
    	BIT_HAVE_KEY                   = 0x4000,
    	BIT_HAVE_SHARECOUNTED          = 0x8000,
    	BIT_PRCSD_MYINFO               = 0x10000,
    	BIT_RECV_FLOODER               = 0x20000,
    	BIT_QUACK_SUPPORTS             = 0x400000,
    	BIT_IPV6                       = 0x800000,
    	BIT_IPV4                       = 0x1000000,
    	BIT_WAITING_FOR_PASS           = 0x2000000,
    	BIT_WARNED_WRONG_IP            = 0x4000000,
    	BIT_IPV6_ACTIVE                = 0x8000000,
    	BIT_CHAT_INSERT                = 0x10000000,
    };

    enum UserInfoBits {
    	INFOBIT_DESCRIPTION_CHANGED        = 0x1,
    	INFOBIT_TAG_CHANGED                = 0x2,
    	INFOBIT_CONNECTION_CHANGED         = 0x4,
    	INFOBIT_EMAIL_CHANGED              = 0x8,
    	INFOBIT_SHARE_CHANGED              = 0x10,
    	INFOBIT_DESCRIPTION_SHORT_PERM     = 0x20,
    	INFOBIT_DESCRIPTION_LONG_PERM      = 0x40,
    	INFOBIT_TAG_SHORT_PERM             = 0x80,
    	INFOBIT_TAG_LONG_PERM              = 0x100,
    	INFOBIT_CONNECTION_SHORT_PERM      = 0x200,
    	INFOBIT_CONNECTION_LONG_PERM       = 0x400,
    	INFOBIT_EMAIL_SHORT_PERM           = 0x800,
    	INFOBIT_EMAIL_LONG_PERM            = 0x1000,
    	INFOBIT_SHARE_SHORT_PERM           = 0x2000,
    	INFOBIT_SHARE_LONG_PERM            = 0x4000
    };

    enum UserSupportBits {
    	SUPPORTBIT_NOGETINFO               = 0x1,
    	SUPPORTBIT_USERCOMMAND             = 0x2,
    	SUPPORTBIT_NOHELLO                 = 0x4,
    	SUPPORTBIT_QUICKLIST               = 0x8,
    	SUPPORTBIT_USERIP2                 = 0x10,
    	SUPPORTBIT_ZPIPE                   = 0x20,
    	SUPPORTBIT_IP64                    = 0x40,
    	SUPPORTBIT_IPV4                    = 0x80,
    };

    uint64_t ui64SharedSize, ui64ChangedSharedSizeShort, ui64ChangedSharedSizeLong;
	uint64_t ui64GetNickListsTick, ui64MyINFOsTick, ui64SearchsTick, ui64ChatMsgsTick;
    uint64_t ui64PMsTick, ui64SameSearchsTick, ui64SamePMsTick, ui64SameChatsTick;
    uint64_t iLastMyINFOSendTick, iLastNicklist, iReceivedPmTick, ui64ChatMsgsTick2;
    uint64_t ui64PMsTick2, ui64SearchsTick2, ui64MyINFOsTick2, ui64CTMsTick;
    uint64_t ui64CTMsTick2, ui64RCTMsTick, ui64RCTMsTick2, ui64SRsTick;
    uint64_t ui64SRsTick2, ui64RecvsTick, ui64RecvsTick2, ui64ChatIntMsgsTick;
    uint64_t ui64PMsIntTick, ui64SearchsIntTick;

    uint32_t ui32Recvs, ui32Recvs2;

    uint32_t Hubs, Slots, OLimit, LLimit, DLimit, iNormalHubs, iRegHubs, iOpHubs;
    uint32_t iSendCalled, iRecvCalled, iReceivedPmCount, iSR, iDefloodWarnings;
    uint32_t ui32BoolBits, ui32InfoBits, ui32SupportBits;

#ifdef _WIN32
	SOCKET Sck;
#else
	int Sck;
#endif

    time_t LoginTime;

    uint32_t sendbuflen, recvbuflen, sbdatalen, rbdatalen;

    uint32_t ui32NickHash;

    int32_t iProfile;

    char * sNick, *sVersion;
    char * sMyInfoOriginal, *sMyInfoShort, *sMyInfoLong;
    char * sDescription, *sTag, *sConnection, *sEmail;
    char * sClient, *sTagVersion;
    char * sLastChat, *sLastPM, *sLastSearch;
    char * sendbuf, *recvbuf, *sbplayhead;
    char * sChangedDescriptionShort, *sChangedDescriptionLong, *sChangedTagShort, *sChangedTagLong;
    char * sChangedConnectionShort, *sChangedConnectionLong, *sChangedEmailShort, *sChangedEmailLong;
    
    unsigned char MagicByte;
    
    LoginLogout *uLogInOut;

    PrcsdToUsrCmd *cmdToUserStrt, *cmdToUserEnd;

    PrcsdUsrCmd *cmdStrt, *cmdEnd, 
        * cmdActive4Search, * cmdActive6Search, * cmdPassiveSearch;

    User *prev, *next, *hashtableprev, *hashtablenext, *hashiptableprev, *hashiptablenext;

    uint16_t ui16MyInfoOriginalLen, ui16MyInfoShortLen, ui16MyInfoLongLen;
    uint16_t ui16GetNickLists, ui16MyINFOs, ui16Searchs, ui16ChatMsgs, ui16PMs;
    uint16_t ui16SameSearchs, ui16LastSearchLen, ui16SamePMs, ui16LastPMLen;
    uint16_t ui16SameChatMsgs, ui16LastChatLen, ui16LastPmLines, ui16SameMultiPms; 
    uint16_t ui16LastChatLines, ui16SameMultiChats, ui16ChatMsgs2, ui16PMs2;
    uint16_t ui16Searchs2, ui16MyINFOs2, ui16CTMs, ui16CTMs2;
    uint16_t ui16RCTMs, ui16RCTMs2, ui16SRs, ui16SRs2;
    uint16_t ui16ChatIntMsgs, ui16PMsInt, ui16SearchsInt;
    uint16_t ui16IpTableIdx;

    uint8_t ui8NickLen;
    uint8_t ui8IpLen, ui8ConnectionLen, ui8DescriptionLen, ui8EmailLen, ui8TagLen, ui8ClientLen, ui8TagVersionLen;
    uint8_t ui8Country, ui8State, ui8IPv4Len;
    uint8_t ui8ChangedDescriptionShortLen, ui8ChangedDescriptionLongLen, ui8ChangedTagShortLen, ui8ChangedTagLongLen;
    uint8_t ui8ChangedConnectionShortLen, ui8ChangedConnectionLongLen, ui8ChangedEmailShortLen, ui8ChangedEmailLongLen;

    uint8_t ui128IpHash[16];

    char sIP[46], sIPv4[16];

    char sModes[3];
};
//---------------------------------------------------------------------------

#endif
