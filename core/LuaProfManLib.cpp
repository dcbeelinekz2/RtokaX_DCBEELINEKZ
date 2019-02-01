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
#include "LuaProfManLib.h"
//---------------------------------------------------------------------------
#include "ProfileManager.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------

static void PushProfilePermissions(lua_State * L, const uint16_t &iProfile) {
    ProfileItem *Prof = clsProfileManager::mPtr->ProfilesTable[iProfile];

    lua_checkstack(L, 3); // we need 3 (1 table, 2 id, 3 value) empty slots in stack, check it to be sure

    lua_newtable(L);
    int t = lua_gettop(L);

    lua_pushliteral(L, "bIsOP");
    Prof->bPermissions[clsProfileManager::HASKEYICON] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodGetNickList");
    Prof->bPermissions[clsProfileManager::NODEFLOODGETNICKLIST] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodNMyINFO");
    Prof->bPermissions[clsProfileManager::NODEFLOODMYINFO] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodSearch");
    Prof->bPermissions[clsProfileManager::NODEFLOODSEARCH] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodPM");
    Prof->bPermissions[clsProfileManager::NODEFLOODPM] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodMainChat");
    Prof->bPermissions[clsProfileManager::NODEFLOODMAINCHAT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bMassMsg");
    Prof->bPermissions[clsProfileManager::MASSMSG] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bTopic");
    Prof->bPermissions[clsProfileManager::TOPIC] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bTempBan");
    Prof->bPermissions[clsProfileManager::TEMP_BAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bTempUnban");
    Prof->bPermissions[clsProfileManager::TEMP_UNBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bRefreshTxt");
    Prof->bPermissions[clsProfileManager::REFRESHTXT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoTagCheck");
    Prof->bPermissions[clsProfileManager::NOTAGCHECK] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bDelRegUser");
    Prof->bPermissions[clsProfileManager::DELREGUSER] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bAddRegUser");
    Prof->bPermissions[clsProfileManager::ADDREGUSER] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoChatLimits");
    Prof->bPermissions[clsProfileManager::NOCHATLIMITS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoMaxHubCheck");
    Prof->bPermissions[clsProfileManager::NOMAXHUBCHECK] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoSlotHubRatio");
    Prof->bPermissions[clsProfileManager::NOSLOTHUBRATIO] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoSlotCheck");
    Prof->bPermissions[clsProfileManager::NOSLOTCHECK] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoShareLimit");
    Prof->bPermissions[clsProfileManager::NOSHARELIMIT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bClrPermBan");
    Prof->bPermissions[clsProfileManager::CLRPERMBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bClrTempBan");
    Prof->bPermissions[clsProfileManager::CLRTEMPBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bGetInfo");
    Prof->bPermissions[clsProfileManager::GETINFO] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bGetBans");
    Prof->bPermissions[clsProfileManager::GETBANLIST] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bRestartScripts");
    Prof->bPermissions[clsProfileManager::RSTSCRIPTS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bRestartHub");
    Prof->bPermissions[clsProfileManager::RSTHUB] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bTempOP");
    Prof->bPermissions[clsProfileManager::TEMPOP] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bGag");
    Prof->bPermissions[clsProfileManager::GAG] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bRedirect");
    Prof->bPermissions[clsProfileManager::REDIRECT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bBan");
    Prof->bPermissions[clsProfileManager::BAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bUnban");
    Prof->bPermissions[clsProfileManager::UNBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bKick");
    Prof->bPermissions[clsProfileManager::KICK] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bDrop");
    Prof->bPermissions[clsProfileManager::DROP] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bEnterFullHub");
    Prof->bPermissions[clsProfileManager::ENTERFULLHUB] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bEnterIfIPBan");
    Prof->bPermissions[clsProfileManager::ENTERIFIPBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bAllowedOPChat");
    Prof->bPermissions[clsProfileManager::ALLOWEDOPCHAT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bSendFullMyinfos");
    Prof->bPermissions[clsProfileManager::SENDFULLMYINFOS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);   
    
    lua_pushliteral(L, "bSendAllUserIP");
    Prof->bPermissions[clsProfileManager::SENDALLUSERIP] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t); 
    
    lua_pushliteral(L, "bRangeBan");
    Prof->bPermissions[clsProfileManager::RANGE_BAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bRangeUnban");
    Prof->bPermissions[clsProfileManager::RANGE_UNBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bRangeTempBan");
    Prof->bPermissions[clsProfileManager::RANGE_TBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bRangeTempUnban");
    Prof->bPermissions[clsProfileManager::RANGE_TUNBAN] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bGetRangeBans");
    Prof->bPermissions[clsProfileManager::GET_RANGE_BANS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bClearRangePermBans");
    Prof->bPermissions[clsProfileManager::CLR_RANGE_BANS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bClearRangeTempBans");
    Prof->bPermissions[clsProfileManager::CLR_RANGE_TBANS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bNoIpCheck");
    Prof->bPermissions[clsProfileManager::NOIPCHECK] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
    
    lua_pushliteral(L, "bClose");
    Prof->bPermissions[clsProfileManager::CLOSE] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoSearchLimits");
    Prof->bPermissions[clsProfileManager::NOSEARCHLIMITS] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodCTM");
    Prof->bPermissions[clsProfileManager::NODEFLOODCTM] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodRCTM");
    Prof->bPermissions[clsProfileManager::NODEFLOODRCTM] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodSR");
    Prof->bPermissions[clsProfileManager::NODEFLOODSR] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoDefloodRecv");
    Prof->bPermissions[clsProfileManager::NODEFLOODRECV] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoChatInterval");
    Prof->bPermissions[clsProfileManager::NOCHATINTERVAL] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoPMInterval");
    Prof->bPermissions[clsProfileManager::NOPMINTERVAL] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoSearchInterval");
    Prof->bPermissions[clsProfileManager::NOSEARCHINTERVAL] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoMaxUsersSameIP");
    Prof->bPermissions[clsProfileManager::NOUSRSAMEIP] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);

    lua_pushliteral(L, "bNoReConnTime");
    Prof->bPermissions[clsProfileManager::NORECONNTIME] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, t);
}
//------------------------------------------------------------------------------

static void PushProfile(lua_State * L, const uint16_t &iProfile) {
	lua_checkstack(L, 3); // we need 3 (1 table, 2 id, 3 value) empty slots in stack, check it to be sure

    lua_newtable(L);
    int i = lua_gettop(L);

    lua_pushliteral(L, "sProfileName");
	lua_pushstring(L, clsProfileManager::mPtr->ProfilesTable[iProfile]->sName);
    lua_rawset(L, i);

    lua_pushliteral(L, "iProfileNumber");
#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, iProfile);
#else
	lua_pushunsigned(L, iProfile);
#endif
    lua_rawset(L, i);

    lua_pushliteral(L, "tProfilePermissions");
	PushProfilePermissions(L, iProfile);
    lua_rawset(L, i);
}
//------------------------------------------------------------------------------

static int AddProfile(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'AddProfile' (1 expected, got %d)", lua_gettop(L));
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
    char * sProfileName = (char *)lua_tolstring(L, 1, &szLen);

    if(szLen == 0 || szLen > 64) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	int32_t idx = clsProfileManager::mPtr->AddProfile(sProfileName);
    if(idx == -1 || idx == -2) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_settop(L, 0);

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, idx);
#else
    lua_pushunsigned(L, idx);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int RemoveProfile(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'RemoveProfile' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) == LUA_TSTRING) {
        size_t szLen;
        char * sProfileName = (char *)lua_tolstring(L, 1, &szLen);

        if(szLen == 0) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        int32_t result = clsProfileManager::mPtr->RemoveProfileByName(sProfileName);
        if(result != 1) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    } else if(lua_type(L, 1) == LUA_TNUMBER) {
#if LUA_VERSION_NUM < 503
        uint16_t idx = (uint16_t)lua_tonumber(L, 1);
#else
    	uint16_t idx = (uint16_t)lua_tounsigned(L, 1);
#endif

    	lua_settop(L, 0);

        // if the requested index is out of bounds return nil
        if(idx >= clsProfileManager::mPtr->iProfileCount) {
            lua_pushnil(L);
            return 1;
        }

        if(clsProfileManager::mPtr->RemoveProfile(idx) == false) {
            lua_settop(L, 0);
            lua_pushnil(L);
            return 1;
        }

        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        return 1;
    }

    luaL_error(L, "bad argument #1 to 'RemoveProfile' (string or number expected, got %d)", lua_typename(L, lua_type(L, 1)));
	lua_settop(L, 0);
	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int MoveDown(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'MoveDown' (1 expected, got %d)", lua_gettop(L));
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
    
    // if the requested index is out of bounds return nil
    if(iProfile >= clsProfileManager::mPtr->iProfileCount-1) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
	}

    clsProfileManager::mPtr->MoveProfileDown(iProfile);

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int MoveUp(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'MoveUp' (1 expected, got %d)", lua_gettop(L));
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
    
    // if the requested index is out of bounds return nil
    if(iProfile == 0 || iProfile >= clsProfileManager::mPtr->iProfileCount) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
	}

    clsProfileManager::mPtr->MoveProfileUp(iProfile);

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int GetProfile(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetProfile' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) == LUA_TSTRING) {
        size_t szLen;
        char *profName = (char *)lua_tolstring(L, 1, &szLen);

        if(szLen == 0) {
            lua_pushnil(L);
            return 1;
        }

		int32_t idx = clsProfileManager::mPtr->GetProfileIndex(profName);

        lua_settop(L, 0);

        if(idx == -1) {
            lua_pushnil(L);
            return 1;
        }

        PushProfile(L, (uint16_t)idx);
        return 1;
    } else if(lua_type(L, 1) == LUA_TNUMBER) {
#if LUA_VERSION_NUM < 503
		uint16_t idx = (uint16_t)lua_tonumber(L, 1);
#else
    	uint16_t idx = (uint16_t)lua_tounsigned(L, 1);
#endif

    	lua_settop(L, 0);
    
        // if the requested index is out of bounds return nil
        if(idx >= clsProfileManager::mPtr->iProfileCount) {
            lua_pushnil(L);
            return 1;
        }
    
        PushProfile(L, idx);
        return 1;
    }

    luaL_error(L, "bad argument #1 to 'GetProfile' (string or number expected, got %d)", lua_typename(L, lua_type(L, 1)));
	lua_settop(L, 0);
	lua_pushnil(L);
    return 1;
}
//------------------------------------------------------------------------------

static int GetProfiles(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetProfiles' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int t = lua_gettop(L);

    for(uint16_t ui16i = 0; ui16i < clsProfileManager::mPtr->iProfileCount; ui16i++) {
#if LUA_VERSION_NUM < 503
		lua_pushnumber(L, (ui16i+1));
#else
        lua_pushunsigned(L, (ui16i+1));
#endif

        PushProfile(L, ui16i);
        lua_rawset(L, t);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int GetProfilePermission(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'GetProfilePermission' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TNUMBER);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t iProfile = (uint16_t)lua_tonumber(L, 1);
	size_t szId = (size_t)lua_tonumber(L, 2);
#else
    uint16_t iProfile = (uint16_t)lua_tounsigned(L, 1);
    size_t szId = (size_t)lua_tounsigned(L, 2);
#endif
    
    lua_settop(L, 0);
    
	if(iProfile >= clsProfileManager::mPtr->iProfileCount) {
        lua_pushnil(L);
        return 1;
    }

	if(szId > clsProfileManager::NORECONNTIME) {
		luaL_error(L, "bad argument #2 to 'GetProfilePermission' (it's not valid id)");
		lua_pushnil(L);
		return 1;
	}

    clsProfileManager::mPtr->ProfilesTable[iProfile]->bPermissions[szId] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);

    return 1;
}
//------------------------------------------------------------------------------

static int GetProfilePermissions(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetProfilePermissions' (1 expected, got %d)", lua_gettop(L));
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
    
    // if the requested index is out of bounds return nil
    if(iProfile >= clsProfileManager::mPtr->iProfileCount) {
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
	}

    PushProfilePermissions(L, iProfile);

    return 1;
}
//------------------------------------------------------------------------------

static int SetProfileName(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SetProfileName' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TSTRING);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t iProfile = (uint16_t)lua_tonumber(L, 1);
#else
    uint16_t iProfile = (uint16_t)lua_tounsigned(L, 1);
#endif

    if(iProfile >= clsProfileManager::mPtr->iProfileCount) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    size_t szLen;
	char * sName = (char *)lua_tolstring(L, 2, &szLen);

    if(szLen == 0 || szLen > 64) {
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

	clsProfileManager::mPtr->ChangeProfileName(iProfile, sName, szLen);

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int SetProfilePermission(lua_State * L) {
	if(lua_gettop(L) != 3) {
        luaL_error(L, "bad argument count to 'SetProfilePermission' (3 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 1;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TNUMBER);
        luaL_checktype(L, 3, LUA_TBOOLEAN);
		lua_settop(L, 0);
		lua_pushnil(L);
        return 1;
    }

#if LUA_VERSION_NUM < 503
	uint16_t iProfile = (uint16_t)lua_tonumber(L, 1);
	size_t szId = (size_t)lua_tonumber(L, 2);
#else
    uint16_t iProfile = (uint16_t)lua_tounsigned(L, 1);
    size_t szId = (size_t)lua_tounsigned(L, 2);
#endif

    bool bValue = lua_toboolean(L, 3) == 0 ? false : true;
    
    lua_settop(L, 0);
    
	if(iProfile >= clsProfileManager::mPtr->iProfileCount) {
		lua_pushnil(L);
		return 1;
	}

	if(szId > clsProfileManager::NORECONNTIME) {
		luaL_error(L, "bad argument #2 to 'SetProfilePermission' (it's not valid id)");
		lua_pushnil(L);
		return 1;
	}

    clsProfileManager::mPtr->ChangeProfilePermission(iProfile, szId, bValue);

    lua_pushboolean(L, 1);
    return 1;
}
//------------------------------------------------------------------------------

static int Save(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Save' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsProfileManager::mPtr->SaveProfiles();

    return 0;
}
//------------------------------------------------------------------------------

static const luaL_Reg profman[] = {
	{ "AddProfile", AddProfile },
	{ "RemoveProfile", RemoveProfile }, 
	{ "MoveDown", MoveDown }, 
	{ "MoveUp", MoveUp }, 
	{ "GetProfile", GetProfile }, 
	{ "GetProfiles", GetProfiles }, 
	{ "GetProfilePermission", GetProfilePermission }, 
	{ "GetProfilePermissions", GetProfilePermissions }, 
	{ "SetProfileName", SetProfileName }, 
	{ "SetProfilePermission", SetProfilePermission }, 
	{ "Save", Save },
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegProfMan(lua_State * L) {
    luaL_newlib(L, profman);
    return 1;
#else
void RegProfMan(lua_State * L) {
    luaL_register(L, "ProfMan", profman);
#endif
}
//---------------------------------------------------------------------------
