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
#include "LuaRegManLib.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "GlobalDataQueue.h"
#include "hashRegManager.h"
#include "hashUsrManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ProfileManager.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "User.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------

static void PushReg(lua_State * L, RegUser * r) {
	lua_checkstack(L, 3); // we need 3 (1 table, 2 id, 3 value) empty slots in stack, check it to be sure

    lua_newtable(L);
    int i = lua_gettop(L);

    lua_pushliteral(L, "sNick");
    lua_pushstring(L, r->sNick);
    lua_rawset(L, i);

    lua_pushliteral(L, "sPassword");
    if(r->bPassHash == true) {
        lua_pushnil(L);
    } else {
        lua_pushstring(L, r->sPass);
    }
    lua_rawset(L, i);
            
    lua_pushliteral(L, "iProfile");
#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, r->ui16Profile);
#else
    lua_pushunsigned(L, r->ui16Profile);
#endif
    lua_rawset(L, i);
}
//------------------------------------------------------------------------------

static int Save(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Save' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsRegManager::mPtr->Save();

    return 0;
}
//------------------------------------------------------------------------------

static int GetRegsByProfile(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetRegsByProfile' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TNUMBER);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t iProfile = (uint16_t)lua_tonumber(L, 1);
#else
    uint16_t iProfile = (uint16_t)lua_tounsigned(L, 1);
#endif

    lua_settop(L, 0);

    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

	RegUser *next = clsRegManager::mPtr->RegListS;
        
    while(next != NULL) {
        RegUser *cur = next;
        next = cur->next;
        
		if(cur->ui16Profile == iProfile) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
            PushReg(L, cur);
            lua_rawset(L, t);
        }
    }
    
    return 1;
}
//------------------------------------------------------------------------------

static int GetRegsByOpStatus(lua_State * L, const bool &bOperator) {
	if(lua_gettop(L) != 0) {
        if(bOperator == true) {
            luaL_error(L, "bad argument count to 'GetOps' (0 expected, got %d)", lua_gettop(L));
        } else {
            luaL_error(L, "bad argument count to 'GetNonOps' (0 expected, got %d)", lua_gettop(L));
        }
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

    RegUser *next = clsRegManager::mPtr->RegListS;

    while(next != NULL) {
        RegUser *curReg = next;
		next = curReg->next;

        if(clsProfileManager::mPtr->IsProfileAllowed(curReg->ui16Profile, clsProfileManager::HASKEYICON) == bOperator) {
#if LUA_VERSION_NUM < 503
			lua_pushnumber(L, ++i);
#else
            lua_pushunsigned(L, ++i);
#endif
			PushReg(L, curReg);
            lua_rawset(L, t);
        }
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetNonOps(lua_State * L) {
    return GetRegsByOpStatus(L, false);
}
//------------------------------------------------------------------------------

static int GetOps(lua_State * L) {
    return GetRegsByOpStatus(L, true);
}
//------------------------------------------------------------------------------

static int GetReg(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetReg' (1 expected, got %d)", lua_gettop(L));
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
    char * sNick = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    RegUser * r = clsRegManager::mPtr->Find(sNick, szLen);

    lua_settop(L, 0);

    if(r == NULL) {
		lua_pushnil(L);
        return 1;
    }

    PushReg(L, r);

    return 1;
}
//------------------------------------------------------------------------------

static int GetRegs(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetRegs' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
    
    lua_newtable(L);
    int t = lua_gettop(L), i = 0;

    RegUser *next = clsRegManager::mPtr->RegListS;

    while(next != NULL) {
        RegUser *curReg = next;
		next = curReg->next;

#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, ++i);
#else
        lua_pushunsigned(L, ++i);
#endif

		PushReg(L, curReg); 
    
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int AddReg(lua_State * L) {
	if(lua_gettop(L) == 3) {
        if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TSTRING);
            luaL_checktype(L, 2, LUA_TSTRING);
            luaL_checktype(L, 3, LUA_TNUMBER);
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        size_t szNickLen, szPassLen;
        char *sNick = (char *)lua_tolstring(L, 1, &szNickLen);
        char *sPass = (char *)lua_tolstring(L, 2, &szPassLen);

#if LUA_VERSION_NUM < 503
		uint16_t i16Profile = (uint16_t)lua_tonumber(L, 3);
#else
        uint16_t i16Profile = (uint16_t)lua_tounsigned(L, 3);
#endif

        if(i16Profile > clsProfileManager::mPtr->iProfileCount-1 || szNickLen == 0 || szNickLen > 64 || szPassLen == 0 || szPassLen > 64 || strpbrk(sNick, " $|") != NULL || strchr(sPass, '|') != NULL) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        bool bAdded = clsRegManager::mPtr->AddNew(sNick, sPass, i16Profile);

        lua_settop(L, 0);

        if(bAdded == false) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushboolean(L, 1);
        return 1;
    } else if(lua_gettop(L) == 2) {
        if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TSTRING);
            luaL_checktype(L, 2, LUA_TNUMBER);
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        size_t szNickLen;
        char *sNick = (char *)lua_tolstring(L, 1, &szNickLen);

#if LUA_VERSION_NUM < 503
		uint16_t ui16Profile = (uint16_t)lua_tonumber(L, 2);
#else
        uint16_t ui16Profile = (uint16_t)lua_tounsigned(L, 2);
#endif

        if(ui16Profile > clsProfileManager::mPtr->iProfileCount-1 || szNickLen == 0 || szNickLen > 64 || strpbrk(sNick, " $|") != NULL) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        // check if user is registered
        if(clsRegManager::mPtr->Find(sNick, szNickLen) != NULL) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        User * pUser = clsHashManager::mPtr->FindUser(sNick, szNickLen);
        if(pUser == NULL) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        if(pUser->uLogInOut == NULL) {
            pUser->uLogInOut = new LoginLogout();
            if(pUser->uLogInOut == NULL) {
                pUser->ui32BoolBits |= User::BIT_ERROR;
                pUser->Close();

                AppendDebugLog("%s - [MEM] Cannot allocate new pUser->uLogInOut in RegMan.AddReg\n", 0);
                lua_settop(L, 0);
                lua_pushnil(L);
                return 1;
            }
        }

        pUser->SetBuffer(clsProfileManager::mPtr->ProfilesTable[ui16Profile]->sName);
        pUser->ui32BoolBits |= User::BIT_WAITING_FOR_PASS;

        int iMsgLen = sprintf(clsServerManager::sGlobalBuffer, "<%s> %s.|$GetPass|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_WERE_REGISTERED_PLEASE_ENTER_YOUR_PASSWORD]);
        if(CheckSprintf(iMsgLen, clsServerManager::szGlobalBufferSize, "RegMan.AddReg1") == true) {
            pUser->SendCharDelayed(clsServerManager::sGlobalBuffer, iMsgLen);
        }

        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    } else {
        luaL_error(L, "bad argument count to 'RegMan.AddReg' (2 or 3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }
}
//------------------------------------------------------------------------------

static int DelReg(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'DelReg' (1 expected, got %d)", lua_gettop(L));
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

    size_t szNickLen;
    char * sNick = (char *)lua_tolstring(L, 1, &szNickLen);

    if(szNickLen == 0) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	RegUser *reg = clsRegManager::mPtr->Find(sNick, szNickLen);

    lua_settop(L, 0);

    if(reg == NULL) {
        lua_pushnil(L);
        return 1;
    }

    clsRegManager::mPtr->Delete(reg);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int ChangeReg(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'ChangeReg' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 3) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TNUMBER);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szNickLen, szPassLen = 0;
    char * sNick = (char *)lua_tolstring(L, 1, &szNickLen);
    char * sPass = NULL;

    if(lua_type(L, 2) == LUA_TSTRING) {
        char * sPass = (char *)lua_tolstring(L, 2, &szPassLen);
        if(szPassLen == 0 || szPassLen > 64 || strpbrk(sPass, "|") != NULL) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }
    } else if(lua_type(L, 2) != LUA_TNIL) {
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t i16Profile = (uint16_t)lua_tonumber(L, 3);
#else
	uint16_t i16Profile = (uint16_t)lua_tounsigned(L, 3);
#endif

	if(i16Profile > clsProfileManager::mPtr->iProfileCount-1 || szNickLen == 0 || szNickLen > 64 || strpbrk(sNick, " $|") != NULL) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

	RegUser *reg = clsRegManager::mPtr->Find(sNick, szNickLen);

    if(reg == NULL) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    clsRegManager::mPtr->ChangeReg(reg, sPass, i16Profile);

    lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int ClrRegBadPass(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'ClrRegBadPass' (1 expected, got %d)", lua_gettop(L));
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
    
    size_t szNickLen;
    char * sNick = (char*)lua_tolstring(L, 1, &szNickLen);

    if(szNickLen != 0) {
        RegUser *Reg = clsRegManager::mPtr->Find(sNick, szNickLen);
        if(Reg != NULL) {
            Reg->ui8BadPassCount = 0;
        } else {
			lua_settop(L, 0);
			lua_pushnil(L);
			return 1;
        }
    }

	lua_settop(L, 0);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static const luaL_Reg regman[] = {
    { "Save", Save },
	{ "GetRegsByProfile", GetRegsByProfile }, 
	{ "GetNonOps", GetNonOps }, 
	{ "GetOps", GetOps }, 
	{ "GetReg", GetReg }, 
	{ "GetRegs", GetRegs }, 
	{ "AddReg", AddReg }, 
	{ "DelReg", DelReg }, 
	{ "ChangeReg", ChangeReg }, 
	{ "ClrRegBadPass", ClrRegBadPass }, 
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegRegMan(lua_State * L) {
    luaL_newlib(L, regman);
    return 1;
#else
void RegRegMan(lua_State * L) {
    luaL_register(L, "RegMan", regman);
#endif
}
//---------------------------------------------------------------------------
