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
#include "LuaUDPDbgLib.h"
//---------------------------------------------------------------------------
#include "LuaScriptManager.h"
#include "UdpDebug.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "LuaScript.h"
//---------------------------------------------------------------------------

static int Reg(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'Reg' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TSTRING || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TSTRING);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 3, LUA_TBOOLEAN);

		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

	Script * cur = clsScriptManager::mPtr->FindScript(L);
	if(cur == NULL || cur->bRegUDP == true) {
        lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    size_t szLen;
    char * sIP = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen < 7 || szLen > 15 || isIP(sIP) == false) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t usPort = (uint16_t)lua_tonumber(L, 2);
#else
    uint16_t usPort = (uint16_t)lua_tounsigned(L, 2);
#endif

    bool bAllData = lua_toboolean(L, 3) == 0 ? false : true;

    if(clsUdpDebug::mPtr->New(sIP, usPort, bAllData, cur->sName) == false) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

    cur->bRegUDP = true;

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int Unreg(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Unreg' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	Script * cur = clsScriptManager::mPtr->FindScript(L);
	if(cur == NULL || cur->bRegUDP == false) {
        lua_settop(L, 0);
        return 0;
    }

    clsUdpDebug::mPtr->Remove(cur->sName);

    cur->bRegUDP = false;

    return 0;
}
//------------------------------------------------------------------------------

static int Send(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'Send' (1 expected, got %d)", lua_gettop(L));
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
    char * sMsg = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

	Script * cur = clsScriptManager::mPtr->FindScript(L);
	if(cur == NULL || cur->bRegUDP == false) {
        clsUdpDebug::mPtr->Broadcast(sMsg, szLen);
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    }

    clsUdpDebug::mPtr->Send(cur->sName, sMsg, szLen);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static const luaL_Reg udpdbg[] = {
	{ "Reg", Reg }, 
	{ "Unreg", Unreg }, 
	{ "Send", Send }, 
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegUDPDbg(lua_State * L) {
    luaL_newlib(L, udpdbg);
    return 1;
#else
void RegUDPDbg(lua_State * L) {
    luaL_register(L, "UDPDbg", udpdbg);
#endif
}
//---------------------------------------------------------------------------
