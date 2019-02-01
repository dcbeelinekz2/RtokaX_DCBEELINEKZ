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
#include "LuaInc.h"
//---------------------------------------------------------------------------
#include "LuaBanManLib.h"
//---------------------------------------------------------------------------
#include "hashBanManager.h"
#include "hashUsrManager.h"
#include "LuaScriptManager.h"
#include "ServerManager.h"
#include "UdpDebug.h"
#include "User.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "LuaScript.h"
//---------------------------------------------------------------------------

static void PushBan(lua_State * L, BanItem * b) {
	lua_checkstack(L, 3); // we need 3 (1 table, 2 id, 3 value) empty slots in stack, check it to be sure

	lua_newtable(L);
	int i = lua_gettop(L);

    lua_pushliteral(L, "sIP");
    if(b->sIp[0] == '\0') {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, b->sIp);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "sNick");
    if(b->sNick == NULL) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, b->sNick);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "sReason");
    if(b->sReason == NULL) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, b->sReason);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "sBy");
    if(b->sBy == NULL) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, b->sBy);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "iExpireTime");
#if LUA_VERSION_NUM < 503
	((b->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false ? lua_pushnil(L) : lua_pushnumber(L, (double)b->tempbanexpire);
#else
    ((b->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false ? lua_pushnil(L) : lua_pushunsigned(L, b->tempbanexpire);
#endif
    lua_rawset(L, i);

    lua_pushliteral(L, "bIpBan");
    ((b->ui8Bits & clsBanManager::IP) == clsBanManager::IP) == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);

    lua_pushliteral(L, "bNickBan");
    ((b->ui8Bits & clsBanManager::NICK) == clsBanManager::NICK) == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);

    lua_pushliteral(L, "bFullIpBan");
    ((b->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);
}
//------------------------------------------------------------------------------

static void PushRangeBan(lua_State * L, RangeBanItem * rb) {
	lua_checkstack(L, 3); // we need 3 (1 table, 2 id, 3 value) empty slots in stack, check it to be sure

	lua_newtable(L);
	int i = lua_gettop(L);

    lua_pushliteral(L, "sIPFrom");
    lua_pushstring(L, rb->sIpFrom);
    lua_rawset(L, i);

    lua_pushliteral(L, "sIPTo");
    lua_pushstring(L, rb->sIpTo);
    lua_rawset(L, i);

    lua_pushliteral(L, "sReason");
    if(rb->sReason == NULL) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, rb->sReason);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "sBy");
    if(rb->sBy == NULL) {
		lua_pushnil(L);
	} else {
		lua_pushstring(L, rb->sBy);
	}
    lua_rawset(L, i);

    lua_pushliteral(L, "iExpireTime");
#if LUA_VERSION_NUM < 503
	((rb->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false ? lua_pushnil(L) : lua_pushnumber(L, (double)rb->tempbanexpire);
#else
    ((rb->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false ? lua_pushnil(L) : lua_pushunsigned(L, rb->tempbanexpire);
#endif
    lua_rawset(L, i);

    lua_pushliteral(L, "bFullIpBan");
    ((rb->ui8Bits & clsBanManager::FULL) == clsBanManager::FULL) == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);
}
//------------------------------------------------------------------------------

static int Save(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Save' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsBanManager::mPtr->Save(true);

    return 0;
}
//------------------------------------------------------------------------------

static int GetBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

    time_t acc_time;
    time(&acc_time);

    BanItem *nextBan = clsBanManager::mPtr->TempBanListS;

    while(nextBan != NULL) {
        BanItem *curBan = nextBan;
		nextBan = curBan->next;

        if(acc_time > curBan->tempbanexpire) {
			clsBanManager::mPtr->Rem(curBan);
            delete curBan;

			continue;
        }

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushBan(L, curBan);        
        lua_rawset(L, t);
    }

	nextBan = clsBanManager::mPtr->PermBanListS;

    while(nextBan != NULL) {
        BanItem *curBan = nextBan;
		nextBan = curBan->next;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushBan(L, curBan);        
        lua_rawset(L, t);
    }

    return 1;
}
//---------------------------------------------------------------------------

static int GetTempBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetTempBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

    time_t acc_time;
    time(&acc_time);

    BanItem *nextBan = clsBanManager::mPtr->TempBanListS;

    while(nextBan != NULL) {
        BanItem *curBan = nextBan;
		nextBan = curBan->next;

        if(acc_time > curBan->tempbanexpire) {
			clsBanManager::mPtr->Rem(curBan);
            delete curBan;

			continue;
        }

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushBan(L, curBan);        
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetPermBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetPermBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

	BanItem *nextBan = clsBanManager::mPtr->PermBanListS;

    while(nextBan != NULL) {
        BanItem *curBan = nextBan;
		nextBan = curBan->next;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushBan(L, curBan);        
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetBan(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetBan' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    time_t acc_time;
    time(&acc_time);

    size_t szLen;
    char * sValue = (char *)lua_tolstring(L, 1, &szLen);

	BanItem *Ban = clsBanManager::mPtr->FindNick(sValue, szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

	if(HashIP(sValue, ui128Hash) == true) {
        lua_settop(L, 0);
        
        lua_newtable(L);
        int t = lua_gettop(L), i = 0;

        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban); 
            lua_rawset(L, t);
        }

		Ban = clsBanManager::mPtr->FindIP(ui128Hash, acc_time);
        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban);        
            lua_rawset(L, t);

            BanItem *nextBan = Ban->hashiptablenext;
        
            while(nextBan != NULL) {
                BanItem *curBan = nextBan;
				nextBan = curBan->hashiptablenext;

				if((((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) && acc_time > curBan->tempbanexpire) {
					clsBanManager::mPtr->Rem(curBan);
                    delete curBan;

        			continue;
                }

#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, ++i);
#else
                lua_pushunsigned(L, ++i);
#endif
                PushBan(L, curBan);        
                lua_rawset(L, t);
            }
        }
        return 1;
    } else {
        lua_settop(L, 0);

        if(Ban == NULL) {
        	lua_pushnil(L);
            return 1;
        }

        PushBan(L, Ban); 
        return 1;
    }
}
//------------------------------------------------------------------------------

static int GetPermBan(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetPermBan' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szLen;
    char * sValue = (char *)lua_tolstring(L, 1, &szLen);

	BanItem *Ban = clsBanManager::mPtr->FindPermNick(sValue, szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

    if(HashIP(sValue, ui128Hash) == true) {
        lua_settop(L, 0);
        
        lua_newtable(L);
        int t = lua_gettop(L), i = 0;

        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban); 
            lua_rawset(L, t);
        }

		Ban = clsBanManager::mPtr->FindPermIP(ui128Hash);
        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban);        
            lua_rawset(L, t);

            BanItem *nextBan = Ban->hashiptablenext;
        
            while(nextBan != NULL) {
                BanItem *curBan = nextBan;
        		nextBan = curBan->hashiptablenext;

				if(((curBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == false) {
        			continue;
                }

#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, ++i);
#else
                lua_pushunsigned(L, ++i);
#endif
                PushBan(L, curBan);        
                lua_rawset(L, t);
            }
        }
        return 1;
    } else {
        lua_settop(L, 0);

        if(Ban == NULL) {
        	lua_pushnil(L);
            return 1;
        }

        PushBan(L, Ban); 
        return 1;
    }
}
//------------------------------------------------------------------------------

static int GetTempBan(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetTempBan' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    time_t acc_time;
    time(&acc_time);

    size_t szLen;
    char * sValue = (char *)lua_tolstring(L, 1, &szLen);

	BanItem * Ban = clsBanManager::mPtr->FindTempNick(sValue, szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

    if(HashIP(sValue, ui128Hash) == true) {
        lua_settop(L, 0);
        
        lua_newtable(L);
        int t = lua_gettop(L), i = 0;

        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban); 
            lua_rawset(L, t);
        }

		Ban = clsBanManager::mPtr->FindTempIP(ui128Hash, acc_time);
        if(Ban != NULL) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushBan(L, Ban);        
            lua_rawset(L, t);

            BanItem *nextBan = Ban->hashiptablenext;
        
            while(nextBan != NULL) {
                BanItem *curBan = nextBan;
        		nextBan = curBan->hashiptablenext;

				if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                    if(acc_time > curBan->tempbanexpire) {
						clsBanManager::mPtr->Rem(curBan);
                        delete curBan;
    
            			continue;
                    }

#if LUA_VERSION_NUM < 503
					lua_pushnumber(L, ++i);
#else
                    lua_pushunsigned(L, ++i);
#endif
                    PushBan(L, curBan);        
                    lua_rawset(L, t);
                }
            }
        }
        return 1;
    } else {
        lua_settop(L, 0);

        if(Ban == NULL) {
        	lua_pushnil(L);
            return 1;
        }

        PushBan(L, Ban); 
        return 1;
    }
}
//------------------------------------------------------------------------------

static int GetRangeBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetRangeBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
	int t = lua_gettop(L), i = 0;

    time_t acc_time;
    time(&acc_time);

    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

    while(nextBan != NULL) {
        RangeBanItem *curBan = nextBan;
		nextBan = curBan->next;
        
        if((((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) && acc_time > curBan->tempbanexpire) {
			clsBanManager::mPtr->RemRange(curBan);
            delete curBan;

			continue;
        }

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushRangeBan(L, curBan);
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetTempRangeBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetTempRangeBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
	int t = lua_gettop(L), i = 0;

    time_t acc_time;
    time(&acc_time);

    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

    while(nextBan != NULL) {
        RangeBanItem *curBan = nextBan;
		nextBan = curBan->next;

        if(((curBan->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == false) {
            continue;
        }
        
        if(acc_time > curBan->tempbanexpire) {
			clsBanManager::mPtr->RemRange(curBan);
            delete curBan;

			continue;
        }

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushRangeBan(L, curBan);
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetPermRangeBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetPermRangeBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
	int t = lua_gettop(L), i = 0;

    RangeBanItem *nextBan = clsBanManager::mPtr->RangeBanListS;

    while(nextBan != NULL) {
        RangeBanItem *curBan = nextBan;
		nextBan = curBan->next;

        if(((curBan->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == false) {
            continue;
        }

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif
        PushRangeBan(L, curBan);
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetRangeBan(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetRangeBan' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromLen, szToLen;
    char * sFrom = (char *)lua_tolstring(L, 1, &szFromLen);
    char * sTo = (char *)lua_tolstring(L, 2, &szToLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

    if(szFromLen == 0 || szToLen == 0 || HashIP(sFrom, ui128FromHash) == false || HashIP(sTo, ui128ToHash) == false || memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 0);

    time_t acc_time;
    time(&acc_time);

	RangeBanItem *next = clsBanManager::mPtr->RangeBanListS;

    while(next != NULL) {
        RangeBanItem *cur = next;
        next = cur->next;

        if(memcmp(cur->ui128FromIpHash, ui128FromHash, 16) == 0 && memcmp(cur->ui128ToIpHash, ui128ToHash, 16) == 0) {
            // PPK ... check if it's temban and then if it's expired
            if(((cur->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                if(acc_time >= cur->tempbanexpire) {
					clsBanManager::mPtr->RemRange(cur);
                    delete cur;

					continue;
                }
            }
            PushRangeBan(L, cur);
            return 1;
        }
    }

	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int GetRangePermBan(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetRangePermBan' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromLen, szToLen;
    char * sFrom = (char *)lua_tolstring(L, 1, &szFromLen);
    char * sTo = (char *)lua_tolstring(L, 2, &szToLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromLen == 0 || szToLen == 0 || HashIP(sFrom, ui128FromHash) == false || HashIP(sTo, ui128ToHash) == false || memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 0);

    RangeBanItem *next = clsBanManager::mPtr->RangeBanListS;

    while(next != NULL) {
        RangeBanItem *cur = next;
        next = cur->next;

        if(memcmp(cur->ui128FromIpHash, ui128FromHash, 16) == 0 && memcmp(cur->ui128ToIpHash, ui128ToHash, 16) == 0) {
            if(((cur->ui8Bits & clsBanManager::PERM) == clsBanManager::PERM) == true) {
                PushRangeBan(L, cur);
                return 1;
            }
        }
    }

	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int GetRangeTempBan(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetRangeTempBan' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromLen, szToLen;
    char * sFrom = (char *)lua_tolstring(L, 1, &szFromLen);
    char * sTo = (char *)lua_tolstring(L, 2, &szToLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromLen == 0 || szToLen == 0 || HashIP(sFrom, ui128FromHash) == false || HashIP(sTo, ui128ToHash) == false || memcmp(ui128ToHash, ui128FromHash, 16) <= 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 0);

    time_t acc_time;
    time(&acc_time);

    RangeBanItem *next = clsBanManager::mPtr->RangeBanListS;

    while(next != NULL) {
        RangeBanItem *cur = next;
        next = cur->next;

        if(memcmp(cur->ui128FromIpHash, ui128FromHash, 16) == 0 && memcmp(cur->ui128ToIpHash, ui128ToHash, 16) == 0) {
            // PPK ... check if it's temban and then if it's expired
            if(((cur->ui8Bits & clsBanManager::TEMP) == clsBanManager::TEMP) == true) {
                if(acc_time >= cur->tempbanexpire) {
					clsBanManager::mPtr->RemRange(cur);
                    delete cur;

					continue;
                }

                PushRangeBan(L, cur);
                return 1;
            }
        }
    }

	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int Unban(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'Unban' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szLen;
    char * sWhat = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }
    
    if(clsBanManager::mPtr->Unban(sWhat) == false) {
        lua_settop(L, 0);
        lua_pushnil(L);
    } else {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int UnbanPerm(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnbanPerm' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szLen;
    char * sWhat = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }
    
    if(clsBanManager::mPtr->PermUnban(sWhat) == false) {
        lua_settop(L, 0);
        lua_pushnil(L);
    } else {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int UnbanTemp(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnbanTemp' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szLen;
    char * sWhat = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }
    
    if(clsBanManager::mPtr->TempUnban(sWhat) == false) {
        lua_settop(L, 0);
        lua_pushnil(L);
    } else {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int UnbanAll(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnbanAll' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

    if(szLen == 0 || HashIP(sIP, ui128Hash) == false) {
		lua_settop(L, 0);
        return 0;
    }

    lua_settop(L, 0);

	clsBanManager::mPtr->RemoveAllIP(ui128Hash);

    return 0;
}
//------------------------------------------------------------------------------

static int UnbanPermAll(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnbanPermAll' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

    if(szLen == 0 || HashIP(sIP, ui128Hash) == false) {
		lua_settop(L, 0);
        return 0;
    }

    lua_settop(L, 0);

	clsBanManager::mPtr->RemovePermAllIP(ui128Hash);

    return 0;
}
//------------------------------------------------------------------------------

static int UnbanTempAll(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnbanTempAll' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szLen);

	uint8_t ui128Hash[16];
	memset(ui128Hash, 0, 16);

    if(szLen == 0 || HashIP(sIP, ui128Hash) == false) {
		lua_settop(L, 0);
        return 0;
    }

    lua_settop(L, 0);

	clsBanManager::mPtr->RemoveTempAllIP(ui128Hash);

    return 0;
}
//------------------------------------------------------------------------------

static int RangeUnban(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'RangeUnban' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromIpLen, szToIpLen;
    char * sFromIp = (char *)lua_tolstring(L, 1, &szFromIpLen);
    char * sToIp = (char *)lua_tolstring(L, 2, &szToIpLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromIpLen != 0 && szToIpLen != 0 && HashIP(sFromIp, ui128FromHash) == true && HashIP(sToIp, ui128ToHash) == true &&
		memcmp(ui128ToHash, ui128FromHash, 16) > 0 && clsBanManager::mPtr->RangeUnban(ui128FromHash, ui128ToHash) == true) {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_settop(L, 0);
    lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int RangeUnbanPerm(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'RangeUnbanPerm' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromIpLen, szToIpLen;
    char * sFromIp = (char *)lua_tolstring(L, 1, &szFromIpLen);
    char * sToIp = (char *)lua_tolstring(L, 2, &szToIpLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromIpLen != 0 && szToIpLen != 0 && HashIP(sFromIp, ui128FromHash) == true && HashIP(sToIp, ui128ToHash) == true &&
		memcmp(ui128ToHash, ui128FromHash, 16) > 0 && clsBanManager::mPtr->RangeUnban(ui128FromHash, ui128ToHash, clsBanManager::PERM) == true) {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_settop(L, 0);
    lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int RangeUnbanTemp(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'RangeUnbanTemp' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromIpLen, szToIpLen;
    char * sFromIp = (char *)lua_tolstring(L, 1, &szFromIpLen);
    char * sToIp = (char *)lua_tolstring(L, 2, &szToIpLen);

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromIpLen != 0 && szToIpLen != 0 && HashIP(sFromIp, ui128FromHash) == true && HashIP(sToIp, ui128ToHash) == true &&
		memcmp(ui128ToHash, ui128FromHash, 16) > 0 && clsBanManager::mPtr->RangeUnban(ui128FromHash, ui128ToHash, clsBanManager::TEMP) == true) {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_settop(L, 0);
    lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int ClearBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsBanManager::mPtr->ClearTemp();
	clsBanManager::mPtr->ClearPerm();

    return 0;
}
//------------------------------------------------------------------------------

static int ClearPermBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearPermBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsBanManager::mPtr->ClearPerm();

    return 0;
}
//------------------------------------------------------------------------------

static int ClearTempBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearTempBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsBanManager::mPtr->ClearTemp();

    return 0;
}
//------------------------------------------------------------------------------

static int ClearRangeBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearRangeBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsBanManager::mPtr->ClearRange();

    return 0;
}
//------------------------------------------------------------------------------

static int ClearRangePermBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearRangePermBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsBanManager::mPtr->ClearPermRange();

    return 0;
}
//------------------------------------------------------------------------------

static int ClearRangeTempBans(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ClearRangeTempBans' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    clsBanManager::mPtr->ClearTempRange();

    return 0;
}
//------------------------------------------------------------------------------

static int Ban(lua_State * L) {
	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'Ban' (4 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING || lua_type(L, 4) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 4, "Ban");
                
    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 2, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

	size_t szByLen;
    char *sBy = (char *)lua_tolstring(L, 3, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 4) == 0 ? false : true;

    clsBanManager::mPtr->Ban(u, sReason, sBy, bFull);

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] User %s (%s) banned by script.", u->sNick, u->sIP);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Ban") == true) {
        clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
    }

    u->Close();

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int BanIP(lua_State * L) {
	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'Ban' (4 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING || lua_type(L, 4) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szIpLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szIpLen);
    if(szIpLen == 0) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 2, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 3, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 4) == 0 ? false : true;

    if(clsBanManager::mPtr->BanIp(NULL, sIP, sReason, sBy, bFull) == 0) {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int BanNick(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'BanNick' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szNickLen;
    char * sNick = (char *)lua_tolstring(L, 1, &szNickLen);
    if(szNickLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 2, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 3, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    User *curUser = clsHashManager::mPtr->FindUser(sNick, szNickLen);
    if(curUser != NULL) {
        if(clsBanManager::mPtr->NickBan(curUser, NULL, sReason, sBy) == true) {
            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] User %s (%s) nickbanned by script.", curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "NickBan") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }
            curUser->Close();
            lua_pushboolean(L, 1);
        } else {
            curUser->Close();
            lua_pushnil(L);
        }
    } else {
        if(clsBanManager::mPtr->NickBan(NULL, sNick, sReason, sBy) == true) {
            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick %s nickbanned by script.", sNick);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "NickBan1") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }
            lua_pushboolean(L, 1);
        } else {
            lua_pushnil(L);
        }
    }

    return 1;
}
//------------------------------------------------------------------------------

static int TempBan(lua_State * L) {
	if(lua_gettop(L) != 5) {
        luaL_error(L, "bad argument count to 'TempBan' (5 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TSTRING || 
        lua_type(L, 4) != LUA_TSTRING || lua_type(L, 5) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        luaL_checktype(L, 5, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 5, "TempBan");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint32_t iMinutes = (uint32_t)lua_tonumber(L, 2);
#else
	uint32_t iMinutes = (uint32_t)lua_tounsigned(L, 2);
#endif

	size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 3, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

	size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 4, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 5) == 0 ? false : true;

    clsBanManager::mPtr->TempBan(u, sReason, sBy, iMinutes, 0, bFull);

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] User %s (%s) tempbanned by script.", u->sNick, u->sIP);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "TempBan2") == true) {
        clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
    }

    u->Close();

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int TempBanIP(lua_State * L) {
	if(lua_gettop(L) != 5) {
        luaL_error(L, "bad argument count to 'TempBanIP' (5 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TSTRING || 
        lua_type(L, 4) != LUA_TSTRING || lua_type(L, 5) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        luaL_checktype(L, 5, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szIpLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szIpLen);
    if(szIpLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint32_t i32Minutes = (uint32_t)lua_tonumber(L, 2);
#else
	uint32_t i32Minutes = (uint32_t)lua_tounsigned(L, 2);
#endif

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 3, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 4, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 5) == 0 ? false : true;

    if(clsBanManager::mPtr->TempBanIp(NULL, sIP, sReason, sBy, i32Minutes, 0, bFull) == 0) {
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
    } else {
        lua_settop(L, 0);
        lua_pushnil(L);
    }

    return 1;
}
//------------------------------------------------------------------------------
// NickTempBan(Nick, iTime, sReason, sBy) and user:NickTempBan(iTime, sReason, sBy)
static int TempBanNick(lua_State * L) {
	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'TempBanNick' (4 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TSTRING || lua_type(L, 4) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szNickLen;
    char * sNick = (char *)lua_tolstring(L, 1, &szNickLen);
    if(szNickLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint32_t i32Minutes = (uint32_t)lua_tonumber(L, 2);
#else
	uint32_t i32Minutes = (uint32_t)lua_tounsigned(L, 2);
#endif

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 3, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 4, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    User *curUser = clsHashManager::mPtr->FindUser(sNick, szNickLen);
    if(curUser != NULL) {
        if(clsBanManager::mPtr->NickTempBan(curUser, NULL, sReason, sBy, i32Minutes, 0) == true) {
            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] User %s (%s) nickbanned by script.", curUser->sNick, curUser->sIP);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "NickTempBan2") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }
            curUser->Close();
            lua_pushboolean(L, 1);
        } else {
            curUser->Close();
            lua_pushnil(L);
        }
    } else {
        if(clsBanManager::mPtr->NickTempBan(NULL, sNick, sReason, sBy, i32Minutes, 0) == true) {
            int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Nick %s nickbanned by script.", sNick);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "NickTempBan3") == true) {
                clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
            }
            lua_pushboolean(L, 1);
        } else {
            lua_pushnil(L);
        }
    }

    return 1;
}
//------------------------------------------------------------------------------

static int RangeBan(lua_State * L) {
	if(lua_gettop(L) != 5) {
        luaL_error(L, "bad argument count to 'RangeBan' (5 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING || 
        lua_type(L, 4) != LUA_TSTRING || lua_type(L, 5) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        luaL_checktype(L, 5, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromIpLen;
    char * sFromIP = (char *)lua_tolstring(L, 1, &szFromIpLen);

    size_t szToIpLen;
    char * sToIP = (char *)lua_tolstring(L, 2, &szToIpLen);

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 3, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 4, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 5) == 0 ? false : true;

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromIpLen != 0 && szToIpLen != 0 && HashIP(sFromIP, ui128FromHash) == true && HashIP(sToIP, ui128ToHash) == true &&
        memcmp(ui128ToHash, ui128FromHash, 16) > 0 && clsBanManager::mPtr->RangeBan(sFromIP, ui128FromHash, sToIP, ui128ToHash, sReason, sBy, bFull) == true) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
        return 1;
    }

	lua_settop(L, 0);
	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int RangeTempBan(lua_State * L) {
	if(lua_gettop(L) != 6) {
        luaL_error(L, "bad argument count to 'RangeTempBan' (6 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TNUMBER || 
        lua_type(L, 4) != LUA_TSTRING || lua_type(L, 5) != LUA_TSTRING || lua_type(L, 6) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TNUMBER);
        luaL_checktype(L, 4, LUA_TSTRING);
        luaL_checktype(L, 5, LUA_TSTRING);
        luaL_checktype(L, 6, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szFromIpLen;
    char * sFromIP = (char *)lua_tolstring(L, 1, &szFromIpLen);

    size_t szToIpLen;
    char * sToIP = (char *)lua_tolstring(L, 2, &szToIpLen);

#if LUA_VERSION_NUM < 503
	uint32_t i32Minutes = (uint32_t)lua_tonumber(L, 3);
#else
    uint32_t i32Minutes = (uint32_t)lua_tounsigned(L, 3);
#endif

    size_t szReasonLen;
    char * sReason = (char *)lua_tolstring(L, 4, &szReasonLen);
    if(szReasonLen == 0) {
        sReason = NULL;
    }

    size_t szByLen;
    char * sBy = (char *)lua_tolstring(L, 5, &szByLen);
    if(szByLen == 0) {
        sBy = NULL;
    }

    bool bFull = lua_toboolean(L, 6) == 0 ? false : true;

	uint8_t ui128FromHash[16], ui128ToHash[16];
	memset(ui128FromHash, 0, 16);
	memset(ui128ToHash, 0, 16);

	if(szFromIpLen != 0 && szToIpLen != 0 && HashIP(sFromIP, ui128FromHash) == true && HashIP(sToIP, ui128ToHash) == true &&
		memcmp(ui128ToHash, ui128FromHash, 16) > 0 && clsBanManager::mPtr->RangeTempBan(sFromIP, ui128FromHash, sToIP, ui128ToHash, sReason, sBy, i32Minutes, 0, bFull) == true) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
        return 1;
    }

	lua_settop(L, 0);
	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static const luaL_Reg banman[] = {
    { "Save", Save },
	{ "GetBans", GetBans }, 
	{ "GetTempBans", GetTempBans }, 
	{ "GetPermBans", GetPermBans }, 
	{ "GetBan", GetBan }, 
	{ "GetPermBan", GetPermBan }, 
	{ "GetTempBan", GetTempBan }, 
	{ "GetRangeBans", GetRangeBans }, 
	{ "GetTempRangeBans", GetTempRangeBans }, 
	{ "GetPermRangeBans", GetPermRangeBans }, 
	{ "GetRangeBan", GetRangeBan }, 
	{ "GetRangePermBan", GetRangePermBan }, 
	{ "GetRangeTempBan", GetRangeTempBan }, 
	{ "Unban", Unban }, 
	{ "UnbanPerm", UnbanPerm }, 
	{ "UnbanTemp", UnbanTemp }, 
	{ "UnbanAll", UnbanAll }, 
	{ "UnbanPermAll", UnbanPermAll }, 
	{ "UnbanTempAll", UnbanTempAll }, 
	{ "RangeUnban", RangeUnban }, 
	{ "RangeUnbanPerm", RangeUnbanPerm }, 
	{ "RangeUnbanTemp", RangeUnbanTemp }, 
	{ "ClearBans", ClearBans }, 
	{ "ClearPermBans", ClearPermBans }, 
	{ "ClearTempBans", ClearTempBans }, 
	{ "ClearRangeBans", ClearRangeBans }, 
	{ "ClearRangePermBans", ClearRangePermBans }, 
	{ "ClearRangeTempBans", ClearRangeTempBans }, 
	{ "Ban", Ban }, 
	{ "BanIP", BanIP }, 
	{ "BanNick", BanNick }, 
	{ "TempBan", TempBan }, 
	{ "TempBanIP", TempBanIP }, 
	{ "TempBanNick", TempBanNick }, 
	{ "RangeBan", RangeBan }, 
	{ "RangeTempBan", RangeTempBan }, 
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegBanMan(lua_State * L) {
    luaL_newlib(L, banman);
    return 1;
#else
void RegBanMan(lua_State * L) {
    luaL_register(L, "BanMan", banman);
#endif
}
//---------------------------------------------------------------------------
