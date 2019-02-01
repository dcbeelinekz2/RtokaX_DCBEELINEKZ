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
#include "LuaSetManLib.h"
//---------------------------------------------------------------------------
#include "eventqueue.h"
#include "../core/hashRegManager.h"
#include "hashUsrManager.h"
#include "LuaScriptManager.h"
#include "SettingManager.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------

static int Save(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'Save' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    clsScriptManager::mPtr->SaveScripts();

	clsSettingManager::mPtr->Save();

    return 0;
}
//------------------------------------------------------------------------------

static int GetMOTD(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetMOTD' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 0;
    }

	if(clsSettingManager::mPtr->sMOTD != NULL) {
        lua_pushlstring(L, clsSettingManager::mPtr->sMOTD, (size_t)clsSettingManager::mPtr->ui16MOTDLen);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int SetMOTD(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'SetMOTD' (1 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TSTRING);
		lua_settop(L, 0);
        return 0;
    }

    size_t szLen = 0;
    char * sTxt = (char *)lua_tolstring(L, 1, &szLen);

	clsSettingManager::mPtr->SetMOTD(sTxt, szLen);

	clsSettingManager::mPtr->UpdateMOTD();

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int GetBool(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetBool' (1 expected, got %d)", lua_gettop(L));
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
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    lua_settop(L, 0);

    if(szId >= SETBOOL_IDS_END) {
        luaL_error(L, "bad argument #1 to 'GetBool' (it's not valid id)");
        lua_pushnil(L);
        return 1;
    }  

    clsSettingManager::mPtr->bBools[szId] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);

    return 1;
}
//------------------------------------------------------------------------------

static int SetBool(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SetBool' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        lua_settop(L, 0);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    bool bValue = (lua_toboolean(L, 2) == 0 ? false : true);

    lua_settop(L, 0);

    if(szId >= SETBOOL_IDS_END) {
        luaL_error(L, "bad argument #1 to 'SetBool' (it's not valid id)");
        return 0;
    }  

    if(szId == SETBOOL_ENABLE_SCRIPTING && bValue == false) {
        clsEventQueue::mPtr->AddNormal(clsEventQueue::EVENT_STOP_SCRIPTING, NULL);
        return 0;
    }

    if(szId == SETBOOL_HASH_PASSWORDS && bValue == true) {
        clsRegManager::mPtr->HashPasswords();
    }

    clsSettingManager::mPtr->SetBool(szId, bValue);

    return 0;
}
//------------------------------------------------------------------------------

static int GetNumber(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetNumber' (1 expected, got %d)", lua_gettop(L));
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
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    lua_settop(L, 0);

	if(szId >= (SETSHORT_IDS_END-1)) {
        luaL_error(L, "bad argument #1 to 'GetNumber' (it's not valid id)");
        lua_pushnil(L);
        return 1;
    }  

    lua_pushinteger(L, clsSettingManager::mPtr->iShorts[szId]);

    return 1;
}
//------------------------------------------------------------------------------

static int SetNumber(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SetNumber' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TNUMBER);
        lua_settop(L, 0);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    int16_t iValue = (int16_t)lua_tointeger(L, 2);

    lua_settop(L, 0);

	if(szId >= (SETSHORT_IDS_END-1)) {
        luaL_error(L, "bad argument #1 to 'SetNumber' (it's not valid id)");
        return 0;
    }  

    clsSettingManager::mPtr->SetShort(szId, iValue);

    return 0;
}
//------------------------------------------------------------------------------

static int GetString(lua_State * L) {
	if(lua_gettop(L) != 1) {
        luaL_error(L, "bad argument count to 'GetString' (1 expected, got %d)", lua_gettop(L));
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
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    lua_settop(L, 0);

    if(szId >= SETTXT_IDS_END) {
        luaL_error(L, "bad argument #1 to 'GetString' (it's not valid id)");
        lua_pushnil(L);
        return 1;
    }  

    if(clsSettingManager::mPtr->sTexts[szId] != NULL) {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[szId], (size_t)clsSettingManager::mPtr->ui16TextsLens[szId]);
    } else {
        lua_pushnil(L);
    }

    return 1;
}
//------------------------------------------------------------------------------

static int SetString(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SetString' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TSTRING);
        lua_settop(L, 0);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	size_t szId = (size_t)lua_tonumber(L, 1);
#else
    size_t szId = (size_t)lua_tounsigned(L, 1);
#endif

    if(szId >= SETTXT_IDS_END) {
        luaL_error(L, "bad argument #1 to 'SetString' (it's not valid id)");
        lua_settop(L, 0);
        return 0;
    }  

    size_t szLen;
    char * sValue = (char *)lua_tolstring(L, 2, &szLen);

    clsSettingManager::mPtr->SetText(szId, sValue, szLen);

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int GetMinShare(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetMinShare' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)clsSettingManager::mPtr->ui64MinShare);
#else
    lua_pushunsigned(L, clsSettingManager::mPtr->ui64MinShare);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int SetMinShare(lua_State * L) {
    int n = lua_gettop(L);

    if(n == 2) {
        if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TNUMBER);
            luaL_checktype(L, 2, LUA_TNUMBER);
            lua_settop(L, 0);
            return 0;
        }

		clsSettingManager::mPtr->bUpdateLocked = true;

#if LUA_VERSION_NUM < 503
		clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_LIMIT, (int16_t)lua_tonumber(L, 1));
		clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_UNITS, (int16_t)lua_tonumber(L, 2));
#else
        clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_LIMIT, (int16_t)lua_tounsigned(L, 1));
		clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_UNITS, (int16_t)lua_tounsigned(L, 2));
#endif

		clsSettingManager::mPtr->bUpdateLocked = false;
    } else if(n == 1) {
        if(lua_type(L, 1) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TNUMBER);
            lua_settop(L, 0);
            return 0;
        }

		double dBytes = lua_tonumber(L, 1);
        uint16_t iter = 0;
		for(; dBytes > 1024; iter++) {
			dBytes /= 1024;
		}

		clsSettingManager::mPtr->bUpdateLocked = true;

        clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_LIMIT, (int16_t)dBytes);
		clsSettingManager::mPtr->SetShort(SETSHORT_MIN_SHARE_UNITS, iter);

		clsSettingManager::mPtr->bUpdateLocked = false;
    } else {
        luaL_error(L, "bad argument count to 'SetMinShare' (1 or 2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsSettingManager::mPtr->UpdateMinShare();
	clsSettingManager::mPtr->UpdateShareLimitMessage();

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int GetMaxShare(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetMaxShare' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 0;
    }

#if LUA_VERSION_NUM < 503
	lua_pushnumber(L, (double)clsSettingManager::mPtr->ui64MaxShare);
#else
    lua_pushunsigned(L, clsSettingManager::mPtr->ui64MaxShare);
#endif

    return 1;
}
//------------------------------------------------------------------------------

static int SetMaxShare(lua_State * L) {
    int n = lua_gettop(L);

    if(n == 2) {
        if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TNUMBER);
            luaL_checktype(L, 2, LUA_TNUMBER);
            lua_settop(L, 0);
            return 0;
        }

		clsSettingManager::mPtr->bUpdateLocked = true;

#if LUA_VERSION_NUM < 503
		clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_LIMIT, (int16_t)lua_tonumber(L, 1));
		clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_UNITS, (int16_t)lua_tonumber(L, 2));
#else
        clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_LIMIT, (int16_t)lua_tounsigned(L, 1));
		clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_UNITS, (int16_t)lua_tounsigned(L, 2));
#endif

		clsSettingManager::mPtr->bUpdateLocked = false;
    } else if(n == 1) {
        if(lua_type(L, 1) != LUA_TNUMBER) {
            luaL_checktype(L, 1, LUA_TNUMBER);
            lua_settop(L, 0);
            return 0;
        }

       	double dBytes = (double)lua_tonumber(L, 1);
        uint16_t iter = 0;
		for(; dBytes > 1024; iter++) {
			dBytes /= 1024;
		}

		clsSettingManager::mPtr->bUpdateLocked = true;

		clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_LIMIT, (int16_t)dBytes);
		clsSettingManager::mPtr->SetShort(SETSHORT_MAX_SHARE_UNITS, iter);

		clsSettingManager::mPtr->bUpdateLocked = false;
    } else {
        luaL_error(L, "bad argument count to 'SetMaxShare' (1 or 2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

	clsSettingManager::mPtr->UpdateMaxShare();
	clsSettingManager::mPtr->UpdateShareLimitMessage();

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int SetHubSlotRatio(lua_State * L) {
	if(lua_gettop(L) != 2) {
        luaL_error(L, "bad argument count to 'SetHubSlotRatio' (2 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER) {
        luaL_checktype(L, 1, LUA_TNUMBER);
        luaL_checktype(L, 2, LUA_TNUMBER);
        lua_settop(L, 0);
        return 0;
    }

	clsSettingManager::mPtr->bUpdateLocked = true;

#if LUA_VERSION_NUM < 503
		clsSettingManager::mPtr->SetShort(SETSHORT_HUB_SLOT_RATIO_HUBS, (int16_t)lua_tonumber(L, 1));
		clsSettingManager::mPtr->SetShort(SETSHORT_HUB_SLOT_RATIO_SLOTS, (int16_t)lua_tonumber(L, 2));
#else
    clsSettingManager::mPtr->SetShort(SETSHORT_HUB_SLOT_RATIO_HUBS, (int16_t)lua_tounsigned(L, 1));
	clsSettingManager::mPtr->SetShort(SETSHORT_HUB_SLOT_RATIO_SLOTS, (int16_t)lua_tounsigned(L, 2));
#endif

	clsSettingManager::mPtr->bUpdateLocked = false;

	clsSettingManager::mPtr->UpdateHubSlotRatioMessage();

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int GetOpChat(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetOpChat' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 0;
    }

	lua_newtable(L);
	int i = lua_gettop(L);

	lua_pushliteral(L, "sNick");
    if(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK] == NULL) {
		lua_pushnil(L);
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_OP_CHAT_NICK]);
	}
	lua_rawset(L, i);

	lua_pushliteral(L, "sDescription");
    if(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_DESCRIPTION] == NULL) {
		lua_pushnil(L);
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_DESCRIPTION],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_OP_CHAT_DESCRIPTION]);
	}
	lua_rawset(L, i);

	lua_pushliteral(L, "sEmail");
    if(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_EMAIL] == NULL) {
		lua_pushnil(L);
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_EMAIL],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_OP_CHAT_EMAIL]);
	}
	lua_rawset(L, i);

    lua_pushliteral(L, "bEnabled");
    clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);

    return 1;
}
//------------------------------------------------------------------------------

static int SetOpChat(lua_State * L) {
	if(lua_gettop(L) != 4) {
        luaL_error(L, "bad argument count to 'SetOpChat' (4 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TBOOLEAN || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING || lua_type(L, 4) != LUA_TSTRING) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        lua_settop(L, 0);
        return 0;
    }

    char *NewBotName, *NewBotDescription, *NewBotEmail;
    size_t szNameLen, szDescrLen, szEmailLen;

    NewBotName = (char *)lua_tolstring(L, 2, &szNameLen);
    NewBotDescription = (char *)lua_tolstring(L, 3, &szDescrLen);
    NewBotEmail = (char *)lua_tolstring(L, 4, &szEmailLen);

    if(szNameLen == 0 || szNameLen > 64 || szDescrLen > 64 || szEmailLen > 64 ||
        strpbrk(NewBotName, " $|") != NULL || strpbrk(NewBotDescription, "$|") != NULL ||
		strpbrk(NewBotEmail, "$|") != NULL || clsHashManager::mPtr->FindUser(NewBotName, szNameLen) != NULL) {
        lua_settop(L, 0);
        return 0;
    }

    bool bBotHaveNewNick = (strcmp(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK], NewBotName) != 0);
    bool bEnableBot = (lua_toboolean(L, 1) == 0 ? false : true);

    bool bRegStateChange = false, bDescriptionChange = false, bEmailChange = false;

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] != bEnableBot) {
        bRegStateChange = true;
    }

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true) {
        clsSettingManager::mPtr->DisableOpChat((bBotHaveNewNick == true || bEnableBot == false));
    }

    clsSettingManager::mPtr->bUpdateLocked = true;

    clsSettingManager::mPtr->SetBool(SETBOOL_REG_OP_CHAT, bEnableBot);

    if(bBotHaveNewNick == true) {
        clsSettingManager::mPtr->SetText(SETTXT_OP_CHAT_NICK, NewBotName);
    }

    if(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_DESCRIPTION] == NULL ||
        strcmp(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_DESCRIPTION], NewBotDescription) != 0) {
        if(szDescrLen != (size_t)(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_DESCRIPTION] == NULL ? 0 : -1)) {
            bDescriptionChange = true;
        }

        clsSettingManager::mPtr->SetText(SETTXT_OP_CHAT_DESCRIPTION, NewBotDescription);
    }

    if(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_EMAIL] == NULL ||
        strcmp(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_EMAIL], NewBotEmail) != 0) {
        if(szEmailLen != (size_t)(clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_EMAIL] == NULL ? 0 : -1)) {
            bEmailChange = true;
        }

        clsSettingManager::mPtr->SetText(SETTXT_OP_CHAT_EMAIL, NewBotEmail);
    }

	clsSettingManager::mPtr->bUpdateLocked = false;

    clsSettingManager::mPtr->UpdateBotsSameNick();

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == true &&
        (bRegStateChange == true || bBotHaveNewNick == true || bDescriptionChange == true || bEmailChange == true)) {
        clsSettingManager::mPtr->UpdateOpChat((bBotHaveNewNick == true || bRegStateChange == true));
    }

    lua_settop(L, 0);
    return 0;
}
//------------------------------------------------------------------------------

static int GetHubBot(lua_State * L) {
	if(lua_gettop(L) != 0) {
        luaL_error(L, "bad argument count to 'GetHubBot' (0 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        lua_pushnil(L);
        return 0;
    }

	lua_newtable(L);
	int i = lua_gettop(L);

	lua_pushliteral(L, "sNick");
    if(clsSettingManager::mPtr->sTexts[SETTXT_BOT_NICK] == NULL) {
		lua_pushnil(L);
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_BOT_NICK],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_BOT_NICK]);
	}
	lua_rawset(L, i);

	lua_pushliteral(L, "sDescription");
    if(clsSettingManager::mPtr->sTexts[SETTXT_BOT_DESCRIPTION] == NULL) {
		lua_pushnil(L); 
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_BOT_DESCRIPTION],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_BOT_DESCRIPTION]);
	}
	lua_rawset(L, i);

	lua_pushliteral(L, "sEmail");
    if(clsSettingManager::mPtr->sTexts[SETTXT_BOT_EMAIL] == NULL) {
		lua_pushnil(L); 
	} else {
        lua_pushlstring(L, clsSettingManager::mPtr->sTexts[SETTXT_BOT_EMAIL],
        (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_BOT_EMAIL]);
	}
	lua_rawset(L, i);

    lua_pushliteral(L, "bEnabled");
    clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);

    lua_pushliteral(L, "bUsedAsHubSecAlias");
    clsSettingManager::mPtr->bBools[SETBOOL_USE_BOT_NICK_AS_HUB_SEC] == true ? lua_pushboolean(L, 1) : lua_pushnil(L);
    lua_rawset(L, i);

    return 1;
}
//------------------------------------------------------------------------------

static int SetHubBot(lua_State * L) {
	if(lua_gettop(L) != 5) {
        luaL_error(L, "bad argument count to 'SetHubBot' (5 expected, got %d)", lua_gettop(L));
        lua_settop(L, 0);
        return 0;
    }

    if(lua_type(L, 1) != LUA_TBOOLEAN || lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TSTRING || 
        lua_type(L, 4) != LUA_TSTRING || lua_type(L, 5) != LUA_TBOOLEAN) {
        luaL_checktype(L, 1, LUA_TBOOLEAN);
        luaL_checktype(L, 2, LUA_TSTRING);
        luaL_checktype(L, 3, LUA_TSTRING);
        luaL_checktype(L, 4, LUA_TSTRING);
        luaL_checktype(L, 5, LUA_TBOOLEAN);
        lua_settop(L, 0);
        return 0;
    }

    char *NewBotName, *NewBotDescription, *NewBotEmail;
    size_t szNameLen, szDescrLen, szEmailLen;

    NewBotName = (char *)lua_tolstring(L, 2, &szNameLen);
    NewBotDescription = (char *)lua_tolstring(L, 3, &szDescrLen);
    NewBotEmail = (char *)lua_tolstring(L, 4, &szEmailLen);

    if(szNameLen == 0 || szNameLen > 64 || szDescrLen > 64 || szEmailLen > 64 ||
        strpbrk(NewBotName, " $|") != NULL || strpbrk(NewBotDescription, "$|") != NULL ||
        strpbrk(NewBotEmail, "$|") != NULL || clsHashManager::mPtr->FindUser(NewBotName, szNameLen) != NULL) {
        lua_settop(L, 0);
        return 0;
    }

    bool bBotHaveNewNick = (strcmp(clsSettingManager::mPtr->sTexts[SETTXT_BOT_NICK], NewBotName) != 0);
    bool bEnableBot = (lua_toboolean(L, 1) == 0 ? false : true);

    bool bRegStateChange = false, bDescriptionChange = false, bEmailChange = false;

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] != bEnableBot) {
        bRegStateChange = true;
    }

    clsSettingManager::mPtr->bUpdateLocked = true;

    if(clsSettingManager::mPtr->sTexts[SETTXT_BOT_DESCRIPTION] == NULL ||
        strcmp(clsSettingManager::mPtr->sTexts[SETTXT_BOT_DESCRIPTION], NewBotDescription) != 0) {
        if(szDescrLen != (size_t)(clsSettingManager::mPtr->sTexts[SETTXT_BOT_DESCRIPTION] == NULL ? 0 : -1)) {
            bDescriptionChange = true;
        }

        clsSettingManager::mPtr->SetText(SETTXT_BOT_DESCRIPTION, NewBotDescription);
    }

    if(clsSettingManager::mPtr->sTexts[SETTXT_BOT_EMAIL] == NULL ||
        strcmp(clsSettingManager::mPtr->sTexts[SETTXT_BOT_EMAIL], NewBotEmail) != 0) {
        if(szEmailLen != (size_t)(clsSettingManager::mPtr->sTexts[SETTXT_BOT_EMAIL] == NULL ? 0 : -1)) {
            bEmailChange = true;
        }

        clsSettingManager::mPtr->SetText(SETTXT_BOT_EMAIL, NewBotEmail);
    }

    clsSettingManager::mPtr->SetBool(SETBOOL_USE_BOT_NICK_AS_HUB_SEC, (lua_toboolean(L, 5) == 0 ? false : true));

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true) {
        clsSettingManager::mPtr->bUpdateLocked = false;
        clsSettingManager::mPtr->DisableBot((bBotHaveNewNick == true || bEnableBot == false),
            (bRegStateChange == true || bBotHaveNewNick == true || bDescriptionChange == true || bEmailChange == true) ? true : false);
        clsSettingManager::mPtr->bUpdateLocked = true;
    }

    clsSettingManager::mPtr->SetBool(SETBOOL_REG_BOT, bEnableBot);

    if(bBotHaveNewNick == true) {
        clsSettingManager::mPtr->SetText(SETTXT_BOT_NICK, NewBotName);
    }

    clsSettingManager::mPtr->bUpdateLocked = false;

    clsSettingManager::mPtr->UpdateHubSec();
    clsSettingManager::mPtr->UpdateMOTD();
    clsSettingManager::mPtr->UpdateHubNameWelcome();
    clsSettingManager::mPtr->UpdateRegOnlyMessage();
    clsSettingManager::mPtr->UpdateShareLimitMessage();
    clsSettingManager::mPtr->UpdateSlotsLimitMessage();
    clsSettingManager::mPtr->UpdateHubSlotRatioMessage();
    clsSettingManager::mPtr->UpdateMaxHubsLimitMessage();
    clsSettingManager::mPtr->UpdateNoTagMessage();
    clsSettingManager::mPtr->UpdateNickLimitMessage();
    clsSettingManager::mPtr->UpdateBotsSameNick();

    if(clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true &&
        (bRegStateChange == true || bBotHaveNewNick == true || bDescriptionChange == true || bEmailChange == true)) {
        clsSettingManager::mPtr->UpdateBot((bBotHaveNewNick == true || bRegStateChange == true));
    }

    return 0;
}
//------------------------------------------------------------------------------

static const luaL_Reg setman[] = {
    { "Save", Save },
	{ "GetMOTD", GetMOTD }, 
	{ "SetMOTD", SetMOTD }, 
	{ "GetBool", GetBool }, 
	{ "SetBool", SetBool }, 
	{ "GetNumber", GetNumber }, 
	{ "SetNumber", SetNumber }, 
	{ "GetString", GetString }, 
	{ "SetString", SetString }, 
	{ "GetMinShare", GetMinShare }, 
	{ "SetMinShare", SetMinShare }, 
	{ "GetMaxShare", GetMaxShare }, 
	{ "SetMaxShare", SetMaxShare }, 
	{ "SetHubSlotRatio", SetHubSlotRatio }, 
	{ "GetOpChat", GetOpChat }, 
	{ "SetOpChat", SetOpChat }, 
	{ "GetHubBot", GetHubBot }, 
	{ "SetHubBot", SetHubBot }, 
	{ NULL, NULL }
};
//---------------------------------------------------------------------------

#if LUA_VERSION_NUM > 501
int RegSetMan(lua_State * L) {
    luaL_newlib(L, setman);
    return 1;
#else
void RegSetMan(lua_State * L) {
    luaL_register(L, "SetMan", setman);
#endif
}
//---------------------------------------------------------------------------
