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
#include "LuaCoreLib.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "eventqueue.h"
#include "GlobalDataQueue.h"
#include "hashBanManager.h"
#include "hashUsrManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "UdpDebug.h"
#include "User.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "IP2Country.h"
#include "ResNickManager.h"
#include "LuaScript.h"
//---------------------------------------------------------------------------

static int Restart(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Restart' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsEventQueue::mPtr->AddNormal(clsEventQueue::EVENT_RESTART, NULL);

    return 0;
}
//------------------------------------------------------------------------------

static int Shutdown(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Shutdown' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsEventQueue::mPtr->AddNormal(clsEventQueue::EVENT_SHUTDOWN, NULL);

    return 0;
}
//------------------------------------------------------------------------------

static int ResumeAccepts(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'ResumeAccepts' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsServerManager::ResumeAccepts();

    return 0;
}
//------------------------------------------------------------------------------

static int SuspendAccepts(lua_State * L) {
    int n = lua_gettop(L);

    if(n == 0) {
        clsServerManager::SuspendAccepts(0);
    } else if(n == 1) {
        if(lua_type(L, 1) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TNUMBER);
    		lua_settop(L, 0);
            return 0;
        }

#if LUA_VERSION_NUM < 503
		uint32_t iSec = (uint32_t)lua_tonumber(L, 1);
#else
    	uint32_t iSec = (uint32_t)lua_tounsigned(L, 1);
#endif
    
        if(iSec != 0) {
            clsServerManager::SuspendAccepts(iSec);
        }
        lua_settop(L, 0);
    } else {
        luaL_error(L, "bad argument count to 'SuspendAccepts' (0 or 1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
    }
    
    return 0;
}
//------------------------------------------------------------------------------

static int RegBot(lua_State * L) {
    if(clsScriptManager::mPtr->ui8BotsCount > 63) {
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'RegBot' (4 expected, got %d)", lua_gettop(L));
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

    size_t szNickLen, szDescrLen, szEmailLen;

    char *nick = (char *)lua_tolstring(L, 1, &szNickLen);
    char *description = (char *)lua_tolstring(L, 2, &szDescrLen);
    char *email = (char *)lua_tolstring(L, 3, &szEmailLen);

    bool bIsOP = lua_toboolean(L, 4) == 0 ? false : true;

    if(szNickLen == 0 || szNickLen > 64 || strpbrk(nick, " $|") != NULL || szDescrLen > 64 || strpbrk(description, "$|") != NULL ||
        szEmailLen > 64 || strpbrk(email, "$|") != NULL || clsHashManager::mPtr->FindUser(nick, szNickLen) != NULL || clsReservedNicksManager::mPtr->CheckReserved(nick, HashNick(nick, szNickLen)) != false) {
		lua_settop(L, 0);

		lua_pushnil(L);
        return 1;
    }

    ScriptBot * pNewBot = ScriptBot::CreateScriptBot(nick, szNickLen, description, szDescrLen, email, szEmailLen, bIsOP);
    if(pNewBot == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate pNewBot in Core.RegBot\n", 0);

		lua_settop(L, 0);

		lua_pushnil(L);
        return 1;
    }

    // PPK ... finally we can clear stack
    lua_settop(L, 0);

	Script * obj = clsScriptManager::mPtr->FindScript(L);
	if(obj == NULL) {
		delete pNewBot;

		lua_pushnil(L);
        return 1;
    }

    ScriptBot * next = obj->BotList;

    while(next != NULL) {
        ScriptBot * cur = next;
        next = cur->next;

		if(strcasecmp(pNewBot->sNick, cur->sNick) == 0) {
            delete pNewBot;

            lua_pushnil(L);
            return 1;
        }
    }

    if(obj->BotList == NULL) {
        obj->BotList = pNewBot;
    } else {
        obj->BotList->prev = pNewBot;
        pNewBot->next = obj->BotList;
        obj->BotList = pNewBot;
    }

	clsReservedNicksManager::mPtr->AddReservedNick(pNewBot->sNick, true);

	clsUsers::mPtr->AddBot2NickList(pNewBot->sNick, szNickLen, pNewBot->bIsOP);

    clsUsers::mPtr->AddBot2MyInfos(pNewBot->sMyINFO);

    // PPK ... fixed hello sending only to users without NoHello
    int iMsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Hello %s|", pNewBot->sNick);
    if(CheckSprintf(iMsgLen, clsServerManager::szGlobalBufferSize, "RegBot") == true) {
        clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, iMsgLen, NULL, 0, clsGlobalDataQueue::CMD_HELLO);
    }
    
    clsGlobalDataQueue::mPtr->AddQueueItem(pNewBot->sMyINFO, strlen(pNewBot->sMyINFO), NULL, 0, clsGlobalDataQueue::CMD_MYINFO);
        
    if(pNewBot->bIsOP == true) {
        clsGlobalDataQueue::mPtr->OpListStore(pNewBot->sNick);
    }

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int UnregBot(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'UnregBot' (1 expected, got %d)", lua_gettop(L));
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
    char * botnick = (char *)lua_tolstring(L, 1, &szLen);
    
    if(szLen == 0) {
		lua_settop(L, 0);

		lua_pushnil(L);
        return 1;
    }

	Script * obj = clsScriptManager::mPtr->FindScript(L);
	if(obj == NULL) {
		lua_settop(L, 0);

		lua_pushnil(L);
        return 1;
    }

    ScriptBot * next = obj->BotList;

    while(next != NULL) {
        ScriptBot * cur = next;
        next = cur->next;

		if(strcasecmp(botnick, cur->sNick) == 0) {
            clsReservedNicksManager::mPtr->DelReservedNick(cur->sNick, true);

            clsUsers::mPtr->DelFromNickList(cur->sNick, cur->bIsOP);

            clsUsers::mPtr->DelBotFromMyInfos(cur->sMyINFO);

            int iMsgLen = sprintf(clsServerManager::sGlobalBuffer, "$Quit %s|", cur->sNick);
            if(CheckSprintf(iMsgLen, clsServerManager::szGlobalBufferSize, "UnregBot") == true) {
                clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, iMsgLen, NULL, 0, clsGlobalDataQueue::CMD_QUIT);
            }

            if(cur->prev == NULL) {
                if(cur->next == NULL) {
                    obj->BotList = NULL;
                } else {
                    cur->next->prev = NULL;
                    obj->BotList = cur->next;
                }
            } else if(cur->next == NULL) {
                cur->prev->next = NULL;
            } else {
                cur->prev->next = cur->next;
                cur->next->prev = cur->prev;
            }

            delete cur;

            lua_settop(L, 0);

            lua_pushboolean(L, 1);
            return 1;
        }
    }

    lua_settop(L, 0);

    lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int GetBots(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetBots' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);

    int t = lua_gettop(L), i = 0;

	Script *next = clsScriptManager::mPtr->RunningScriptS;

    while(next != NULL) {
    	Script *cur = next;
        next = cur->next;

        ScriptBot * next = cur->BotList;
        
        while(next != NULL) {
            ScriptBot * bot = next;
            next = bot->next;

#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif

            lua_newtable(L);

            int b = lua_gettop(L);

        	lua_pushliteral(L, "sNick");
        	lua_pushstring(L, bot->sNick);
        	lua_rawset(L, b);

        	lua_pushliteral(L, "sMyINFO");
        	lua_pushstring(L, bot->sMyINFO);
        	lua_rawset(L, b);

        	lua_pushliteral(L, "bIsOP");
        	bot->bIsOP == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
        	lua_rawset(L, b);

        	lua_pushliteral(L, "sScriptName");
        	lua_pushstring(L, cur->sName);
        	lua_rawset(L, b);

            lua_rawset(L, t);
        }

    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetActualUsersPeak(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetActualUsersPeak' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, clsServerManager::ui32Peak);
#else
    lua_pushunsigned(L, clsServerManager::ui32Peak);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetMaxUsersPeak(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetMaxUsersPeak' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS_PEAK]);
#else
    lua_pushunsigned(L, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_USERS_PEAK]);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetCurrentSharedSize(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetCurrentSharedSize' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)clsServerManager::ui64TotalShare);
#else
    lua_pushunsigned(L, clsServerManager::ui64TotalShare);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetHubIP(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetHubIP' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    if(clsServerManager::sHubIP[0] != '\0') {
		lua_pushstring(L, clsServerManager::sHubIP);
	} else {
		lua_pushnil(L);
	}

    return 1;
}
//------------------------------------------------------------------------------

static int GetHubIPs(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetHubIPs' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    if(clsServerManager::sHubIP[0] == '\0' && clsServerManager::sHubIP6[0] == '\0') {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

    if(clsServerManager::sHubIP6[0] != '\0') {
        i++;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, i);
#else
        lua_pushunsigned(L, i);
#endif

		lua_pushstring(L, clsServerManager::sHubIP6);
		lua_rawset(L, t);
	}

    if(clsServerManager::sHubIP[0] != '\0') {
        i++;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, i);
#else
        lua_pushunsigned(L, i);
#endif

		lua_pushstring(L, clsServerManager::sHubIP);
		lua_rawset(L, t);
	}

    return 1;
}
//------------------------------------------------------------------------------

static int GetHubSecAlias(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetHubSecAlias' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }
    
    lua_pushlstring(L, clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
        (size_t)clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_HUB_SEC]);

    return 1;
}
//------------------------------------------------------------------------------

static int GetPtokaXPath(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetPtokaXPath' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

#ifdef _WIN32
    lua_pushlstring(L, clsServerManager::sLuaPath.c_str(), clsServerManager::sLuaPath.size());
#else
    string path = clsServerManager::sPath+"/";
    lua_pushlstring(L, path.c_str(), path.size());
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetUsersCount(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetUsersCount' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, clsServerManager::ui32Logged);
#else
    lua_pushunsigned(L, clsServerManager::ui32Logged);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetUpTime(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetUpTime' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

	time_t t;
    time(&t);

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)(t-clsServerManager::tStartTime));
#else
    lua_pushunsigned(L, t-clsServerManager::tStartTime);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int GetOnlineByOpStatus(lua_State * L, const bool &bOperator) {
    bool bFullTable = false;

    int n = lua_gettop(L);

	if(n != 0) {
        if(n != 1) {
            if(bOperator == true) {
                luaL_error(L, "bad argument count to 'GetOnlineOps' (0 or 1 expected, got %d)", lua_gettop(L));
            } else {
                luaL_error(L, "bad argument count to 'GetOnlineNonOps' (0 or 1 expected, got %d)", lua_gettop(L));
            }
            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        } else if(lua_type(L, 1) != LUA_TBOOLEAN) {
            luaL_checktype(L, 1, LUA_TBOOLEAN);

            lua_settop(L, 0);

            lua_pushnil(L);
            return 1;
        } else {
            bFullTable = lua_toboolean(L, 1) == 0 ? false : true;

            lua_settop(L, 0);
        }
    }

    lua_newtable(L);

    int t = lua_gettop(L), i = 0;

    User *next = clsUsers::mPtr->llist;

    while(next != NULL) {
        User *curUser = next;
        next = curUser->next;

        if(curUser->ui8State == User::STATE_ADDED && ((curUser->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR) == bOperator) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif

			ScriptPushUser(L, curUser, bFullTable);
            lua_rawset(L, t);
        }
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetOnlineNonOps(lua_State * L) {
	return GetOnlineByOpStatus(L, false);
}
//------------------------------------------------------------------------------

static int GetOnlineOps(lua_State * L) {
    return GetOnlineByOpStatus(L, true);
}
//------------------------------------------------------------------------------

static int GetOnlineRegs(lua_State * L) {
    bool bFullTable = false;

    int n = lua_gettop(L);

	if(n != 0) {
        if(n != 1) {
            luaL_error(L, "bad argument count to 'GetOnlineRegs' (0 or 1 expected, got %d)", lua_gettop(L));
            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        } else if(lua_type(L, 1) != LUA_TBOOLEAN) {
            luaL_checktype(L, 1, LUA_TBOOLEAN);

            lua_settop(L, 0);

            lua_pushnil(L);
            return 1;
        } else {
            bFullTable = lua_toboolean(L, 1) == 0 ? false : true;

            lua_settop(L, 0);
        }
    }

    lua_newtable(L);

    int t = lua_gettop(L), i = 0;

    User *next = clsUsers::mPtr->llist;

    while(next != NULL) {
        User *curUser = next;
        next = curUser->next;
        if(curUser->ui8State == User::STATE_ADDED && curUser->iProfile != -1) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif

			ScriptPushUser(L, curUser, bFullTable);
            lua_rawset(L, t);
        }
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetOnlineUsers(lua_State * L) {
    bool bFullTable = false;

    int32_t iProfile = -2;

    int n = lua_gettop(L);

    if(n == 2) {
        if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TBOOLEAN) {
            luaL_checktype(L, 1, LUA_TNUMBER);
            luaL_checktype(L, 2, LUA_TBOOLEAN);
    
            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }

        iProfile = (int32_t)lua_tointeger(L, 1);
        bFullTable = lua_toboolean(L, 2) == 0 ? false : true;

        lua_settop(L, 0);
    } else if(n == 1) {
        if(lua_type(L, 1) == LUA_TNUMBER) {
            iProfile = (int32_t)lua_tointeger(L, 1);
        } else if(lua_type(L, 1) == LUA_TBOOLEAN) {
            bFullTable = lua_toboolean(L, 1) == 0 ? false : true;
        } else {
            luaL_error(L, "bad argument #1 to 'GetOnlineUsers' (number or boolean expected, got %s)", lua_typename(L, lua_type(L, 1)));
            lua_settop(L, 0);
        
            lua_pushnil(L);
            return 1;
        }

        lua_settop(L, 0);
    } else if(n != 0) {
        luaL_error(L, "bad argument count to 'GetOnlineUsers' (0, 1 or 2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);

    int t = lua_gettop(L), i = 0;

    User *next = clsUsers::mPtr->llist;

    if(iProfile == -2) {
	    while(next != NULL) {
	        User *curUser = next;
	        next = curUser->next;

	        if(curUser->ui8State == User::STATE_ADDED) {
#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, ++i);
#else
	            lua_pushunsigned(L, ++i);
#endif

				ScriptPushUser(L, curUser, bFullTable);
	            lua_rawset(L, t);
	        }
	    }
    } else {
		while(next != NULL) {
		    User *curUser = next;
		    next = curUser->next;

		    if(curUser->ui8State == User::STATE_ADDED && curUser->iProfile == iProfile) {
#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, ++i);
#else
		        lua_pushunsigned(L, ++i);
#endif

				ScriptPushUser(L, curUser, bFullTable);
		        lua_rawset(L, t);
		    }
		}
    }
    
    return 1;
}
//------------------------------------------------------------------------------

static int GetUser(lua_State * L) {
    bool bFullTable = false;

    size_t szLen = 0;

    char * nick;
    
    int n = lua_gettop(L);

    if(n == 2) {
        if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TBOOLEAN) {
            luaL_checktype(L, 1, LUA_TSTRING);
            luaL_checktype(L, 2, LUA_TBOOLEAN);
    
            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }

        nick = (char *)lua_tolstring(L, 1, &szLen);

        bFullTable = lua_toboolean(L, 2) == 0 ? false : true;
    } else if(n == 1) {
        if(lua_type(L, 1) != LUA_TSTRING) {
            luaL_checktype(L, 1, LUA_TSTRING);

            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }

        nick = (char *)lua_tolstring(L, 1, &szLen);
    } else {
        luaL_error(L, "bad argument count to 'GetUser' (1 or 2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        
        lua_pushnil(L);
        return 1;
    }

    if(szLen == 0) {
        lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }
    
    User *u = clsHashManager::mPtr->FindUser(nick, szLen);

    lua_settop(L, 0);

    if(u != NULL) {
		ScriptPushUser(L, u, bFullTable);
	} else {
        lua_pushnil(L);
    }
    
    return 1;
}
//------------------------------------------------------------------------------

static int GetUsers(lua_State * L) {
    bool bFullTable = false;

    size_t szLen = 0;

    char * sIP;
    
    int n = lua_gettop(L);

    if(n == 2) {
        if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TBOOLEAN) {
            luaL_checktype(L, 1, LUA_TSTRING);
            luaL_checktype(L, 2, LUA_TBOOLEAN);
    
            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }

        sIP = (char *)lua_tolstring(L, 1, &szLen);

        bFullTable = lua_toboolean(L, 2) == 0 ? false : true;
    } else if(n == 1) {
        if(lua_type(L, 1) != LUA_TSTRING) {
            luaL_checktype(L, 1, LUA_TSTRING);

            lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }

        sIP = (char *)lua_tolstring(L, 1, &szLen);
    } else {
        luaL_error(L, "bad argument count to 'GetUsers' (1 or 2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        
        lua_pushnil(L);
        return 1;
    }

    uint8_t ui128Hash[16];
    memset(ui128Hash, 0, 16);

    if(szLen == 0 || HashIP(sIP, ui128Hash) == false) {
        lua_settop(L, 0);
        lua_pushnil(L);

        return 1;
    }

    User *next = clsHashManager::mPtr->FindUser(ui128Hash);

    lua_settop(L, 0);

    if(next == NULL) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    
    int t = lua_gettop(L), i = 0;
    
    while(next != NULL) {
		User *curUser = next;
        next = curUser->hashiptablenext;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif

    	ScriptPushUser(L, curUser, bFullTable);
        lua_rawset(L, t);
    }
    
    return 1;
}
//------------------------------------------------------------------------------

static int GetUserAllData(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetUserAllData' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE) {
        luaL_checktype(L, 1, LUA_TTABLE);
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 1, "GetUserAllData");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    ScriptPushUserExtended(L, u, 1);

    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int GetUserData(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetUserData' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TNUMBER);
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 2, "GetUserData");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint8_t ui8DataId = (uint8_t)lua_tonumber(L, 2);
#else
    uint8_t ui8DataId = (uint8_t)lua_tounsigned(L, 2);
#endif

    switch(ui8DataId) {
        case 0:
        	lua_pushliteral(L, "sMode");
        	if(u->sModes[0] != '\0') {
				lua_pushstring(L, u->sModes);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 1:
        	lua_pushliteral(L, "sMyInfoString");
        	if(u->sMyInfoOriginal != NULL) {
				lua_pushlstring(L, u->sMyInfoOriginal, u->ui16MyInfoOriginalLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 2:
        	lua_pushliteral(L, "sDescription");
        	if(u->sDescription != NULL) {
				lua_pushlstring(L, u->sDescription, (size_t)u->ui8DescriptionLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 3:
        	lua_pushliteral(L, "sTag");
        	if(u->sTag != NULL) {
				lua_pushlstring(L, u->sTag, (size_t)u->ui8TagLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 4:
        	lua_pushliteral(L, "sConnection");
        	if(u->sConnection != NULL) {
				lua_pushlstring(L, u->sConnection, (size_t)u->ui8ConnectionLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 5:
        	lua_pushliteral(L, "sEmail");
        	if(u->sEmail != NULL) {
				lua_pushlstring(L, u->sEmail, (size_t)u->ui8EmailLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 6:
        	lua_pushliteral(L, "sClient");
        	if(u->sClient != NULL) {
				lua_pushlstring(L, u->sClient, (size_t)u->ui8ClientLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 7:
        	lua_pushliteral(L, "sClientVersion");
        	if(u->sTagVersion != NULL) {
				lua_pushlstring(L, u->sTagVersion, (size_t)u->ui8TagVersionLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 8:
        	lua_pushliteral(L, "sVersion");
        	if(u->sVersion!= NULL) {
				lua_pushstring(L, u->sVersion);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 9:
        	lua_pushliteral(L, "bConnected");
        	u->ui8State == User::STATE_ADDED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 10:
        	lua_pushliteral(L, "bActive");
        	if((u->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
                (u->ui32BoolBits & User::BIT_IPV6_ACTIVE) == User::BIT_IPV6_ACTIVE ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
            } else {
                (u->ui32BoolBits & User::BIT_IPV4_ACTIVE) == User::BIT_IPV4_ACTIVE ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
            }
        	lua_rawset(L, 1);
            break;
        case 11:
        	lua_pushliteral(L, "bOperator");
        	(u->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 12:
        	lua_pushliteral(L, "bUserCommand");
        	(u->ui32SupportBits & User::SUPPORTBIT_USERCOMMAND) == User::SUPPORTBIT_USERCOMMAND ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 13:
        	lua_pushliteral(L, "bQuickList");
        	(u->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 14:
        	lua_pushliteral(L, "bSuspiciousTag");
        	(u->ui32BoolBits & User::BIT_HAVE_BADTAG) == User::BIT_HAVE_BADTAG ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 15:
        	lua_pushliteral(L, "iProfile");
        	lua_pushinteger(L, u->iProfile);
        	lua_rawset(L, 1);
            break;
        case 16:
        	lua_pushliteral(L, "iShareSize");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64SharedSize);
#else
        	lua_pushunsigned(L, u->ui64SharedSize);
#endif
        	lua_rawset(L, 1);
            break;
        case 17:
        	lua_pushliteral(L, "iHubs");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->Hubs);
#else
        	lua_pushunsigned(L, u->Hubs);
#endif
        	lua_rawset(L, 1);
            break;
        case 18:
        	lua_pushliteral(L, "iNormalHubs");
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iNormalHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iNormalHubs);
#endif
        	lua_rawset(L, 1);
            break;
        case 19:
        	lua_pushliteral(L, "iRegHubs");
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iRegHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iRegHubs);
#endif
        	lua_rawset(L, 1);
            break;
        case 20:
        	lua_pushliteral(L, "iOpHubs");
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iOpHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iOpHubs);
#endif
        	lua_rawset(L, 1);
            break;
        case 21:
        	lua_pushliteral(L, "iSlots");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->Slots);
#else
        	lua_pushunsigned(L, u->Slots);
#endif
        	lua_rawset(L, 1);
            break;
        case 22:
        	lua_pushliteral(L, "iLlimit");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->LLimit);
#else
        	lua_pushunsigned(L, u->LLimit);
#endif
        	lua_rawset(L, 1);
            break;
        case 23:
        	lua_pushliteral(L, "iDefloodWarns");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->iDefloodWarnings);
#else
        	lua_pushunsigned(L, u->iDefloodWarnings);
#endif
        	lua_rawset(L, 1);
            break;
        case 24:
        	lua_pushliteral(L, "iMagicByte");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->MagicByte);
#else
        	lua_pushunsigned(L, u->MagicByte);
#endif
        	lua_rawset(L, 1);
            break;   
        case 25:
        	lua_pushliteral(L, "iLoginTime");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->LoginTime);
#else
        	lua_pushunsigned(L, u->LoginTime);
#endif
        	lua_rawset(L, 1);
            break;
        case 26:
        	lua_pushliteral(L, "sCountryCode");
        	if(clsIpP2Country::mPtr->ui32Count != 0) {
				lua_pushlstring(L, clsIpP2Country::mPtr->GetCountry(u->ui8Country, false), 2);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 27: {
            lua_pushliteral(L, "sMac");
            char sMac[18];
            if(GetMacAddress(u->sIP, sMac) == true) {
                lua_pushlstring(L, sMac, 17);
            } else {
                lua_pushnil(L);
            }
        	lua_rawset(L, 1);
            break;
        }
        case 28:
            lua_pushliteral(L, "bDescriptionChanged");
        	(u->ui32InfoBits & User::INFOBIT_DESCRIPTION_CHANGED) == User::INFOBIT_DESCRIPTION_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 29:
            lua_pushliteral(L, "bTagChanged");
        	(u->ui32InfoBits & User::INFOBIT_TAG_CHANGED) == User::INFOBIT_TAG_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 30:
            lua_pushliteral(L, "bConnectionChanged");
        	(u->ui32InfoBits & User::INFOBIT_CONNECTION_CHANGED) == User::INFOBIT_CONNECTION_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 31:
            lua_pushliteral(L, "bEmailChanged");
        	(u->ui32InfoBits & User::INFOBIT_EMAIL_CHANGED) == User::INFOBIT_EMAIL_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 32:
            lua_pushliteral(L, "bShareChanged");
        	(u->ui32InfoBits & User::INFOBIT_SHARE_CHANGED) == User::INFOBIT_SHARE_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	lua_rawset(L, 1);
            break;
        case 33:
            lua_pushliteral(L, "sScriptedDescriptionShort");
        	if(u->sChangedDescriptionShort != NULL) {
				lua_pushlstring(L, u->sChangedDescriptionShort, u->ui8ChangedDescriptionShortLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 34:
            lua_pushliteral(L, "sScriptedDescriptionLong");
        	if(u->sChangedDescriptionLong != NULL) {
				lua_pushlstring(L, u->sChangedDescriptionLong, u->ui8ChangedDescriptionLongLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 35:
            lua_pushliteral(L, "sScriptedTagShort");
        	if(u->sChangedTagShort != NULL) {
				lua_pushlstring(L, u->sChangedTagShort, u->ui8ChangedTagShortLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 36:
            lua_pushliteral(L, "sScriptedTagLong");
        	if(u->sChangedTagLong != NULL) {
				lua_pushlstring(L, u->sChangedTagLong, u->ui8ChangedTagLongLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 37:
            lua_pushliteral(L, "sScriptedConnectionShort");
        	if(u->sChangedConnectionShort != NULL) {
				lua_pushlstring(L, u->sChangedConnectionShort, u->ui8ChangedConnectionShortLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 38:
            lua_pushliteral(L, "sScriptedConnectionLong");
        	if(u->sChangedConnectionLong != NULL) {
				lua_pushlstring(L, u->sChangedConnectionLong, u->ui8ChangedConnectionLongLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 39:
            lua_pushliteral(L, "sScriptedEmailShort");
        	if(u->sChangedEmailShort != NULL) {
				lua_pushlstring(L, u->sChangedEmailShort, u->ui8ChangedEmailShortLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 40:
            lua_pushliteral(L, "sScriptedEmailLong");
        	if(u->sChangedEmailLong != NULL) {
				lua_pushlstring(L, u->sChangedEmailLong, u->ui8ChangedEmailLongLen);
			} else {
				lua_pushnil(L);
			}
        	lua_rawset(L, 1);
            break;
        case 41:
            lua_pushliteral(L, "iScriptediShareSizeShort");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64ChangedSharedSizeShort);
#else
        	lua_pushunsigned(L, u->ui64ChangedSharedSizeShort);
#endif
        	lua_rawset(L, 1);
            break;
        case 42:
            lua_pushliteral(L, "iScriptediShareSizeLong");
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64ChangedSharedSizeLong);
#else
        	lua_pushunsigned(L, u->ui64ChangedSharedSizeLong);
#endif
        	lua_rawset(L, 1);
            break;
        case 43: {
            lua_pushliteral(L, "tIPs");
            lua_newtable(L);

            int t = lua_gettop(L);

#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, 1);
#else
            lua_pushunsigned(L, 1);
#endif
            lua_pushlstring(L, u->sIP, u->ui8IpLen);
            lua_rawset(L, t);

            if(u->sIPv4[0] != '\0') {
#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, 2);
#else
                lua_pushunsigned(L, 2);
#endif
                lua_pushlstring(L, u->sIPv4, u->ui8IPv4Len);
                lua_rawset(L, t);
            }

        	lua_rawset(L, 1);
            break;
        }
        default:
            luaL_error(L, "bad argument #2 to 'GetUserData' (it's not valid id)");
    		lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;       
    }

    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int GetUserValue(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetUserValue' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TNUMBER);
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 2, "GetUserValue");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint8_t ui8DataId = (uint8_t)lua_tonumber(L, 2);
#else
    uint8_t ui8DataId = (uint8_t)lua_tounsigned(L, 2);
#endif

    lua_settop(L, 0);

    switch(ui8DataId) {
        case 0:
        	if(u->sModes[0] != '\0') {
				lua_pushstring(L, u->sModes);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 1:
        	if(u->sMyInfoOriginal != NULL) {
				lua_pushlstring(L, u->sMyInfoOriginal, u->ui16MyInfoOriginalLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 2:
        	if(u->sDescription != NULL) {
				lua_pushlstring(L, u->sDescription, u->ui8DescriptionLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 3:
        	if(u->sTag != NULL) {
				lua_pushlstring(L, u->sTag, u->ui8TagLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 4:
        	if(u->sConnection != NULL) {
				lua_pushlstring(L, u->sConnection, u->ui8ConnectionLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 5:
        	if(u->sEmail != NULL) {
				lua_pushlstring(L, u->sEmail, u->ui8EmailLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 6:
        	if(u->sClient != NULL) {
				lua_pushlstring(L, u->sClient, u->ui8ClientLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 7:
        	if(u->sTagVersion != NULL) {
				lua_pushlstring(L, u->sTagVersion, u->ui8TagVersionLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 8:
        	if(u->sVersion != NULL) {
				lua_pushstring(L, u->sVersion);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 9:
        	u->ui8State == User::STATE_ADDED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 10:
            if((u->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6) {
                (u->ui32BoolBits & User::BIT_IPV6_ACTIVE) == User::BIT_IPV6_ACTIVE ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
            } else {
        	   (u->ui32BoolBits & User::BIT_IPV4_ACTIVE) == User::BIT_IPV4_ACTIVE ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
            }
        	return 1;
        case 11:
        	(u->ui32BoolBits & User::BIT_OPERATOR) == User::BIT_OPERATOR ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 12:
        	(u->ui32SupportBits & User::SUPPORTBIT_USERCOMMAND) == User::SUPPORTBIT_USERCOMMAND ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 13:
        	(u->ui32SupportBits & User::SUPPORTBIT_QUICKLIST) == User::SUPPORTBIT_QUICKLIST ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 14:
        	(u->ui32BoolBits & User::BIT_HAVE_BADTAG) == User::BIT_HAVE_BADTAG ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 15:
        	lua_pushinteger(L, u->iProfile);
        	return 1;
        case 16:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64SharedSize);
#else
        	lua_pushunsigned(L, u->ui64SharedSize);
#endif
        	return 1;
        case 17:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->Hubs);
#else
        	lua_pushunsigned(L, u->Hubs);
#endif
        	return 1;
        case 18:
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iNormalHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iNormalHubs);
#endif
        	return 1;
        case 19:
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iRegHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iRegHubs);
#endif
        	return 1;
        case 20:
#if LUA_VERSION_NUM < 503
			(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushnumber(L, u->iOpHubs);
#else
        	(u->ui32BoolBits & User::BIT_OLDHUBSTAG) == User::BIT_OLDHUBSTAG ? lua_pushnil(L) : lua_pushunsigned(L, u->iOpHubs);
#endif
        	return 1;
        case 21:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->Slots);
#else
        	lua_pushunsigned(L, u->Slots);
#endif
        	return 1;
        case 22:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->LLimit);
#else
        	lua_pushunsigned(L, u->LLimit);
#endif
        	return 1;
        case 23:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->iDefloodWarnings);
#else
        	lua_pushunsigned(L, u->iDefloodWarnings);
#endif
        	return 1;
        case 24:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, u->MagicByte);
#else
        	lua_pushunsigned(L, u->MagicByte);
#endif
        	return 1;  
        case 25:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->LoginTime);
#else
        	lua_pushunsigned(L, u->LoginTime);
#endif
        	return 1;
        case 26:
        	if(clsIpP2Country::mPtr->ui32Count != 0) {
				lua_pushlstring(L, clsIpP2Country::mPtr->GetCountry(u->ui8Country, false), 2);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 27: {
            char sMac[18];
            if(GetMacAddress(u->sIP, sMac) == true) {
                lua_pushlstring(L, sMac, 17);
                return 1;
            } else {
                lua_pushnil(L);
                return 1;
            }
        }
        case 28:
        	(u->ui32InfoBits & User::INFOBIT_DESCRIPTION_CHANGED) == User::INFOBIT_DESCRIPTION_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 29:
        	(u->ui32InfoBits & User::INFOBIT_TAG_CHANGED) == User::INFOBIT_TAG_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 30:
        	(u->ui32InfoBits & User::INFOBIT_CONNECTION_CHANGED) == User::INFOBIT_CONNECTION_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 31:
        	(u->ui32InfoBits & User::INFOBIT_EMAIL_CHANGED) == User::INFOBIT_EMAIL_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 32:
        	(u->ui32InfoBits & User::INFOBIT_SHARE_CHANGED) == User::INFOBIT_SHARE_CHANGED ? lua_pushboolean(L, 1) : lua_pushboolean(L, 0);
        	return 1;
        case 33:
        	if(u->sChangedDescriptionShort != NULL) {
				lua_pushlstring(L, u->sChangedDescriptionShort, u->ui8ChangedDescriptionShortLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 34:
        	if(u->sChangedDescriptionLong != NULL) {
				lua_pushlstring(L, u->sChangedDescriptionLong, u->ui8ChangedDescriptionLongLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 35:
        	if(u->sChangedTagShort != NULL) {
				lua_pushlstring(L, u->sChangedTagShort, u->ui8ChangedTagShortLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 36:
        	if(u->sChangedTagLong != NULL) {
				lua_pushlstring(L, u->sChangedTagLong, u->ui8ChangedTagLongLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 37:
        	if(u->sChangedConnectionShort != NULL) {
				lua_pushlstring(L, u->sChangedConnectionShort, u->ui8ChangedConnectionShortLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 38:
        	if(u->sChangedConnectionLong != NULL) {
				lua_pushlstring(L, u->sChangedConnectionLong, u->ui8ChangedConnectionLongLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 39:
        	if(u->sChangedEmailShort != NULL) {
				lua_pushlstring(L, u->sChangedEmailShort, u->ui8ChangedEmailShortLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 40:
        	if(u->sChangedEmailLong != NULL) {
				lua_pushlstring(L, u->sChangedEmailLong, u->ui8ChangedEmailLongLen);
			} else {
				lua_pushnil(L);
			}
        	return 1;
        case 41:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64ChangedSharedSizeShort);
#else
        	lua_pushunsigned(L, u->ui64ChangedSharedSizeShort);
#endif
        	return 1;
        case 42:
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, (double)u->ui64ChangedSharedSizeLong);
#else
        	lua_pushunsigned(L, u->ui64ChangedSharedSizeLong);
#endif
        	return 1;
        case 43: {
            lua_newtable(L);

            int t = lua_gettop(L);

#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, 1);
#else
            lua_pushunsigned(L, 1);
#endif
            lua_pushlstring(L, u->sIP, u->ui8IpLen);
            lua_rawset(L, t);

            if(u->sIPv4[0] != '\0') {
#if LUA_VERSION_NUM < 503
				lua_pushnumber(L, 2);
#else
                lua_pushunsigned(L, 2);
#endif
                lua_pushlstring(L, u->sIPv4, u->ui8IPv4Len);
                lua_rawset(L, t);
            }

            return 1;
        }
        default:
            luaL_error(L, "bad argument #2 to 'GetUserValue' (it's not valid id)");
            
            lua_pushnil(L);
            return 1;
    }
}
//------------------------------------------------------------------------------

static int Disconnect(lua_State * L) {
    if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'Disconnect' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    User * u;

    if(lua_type(L, 1) == LUA_TTABLE) {
        u = ScriptGetUser(L, 1, "Disconnect");

        if(u == NULL) {
    		lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }
    } else if(lua_type(L, 1) == LUA_TSTRING) {
        size_t szLen;
        char * nick = (char *)lua_tolstring(L, 1, &szLen);
    
        if(szLen == 0) {
            return 0;
        }
    
        u = clsHashManager::mPtr->FindUser(nick, szLen);

        if(u == NULL) {
    		lua_settop(L, 0);
    
            lua_pushnil(L);
            return 1;
        }
    } else {
        luaL_error(L, "bad argument #1 to 'Disconnect' (user table or string expected, got %s)", lua_typename(L, lua_type(L, 1)));
        lua_settop(L, 0);
        
        lua_pushnil(L);
        return 1;
    }

//    int imsgLen = sprintf(g_sBuffer, "[SYS] User %s (%s) disconnected by script.", u->Nick, u->IP);
//    UdpDebug->Broadcast(g_sBuffer, imsgLen);
    u->Close();

    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int Kick(lua_State * L) {
    if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'Kick' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 3, "Kick");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    size_t szKickerLen, szReasonLen;
    char *sKicker = (char *)lua_tolstring(L, 2, &szKickerLen);
    char *sReason = (char *)lua_tolstring(L, 3, &szReasonLen);

    if(szKickerLen == 0 || szKickerLen > 64 || szReasonLen == 0 || szReasonLen > 128000) {
    	lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    clsBanManager::mPtr->TempBan(u, sReason, sKicker, 0, 0, false);

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_BEING_KICKED_BCS], sReason);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Kick5") == true) {
    	u->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
    	if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
    	    imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> *** %s %s IP %s %s %s %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], u->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_LWR], u->sIP, clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED_BY], sKicker,
                clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sReason);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Kick6") == true) {
				clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            }
    	} else {
    	    imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> *** %s %s IP %s %s %s %s: %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], u->sNick,
                clsLanguageManager::mPtr->sTexts[LAN_WITH_LWR], u->sIP, clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED_BY], sKicker, clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], sReason);
            if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Kick7") == true) {
                clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
    	}
    }

    // disconnect the user
    imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] User %s (%s) kicked by script.", u->sNick, u->sIP);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Kick8") == true) {
        clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
    }

    u->Close();
    
    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int Redirect(lua_State * L) {
    if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'Redirect' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 3, "Redirect");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    size_t szAddressLen, szReasonLen;
    char *sAddress = (char *)lua_tolstring(L, 2, &szAddressLen);
    char *sReason = (char *)lua_tolstring(L, 3, &szReasonLen);

    if(szAddressLen == 0 || szAddressLen > 1024 || szReasonLen == 0 || szReasonLen > 128000) {
		lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> %s %s. %s: %s|$ForceMove %s|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_REDIR_TO],
        sAddress, clsLanguageManager::mPtr->sTexts[LAN_MESSAGE], sReason, sAddress);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "Redirect2") == true) {
        u->SendChar(clsServerManager::sGlobalBuffer, imsgLen);
    }

    //int imsgLen = sprintf(g_sBuffer, "[SYS] User %s (%s) redirected by script.", u->Nick, u->IP);
    //clsUdpDebug::mPtr->Broadcast(g_sBuffer, imsgLen);

    u->Close();
    
    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int DefloodWarn(lua_State * L) {
    if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'DefloodWarn' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TTABLE) {
        luaL_checktype(L, 1, LUA_TTABLE);
		lua_settop(L, 0);

        lua_pushnil(L);
        return 1;
    }

    User *u = ScriptGetUser(L, 1, "DefloodWarn");

    if(u == NULL) {
		lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    u->iDefloodWarnings++;

    if(u->iDefloodWarnings >= (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_WARNING_COUNT]) {
        int imsgLen;
        switch(clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_WARNING_ACTION]) {
            case 0:
                break;
            case 1:
				clsBanManager::mPtr->TempBan(u, clsLanguageManager::mPtr->sTexts[LAN_FLOODING], NULL, 0, 0, false);
                break;
            case 2:
                clsBanManager::mPtr->TempBan(u, clsLanguageManager::mPtr->sTexts[LAN_FLOODING], NULL, 
                    clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_TEMP_BAN_TIME], 0, false);
                break;
            case 3: {
                clsBanManager::mPtr->Ban(u, clsLanguageManager::mPtr->sTexts[LAN_FLOODING], NULL, false);
                break;
            }
            default:
                break;
        }

        if(clsSettingManager::mPtr->bBools[SETBOOL_DEFLOOD_REPORT] == true) {
            if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
                imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                    clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_FLOODER], u->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], u->sIP,
                    clsLanguageManager::mPtr->sTexts[LAN_DISCONN_BY_SCRIPT]);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "DefloodWarn1") == true) {
                    clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
                }
            } else {
                imsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_FLOODER], u->sNick,
                    clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], u->sIP, clsLanguageManager::mPtr->sTexts[LAN_DISCONN_BY_SCRIPT]);
                if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "DefloodWarn2") == true) {
                    clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
                }
            }
        }

        imsgLen = sprintf(clsServerManager::sGlobalBuffer, "[SYS] Flood from %s (%s) - user closed by script.", u->sNick, u->sIP);
        if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "DefloodWarn3") == true) {
            clsUdpDebug::mPtr->Broadcast(clsServerManager::sGlobalBuffer, imsgLen);
        }

        u->Close();
    }

    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int SendToAll(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'SendToAll' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    if(lua_gettop(L) == 1) {
        size_t szLen;
        char * sData = (char *)lua_tolstring(L, 1, &szLen);
        if(sData[0] != '\0' && szLen < 128001) {
			if(sData[szLen-1] != '|') {
                memcpy(clsServerManager::sGlobalBuffer, sData, szLen);
                clsServerManager::sGlobalBuffer[szLen] = '|';
                clsServerManager::sGlobalBuffer[szLen+1] = '\0';
				clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, szLen+1, NULL, 0, clsGlobalDataQueue::CMD_LUA);
			} else {
				clsGlobalDataQueue::mPtr->AddQueueItem(sData, szLen, NULL, 0, clsGlobalDataQueue::CMD_LUA);
            }
        }
    }

	lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendToNick(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SendToNick' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szNickLen, szDataLen;
    char *sNick = (char *)lua_tolstring(L, 1, &szNickLen);
    char *sData = (char *)lua_tolstring(L, 2, &szDataLen);

    if(szNickLen == 0 || szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    User *u = clsHashManager::mPtr->FindUser(sNick, szNickLen);
    if(u != NULL) {
        if(sData[szDataLen-1] != '|') {
            memcpy(clsServerManager::sGlobalBuffer, sData, szDataLen);
            clsServerManager::sGlobalBuffer[szDataLen] = '|';
            clsServerManager::sGlobalBuffer[szDataLen+1] = '\0';
            u->SendCharDelayed(clsServerManager::sGlobalBuffer, szDataLen+1);
        } else {
            u->SendCharDelayed(sData, szDataLen);
        }
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendToOpChat(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'SendToOpChat' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szDataLen;
    char * sData = (char *)lua_tolstring(L, 1, &szDataLen);

    if(szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true) {
        int iLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK], clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK], sData);
        if(CheckSprintf(iLen, clsServerManager::szGlobalBufferSize, "SendToOpChat") == true) {
			clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, iLen, NULL, 0, clsGlobalDataQueue::SI_OPCHAT);
        }
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendToOps(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'SendToOps' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen;
    char * sData = (char *)lua_tolstring(L, 1, &szLen);
    
    if(szLen == 0 || szLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    if(sData[szLen-1] != '|') {
        memcpy(clsServerManager::sGlobalBuffer, sData, szLen);
        clsServerManager::sGlobalBuffer[szLen] = '|';
        clsServerManager::sGlobalBuffer[szLen+1] = '\0';
        clsGlobalDataQueue::mPtr->AddQueueItem(clsServerManager::sGlobalBuffer, szLen+1, NULL, 0, clsGlobalDataQueue::CMD_OPS);
    } else {
        clsGlobalDataQueue::mPtr->AddQueueItem(sData, szLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendToProfile(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SendToProfile' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    int32_t i32Profile = (int32_t)lua_tointeger(L, 1);

    size_t szDataLen;
    char * sData = (char *)lua_tolstring(L, 2, &szDataLen);

    if(szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    if(sData[szDataLen-1] != '|') {
        memcpy(clsServerManager::sGlobalBuffer, sData, szDataLen);
		clsServerManager::sGlobalBuffer[szDataLen] = '|';
        clsServerManager::sGlobalBuffer[szDataLen+1] = '\0';
		clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, szDataLen+1, NULL, i32Profile, clsGlobalDataQueue::SI_TOPROFILE);
    } else {
		clsGlobalDataQueue::mPtr->SingleItemStore(sData, szDataLen, NULL, i32Profile, clsGlobalDataQueue::SI_TOPROFILE);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendToUser(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SendToUser' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    User *u = ScriptGetUser(L, 2, "SendToUser");

    if(u == NULL) {
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen;
    char * sData = (char *)lua_tolstring(L, 2, &szLen);

    if(szLen == 0 || szLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    if(sData[szLen-1] != '|') {
        memcpy(clsServerManager::sGlobalBuffer, sData, szLen);
        clsServerManager::sGlobalBuffer[szLen] = '|';
        clsServerManager::sGlobalBuffer[szLen+1] = '\0';
    	u->SendCharDelayed(clsServerManager::sGlobalBuffer, szLen+1);
    } else {
        u->SendCharDelayed(sData, szLen);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendPmToAll(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SendPmToAll' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szFromLen, szDataLen;
    char * sFrom = (char *)lua_tolstring(L, 1, &szFromLen);
    char * sData = (char *)lua_tolstring(L, 2, &szDataLen);

    if(szFromLen == 0 || szFromLen > 64 || szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0); 
        return 0;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", sFrom, sFrom, sData);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "SendPmToAll") == true) {
		clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2ALL);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendPmToNick(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'SendPmToNick' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szToLen, szFromLen, szDataLen;
    char * sTo = (char *)lua_tolstring(L, 1, &szToLen);
    char * sFrom = (char *)lua_tolstring(L, 2, &szFromLen);
    char * sData = (char *)lua_tolstring(L, 3, &szDataLen);

    if(szToLen == 0 || szFromLen == 0 || szFromLen > 64 || szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    User *u = clsHashManager::mPtr->FindUser(sTo, szToLen);
    if(u != NULL) {
        int iMsgLen = sprintf(clsServerManager::sGlobalBuffer, "$To: %s From: %s $<%s> %s|", sTo, sFrom, sFrom, sData);
        if(CheckSprintf(iMsgLen, clsServerManager::szGlobalBufferSize, "SendPmToNick") == true) {
            u->SendCharDelayed(clsServerManager::sGlobalBuffer, iMsgLen);
        }
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendPmToOps(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SendPmToOps' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szFromLen, szDataLen;
    char * sFrom = (char *)lua_tolstring(L, 1, &szFromLen);
    char * sData = (char *)lua_tolstring(L, 2, &szDataLen);

    if(szFromLen == 0 || szFromLen > 64 || szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", sFrom, sFrom, sData);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "SendPmToOps") == true) {
		clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendPmToProfile(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'SendPmToProfile' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    int32_t iProfile = (int32_t)lua_tointeger(L, 1);

    size_t szFromLen, szDataLen;
    char * sFrom = (char *)lua_tolstring(L, 2, &szFromLen);
    char * sData = (char *)lua_tolstring(L, 3, &szDataLen);

    if(szFromLen == 0 || szFromLen > 64 || szDataLen == 0 || szDataLen > 128000) {
        lua_settop(L, 0);
        return 0;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "%s $<%s> %s|", sFrom, sFrom, sData);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "SendPmToProfile") == true) {
		clsGlobalDataQueue::mPtr->SingleItemStore(clsServerManager::sGlobalBuffer, imsgLen, NULL, iProfile, clsGlobalDataQueue::SI_PM2PROFILE);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SendPmToUser(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'SendPmToUser' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    User *u = ScriptGetUser(L, 3, "SendPmToUser");

    if(u == NULL) {
		lua_settop(L, 0);
        return 0;
    }

    size_t szFromLen, szDataLen;
    char * sFrom = (char *)lua_tolstring(L, 2, &szFromLen);
    char * sData = (char *)lua_tolstring(L, 3, &szDataLen);
    
    if(szFromLen == 0 || szFromLen > 64 || szDataLen == 0 || szDataLen > 128000) {
		lua_settop(L, 0);
        return 0;
    }

    int imsgLen = sprintf(clsServerManager::sGlobalBuffer, "$To: %s From: %s $<%s> %s|", u->sNick, sFrom, sFrom, sData);
    if(CheckSprintf(imsgLen, clsServerManager::szGlobalBufferSize, "SendPmToUser") == true) {
        u->SendCharDelayed(clsServerManager::sGlobalBuffer, imsgLen);
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SetUserInfo(lua_State * L) {
	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'SetUserInfo' (4 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TTABLE || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 4) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 4, LUA_TBOOLEAN);
		lua_settop(L, 0);
        return 0;
    }

    User * pUser = ScriptGetUser(L, 4, "SetUserInfo");

    if(pUser == NULL) {
		lua_settop(L, 0);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	uint32_t ui32DataToChange = (uint32_t)lua_tonumber(L, 2);
#else
    uint32_t ui32DataToChange = (uint32_t)lua_tounsigned(L, 2);
#endif

    if(ui32DataToChange > 9) {
		lua_settop(L, 0);
        return 0;
    }

    bool bPermanent = lua_toboolean(L, 4) == 0 ? false : true;

    static const char * sMyInfoPartsNames[] = { "sChangedDescriptionShort", "sChangedDescriptionLong", "sChangedTagShort", "sChangedTagLong",
        "sChangedConnectionShort", "sChangedConnectionLong", "sChangedEmailShort", "sChangedEmailLong" };

    if(lua_type(L, 3) == LUA_TSTRING) {
        if(ui32DataToChange > 7) {
            lua_settop(L, 0);
            return 0;
        }

        size_t szDataLen;
        char * sData = (char *)lua_tolstring(L, 3, &szDataLen);

        if(szDataLen > 64 || strpbrk(sData, "$|") != NULL) {
            lua_settop(L, 0);
            return 0;
        }

        switch(ui32DataToChange) {
            case 0:
                pUser->sChangedDescriptionShort = User::SetUserInfo(pUser->sChangedDescriptionShort, pUser->ui8ChangedDescriptionShortLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_DESCRIPTION_SHORT_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_DESCRIPTION_SHORT_PERM;
                }
                break;
            case 1:
                pUser->sChangedDescriptionLong = User::SetUserInfo(pUser->sChangedDescriptionLong, pUser->ui8ChangedDescriptionLongLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_DESCRIPTION_LONG_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_DESCRIPTION_LONG_PERM;
                }
                break;
            case 2:
                pUser->sChangedTagShort = User::SetUserInfo(pUser->sChangedTagShort, pUser->ui8ChangedTagShortLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_TAG_SHORT_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_TAG_SHORT_PERM;
                }
                break;
            case 3:
                pUser->sChangedTagLong = User::SetUserInfo(pUser->sChangedTagLong, pUser->ui8ChangedTagLongLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_TAG_LONG_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_TAG_LONG_PERM;
                }
                break;
            case 4:
                pUser->sChangedConnectionShort = User::SetUserInfo(pUser->sChangedConnectionShort, pUser->ui8ChangedConnectionShortLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_CONNECTION_SHORT_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_CONNECTION_SHORT_PERM;
                }
                break;
            case 5:
                pUser->sChangedConnectionLong = User::SetUserInfo(pUser->sChangedConnectionLong, pUser->ui8ChangedConnectionLongLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_CONNECTION_LONG_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_CONNECTION_LONG_PERM;
                }
                break;
            case 6:
                pUser->sChangedEmailShort = User::SetUserInfo(pUser->sChangedEmailShort, pUser->ui8ChangedEmailShortLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_EMAIL_SHORT_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_EMAIL_SHORT_PERM;
                }
                break;
            case 7:
                pUser->sChangedEmailLong = User::SetUserInfo(pUser->sChangedEmailLong, pUser->ui8ChangedEmailLongLen, sData, szDataLen, sMyInfoPartsNames[ui32DataToChange]);
                if(bPermanent == true) {
                    pUser->ui32InfoBits |= User::INFOBIT_EMAIL_LONG_PERM;
                } else {
                    pUser->ui32InfoBits &= ~User::INFOBIT_EMAIL_LONG_PERM;
                }
                break;
            default:
                break;
        }
    } else if(lua_type(L, 3) == LUA_TNIL) {
        switch(ui32DataToChange) {
            case 0:
                if(pUser->sChangedDescriptionShort != NULL) {
                    User::FreeInfo(pUser->sChangedDescriptionShort, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedDescriptionShort = NULL;
                    pUser->ui8ChangedDescriptionShortLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_DESCRIPTION_SHORT_PERM;
                break;
            case 1:
                if(pUser->sChangedDescriptionLong != NULL) {
                    User::FreeInfo(pUser->sChangedDescriptionLong, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedDescriptionLong = NULL;
                    pUser->ui8ChangedDescriptionLongLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_DESCRIPTION_LONG_PERM;
                break;
            case 2:
                if(pUser->sChangedTagShort != NULL) {
                    User::FreeInfo(pUser->sChangedTagShort, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedTagShort = NULL;
                    pUser->ui8ChangedTagShortLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_TAG_SHORT_PERM;
                break;
            case 3:
                if(pUser->sChangedTagLong != NULL) {
                    User::FreeInfo(pUser->sChangedTagLong, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedTagLong = NULL;
                    pUser->ui8ChangedTagLongLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_TAG_LONG_PERM;
                break;
            case 4:
                if(pUser->sChangedConnectionShort != NULL) {
                    User::FreeInfo(pUser->sChangedConnectionShort, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedConnectionShort = NULL;
                    pUser->ui8ChangedConnectionShortLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_CONNECTION_SHORT_PERM;
                break;
            case 5:
                if(pUser->sChangedConnectionLong != NULL) {
                    User::FreeInfo(pUser->sChangedConnectionLong, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedConnectionLong = NULL;
                    pUser->ui8ChangedConnectionLongLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_CONNECTION_LONG_PERM;
                break;
            case 6:
                if(pUser->sChangedEmailShort != NULL) {
                    User::FreeInfo(pUser->sChangedEmailShort, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedEmailShort = NULL;
                    pUser->ui8ChangedEmailShortLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_EMAIL_SHORT_PERM;
                break;
            case 7:
                if(pUser->sChangedEmailLong != NULL) {
                    User::FreeInfo(pUser->sChangedEmailLong, sMyInfoPartsNames[ui32DataToChange]);
                    pUser->sChangedEmailLong = NULL;
                    pUser->ui8ChangedEmailLongLen = 0;
                }
                pUser->ui32InfoBits &= ~User::INFOBIT_EMAIL_LONG_PERM;
                break;
            case 8:
                pUser->ui64ChangedSharedSizeShort = pUser->ui64SharedSize;
                pUser->ui32InfoBits &= ~User::INFOBIT_SHARE_SHORT_PERM;
                break;
            case 9:
                pUser->ui64ChangedSharedSizeLong = pUser->ui64SharedSize;
                pUser->ui32InfoBits &= ~User::INFOBIT_SHARE_LONG_PERM;
                break;
            default:
                break;
        }
    } else if(lua_type(L, 3) == LUA_TNUMBER) {
        if(ui32DataToChange < 8) {
            lua_settop(L, 0);
            return 0;
        }

        if(ui32DataToChange == 8) {
#if LUA_VERSION_NUM < 503
			pUser->ui64ChangedSharedSizeShort = (uint64_t)lua_tonumber(L, 3);
#else
            pUser->ui64ChangedSharedSizeShort = lua_tounsigned(L, 3);
#endif
            if(bPermanent == true) {
                pUser->ui32InfoBits |= User::INFOBIT_SHARE_SHORT_PERM;
            } else {
                pUser->ui32InfoBits &= ~User::INFOBIT_SHARE_SHORT_PERM;
            }
        } else if(ui32DataToChange == 9) {
#if LUA_VERSION_NUM < 503
			pUser->ui64ChangedSharedSizeLong = (uint64_t)lua_tonumber(L, 3);
#else
            pUser->ui64ChangedSharedSizeLong = lua_tounsigned(L, 3);
#endif
            if(bPermanent == true) {
                pUser->ui32InfoBits |= User::INFOBIT_SHARE_LONG_PERM;
            } else {
                pUser->ui32InfoBits &= ~User::INFOBIT_SHARE_LONG_PERM;
            }
        }
    } else {
        luaL_error(L, "bad argument #3 to 'SetUserInfo' (string or number or nil expected, got %s)", lua_typename(L, lua_type(L, 3)));
        lua_settop(L, 0);
        return 0;
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static const luaL_Reg core[] = {
	{ "Restart", Restart },
	{ "Shutdown", Shutdown }, 
	{ "ResumeAccepts", ResumeAccepts }, 
	{ "SuspendAccepts", SuspendAccepts }, 
	{ "RegBot", RegBot },  
	{ "UnregBot", UnregBot }, 
	{ "GetBots", GetBots }, 
	{ "GetActualUsersPeak", GetActualUsersPeak }, 
	{ "GetMaxUsersPeak", GetMaxUsersPeak }, 
	{ "GetCurrentSharedSize", GetCurrentSharedSize }, 
	{ "GetHubIP", GetHubIP }, 
	{ "GetHubIPs", GetHubIPs },
	{ "GetHubSecAlias", GetHubSecAlias }, 
	{ "GetPtokaXPath", GetPtokaXPath }, 
	{ "GetUsersCount", GetUsersCount }, 
	{ "GetUpTime", GetUpTime }, 
	{ "GetOnlineNonOps", GetOnlineNonOps }, 
	{ "GetOnlineOps", GetOnlineOps }, 
	{ "GetOnlineRegs", GetOnlineRegs }, 
	{ "GetOnlineUsers", GetOnlineUsers }, 
	{ "GetUser", GetUser }, 
	{ "GetUsers", GetUsers }, 
	{ "GetUserAllData", GetUserAllData }, 
	{ "GetUserData", GetUserData }, 
	{ "GetUserValue", GetUserValue }, 
	{ "Disconnect", Disconnect }, 
	{ "Kick", Kick }, 
	{ "Redirect", Redirect }, 
	{ "DefloodWarn", DefloodWarn }, 
    { "SendToAll", SendToAll }, 
	{ "SendToNick", SendToNick }, 
	{ "SendToOpChat", SendToOpChat }, 
	{ "SendToOps", SendToOps }, 
	{ "SendToProfile", SendToProfile }, 
	{ "SendToUser", SendToUser }, 
	{ "SendPmToAll", SendPmToAll }, 
	{ "SendPmToNick", SendPmToNick }, 
	{ "SendPmToOps", SendPmToOps }, 
	{ "SendPmToProfile", SendPmToProfile }, 
	{ "SendPmToUser", SendPmToUser }, 
	{ "SetUserInfo", SetUserInfo },
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegCore(lua_State * L) {
    luaL_newlib(L, core);
#else
void RegCore(lua_State * L) {
    luaL_register(L, "Core", core);
#endif

    lua_pushliteral(L, "Version");
	lua_pushliteral(L, PtokaXVersionString);
	lua_settable(L, -3);
	lua_pushliteral(L, "BuildNumber");
#ifdef _WIN32
#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)_strtoui64(BUILD_NUMBER, NULL, 10));
#else
	lua_pushunsigned(L, _strtoui64(BUILD_NUMBER, NULL, 10));
#endif
#else
#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)strtoull(BUILD_NUMBER, NULL, 10));
#else
    lua_pushunsigned(L, strtoull(BUILD_NUMBER, NULL, 10));
#endif
#endif
	lua_settable(L, -3);

#if LUA_VERSION_NUM > 501
    return 1;
#endif
}
//---------------------------------------------------------------------------
