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
#include "hashUsrManager.h"
//---------------------------------------------------------------------------
#include "User.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
clsHashManager * clsHashManager::mPtr = NULL;
//---------------------------------------------------------------------------

clsHashManager::clsHashManager() {
    for(uint32_t ui32i = 0; ui32i < 65536; ui32i++) {
        nicktable[ui32i] = NULL;
        iptable[ui32i] = NULL;
    }

    //Memo("clsHashManager created");
}
//---------------------------------------------------------------------------

clsHashManager::~clsHashManager() {
    //Memo("clsHashManager destroyed");
    for(uint32_t ui32i = 0; ui32i < 65536; ui32i++) {
		IpTableItem * next = iptable[ui32i];
        
        while(next != NULL) {
            IpTableItem * cur = next;
            next = cur->next;
        
            delete cur;
		}
    }
}
//---------------------------------------------------------------------------

bool clsHashManager::Add(User * u) {
    uint16_t ui16dx = 0;
    memcpy(&ui16dx, &u->ui32NickHash, sizeof(uint16_t));

    if(nicktable[ui16dx] != NULL) {
        nicktable[ui16dx]->hashtableprev = u;
        u->hashtablenext = nicktable[ui16dx];
    }

    nicktable[ui16dx] = u;

    if(iptable[u->ui16IpTableIdx] == NULL) {
        iptable[u->ui16IpTableIdx] = new IpTableItem();

        if(iptable[u->ui16IpTableIdx] == NULL) {
            u->ui32BoolBits |= User::BIT_ERROR;
            u->Close();

            AppendDebugLog("%s - [MEM] Cannot allocate IpTableItem in clsHashManager::Add\n", 0);
            return false;
        }

        iptable[u->ui16IpTableIdx]->next = NULL;
        iptable[u->ui16IpTableIdx]->prev = NULL;

        iptable[u->ui16IpTableIdx]->FirstUser = u;
		iptable[u->ui16IpTableIdx]->ui16Count = 1;

        return true;
    }

    IpTableItem * next = iptable[u->ui16IpTableIdx];

    while(next != NULL) {
        IpTableItem * cur = next;
        next = cur->next;

        if(memcmp(cur->FirstUser->ui128IpHash, u->ui128IpHash, 16) == 0) {
            cur->FirstUser->hashiptableprev = u;
            u->hashiptablenext = cur->FirstUser;
            cur->FirstUser = u;
			cur->ui16Count++;

            return true;
        }
    }

    IpTableItem * cur = new IpTableItem();

    if(cur == NULL) {
        u->ui32BoolBits |= User::BIT_ERROR;
        u->Close();

		AppendDebugLog("%s - [MEM] Cannot allocate IpTableItem2 in clsHashManager::Add\n", 0);
		return false;
    }

    cur->FirstUser = u;
	cur->ui16Count = 1;

    cur->next = iptable[u->ui16IpTableIdx];
    cur->prev = NULL;

    iptable[u->ui16IpTableIdx]->prev = cur;
    iptable[u->ui16IpTableIdx] = cur;

    return true;
}
//---------------------------------------------------------------------------

void clsHashManager::Remove(User * u) {
    if(u->hashtableprev == NULL) {
        uint16_t ui16dx = 0;
        memcpy(&ui16dx, &u->ui32NickHash, sizeof(uint16_t));

        if(u->hashtablenext == NULL) {
            nicktable[ui16dx] = NULL;
        } else {
            u->hashtablenext->hashtableprev = NULL;
            nicktable[ui16dx] = u->hashtablenext;
        }
    } else if(u->hashtablenext == NULL) {
        u->hashtableprev->hashtablenext = NULL;
    } else {
        u->hashtableprev->hashtablenext = u->hashtablenext;
        u->hashtablenext->hashtableprev = u->hashtableprev;
    }

    u->hashtableprev = NULL;
    u->hashtablenext = NULL;

	if(u->hashiptableprev == NULL) {
        IpTableItem * next = iptable[u->ui16IpTableIdx];
    
        while(next != NULL) {
            IpTableItem * cur = next;
            next = cur->next;

            if(memcmp(cur->FirstUser->ui128IpHash, u->ui128IpHash, 16) == 0) {
				cur->ui16Count--;

                if(u->hashiptablenext == NULL) {
                    if(cur->prev == NULL) {
                        if(cur->next == NULL) {
                            iptable[u->ui16IpTableIdx] = NULL;
                        } else {
                            cur->next->prev = NULL;
                            iptable[u->ui16IpTableIdx] = cur->next;
                        }
                    } else if(cur->next == NULL) {
                        cur->prev->next = NULL;
                    } else {
                        cur->prev->next = cur->next;
                        cur->next->prev = cur->prev;
                    }

                    delete cur;
                } else {
                    u->hashiptablenext->hashiptableprev = NULL;
                    cur->FirstUser = u->hashiptablenext;
                }

                u->hashiptableprev = NULL;
                u->hashiptablenext = NULL;

                return;
            }
        }
    } else if(u->hashiptablenext == NULL) {
        u->hashiptableprev->hashiptablenext = NULL;
    } else {
        u->hashiptableprev->hashiptablenext = u->hashiptablenext;
        u->hashiptablenext->hashiptableprev = u->hashiptableprev;
    }

    u->hashiptableprev = NULL;
    u->hashiptablenext = NULL;

    IpTableItem * next = iptable[u->ui16IpTableIdx];

    while(next != NULL) {
        IpTableItem * cur = next;
        next = cur->next;

        if(memcmp(cur->FirstUser->ui128IpHash, u->ui128IpHash, 16) == 0) {
			cur->ui16Count--;

            return;
        }
    }
}
//---------------------------------------------------------------------------

User * clsHashManager::FindUser(char * sNick, const size_t &szNickLen) {
    uint32_t ui32Hash = HashNick(sNick, szNickLen);

    uint16_t ui16dx = 0;
    memcpy(&ui16dx, &ui32Hash, sizeof(uint16_t));

    User *next = nicktable[ui16dx];

    // pointer exists ? Then we need look for nick 
    if(next != NULL) {
        while(next != NULL) {
            User *cur = next;
            next = cur->hashtablenext;

            // we are looking for duplicate string
			if(cur->ui32NickHash == ui32Hash && cur->ui8NickLen == szNickLen && strcasecmp(cur->sNick, sNick) == 0) {
                return cur;
            }
        }            
    }
    
    // no equal hash found, we dont have the nick in list
    return NULL;
}
//---------------------------------------------------------------------------

User * clsHashManager::FindUser(User * u) {
    uint16_t ui16dx = 0;
    memcpy(&ui16dx, &u->ui32NickHash, sizeof(uint16_t));

    User *next = nicktable[ui16dx];  

    // pointer exists ? Then we need look for nick
    if(next != NULL) { 
        while(next != NULL) {
            User *cur = next;
            next = cur->hashtablenext;

            // we are looking for duplicate string
			if(cur->ui32NickHash == u->ui32NickHash && cur->ui8NickLen == u->ui8NickLen && strcasecmp(cur->sNick, u->sNick) == 0) {
                return cur;
            }
        }            
    }
    
    // no equal hash found, we dont have the nick in list
    return NULL;
}
//---------------------------------------------------------------------------

User * clsHashManager::FindUser(const uint8_t * ui128IpHash) {
    uint16_t ui16IpTableIdx = 0;

    if(ui128IpHash[10] == 255 && ui128IpHash[11] == 255 && memcmp(ui128IpHash, "\0\0\0\0\0\0\0\0\0\0", 10) == 0) {
        ui16IpTableIdx = ui128IpHash[14] * ui128IpHash[15];
    } else {
        ui16IpTableIdx = GetIpTableIdx(ui128IpHash);
    }

	IpTableItem * next = iptable[ui16IpTableIdx];

    while(next != NULL) {
		IpTableItem * cur = next;
        next = cur->next;

        if(memcmp(cur->FirstUser->ui128IpHash, ui128IpHash, 16) == 0) {
            return cur->FirstUser;
        }
    }

    return NULL;
}
//---------------------------------------------------------------------------

uint32_t clsHashManager::GetUserIpCount(User * u) const {
	IpTableItem * next = iptable[u->ui16IpTableIdx];

	while(next != NULL) {
		IpTableItem * cur = next;
		next = cur->next;

        if(memcmp(cur->FirstUser->ui128IpHash, u->ui128IpHash, 16) == 0) {
            return cur->ui16Count;
        }
	}

	return 0;
}
//---------------------------------------------------------------------------
