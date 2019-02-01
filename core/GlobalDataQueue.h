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

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifndef GlobalDataQueueH
#define GlobalDataQueueH
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct User;
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class clsGlobalDataQueue {
private:
    struct QueueItem {
        size_t szLen1, szLen2;
        char * sCommand1, * sCommand2;
        QueueItem * pNext;
        uint8_t ui8CommandType;
    };

    struct GlobalQueue {
        size_t szLen, szSize, szZlen, szZsize;
        char * sBuffer, * sZbuffer;
        GlobalQueue * pNext;
        bool bCreated, bZlined;
    };

    struct OpsQueue {
    	size_t szLen, szSize;
    	char * sBuffer;
    };
    
    struct IPsQueue {
    	size_t szLen, szSize;
    	char * sBuffer;
    	bool bHaveDollars;
    };

    struct SingleDataItem {
        size_t szDataLen;
        uint8_t ui8Type;
        int32_t i32Profile;
        char * sData;
        SingleDataItem * pPrev, * pNext;
        User * pFromUser;
    };

    GlobalQueue * pCreatedGlobalQueues;

    QueueItem * pNewQueueItems[2];
    SingleDataItem * pNewSingleItems[2];

	struct OpsQueue OpListQueue;
	struct IPsQueue UserIPQueue;

    GlobalQueue GlobalQueues[144];

    char msg[128];

    static void AddDataToQueue(GlobalQueue &pQueue, char * sData, const size_t &szLen);
public:
    static clsGlobalDataQueue * mPtr;

    enum {
        CMD_HUBNAME,
        CMD_CHAT,
        CMD_HELLO,
        CMD_MYINFO,
        CMD_QUIT,
        CMD_OPS,
        CMD_LUA,
        CMD_ACTIVE_SEARCH_V6,
        CMD_ACTIVE_SEARCH_V64,
        CMD_ACTIVE_SEARCH_V4,
        CMD_PASSIVE_SEARCH_V6,
        CMD_PASSIVE_SEARCH_V64,
        CMD_PASSIVE_SEARCH_V4,
        CMD_PASSIVE_SEARCH_V4_ONLY,
        CMD_PASSIVE_SEARCH_V6_ONLY,
    };

    enum {
        BIT_LONG_MYINFO                     = 0x1,
        BIT_ALL_SEARCHES_IPV64              = 0x2,
        BIT_ALL_SEARCHES_IPV6               = 0x4,
        BIT_ALL_SEARCHES_IPV4               = 0x8,
        BIT_ACTIVE_SEARCHES_IPV64           = 0x10,
        BIT_ACTIVE_SEARCHES_IPV6            = 0x20,
        BIT_ACTIVE_SEARCHES_IPV4            = 0x40,
        BIT_HELLO                           = 0x80,
        BIT_OPERATOR                        = 0x100,
        BIT_USERIP                          = 0x200,
        BIT_ALL_SEARCHES_IPV6_ACTIVE_IPV4   = 0x400,
        BIT_ACTIVE_SEARCHES_IPV6_ALL_IPV4   = 0x800,
    };

    enum {
        SI_PM2ALL,
        SI_PM2OPS,
        SI_OPCHAT,
        SI_TOPROFILE,
        SI_PM2PROFILE,
    };

    QueueItem * pQueueItems;
    SingleDataItem * pSingleItems;

    clsGlobalDataQueue();
    ~clsGlobalDataQueue();

    void AddQueueItem(char * sCommand1, const size_t &szLen1, char * sCommand2, const size_t &szLen2, const uint8_t &ui8CmdType);
    void OpListStore(char * sNick);
    void UserIPStore(User * pUser);
    void PrepareQueueItems();
    void ClearQueues();
    void ProcessQueues(User * u);
    void ProcessSingleItems(User * u) const;
    void SingleItemStore(char * sData, const size_t &szDataLen, User * pFromUser, const int32_t &i32Profile, const uint8_t &ui8Type);
    void SendFinalQueue();
    void * GetLastQueueItem();
    void * GetFirstQueueItem();
    void * InsertBlankQueueItem(void * pAfterItem, const uint8_t &ui8CmdType);
    void FillBlankQueueItem(char * sCommand, const size_t &szLen, void * pQueueItem);
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif
