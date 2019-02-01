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
#ifndef SetManH
#define SetManH
//---------------------------------------------------------------------------
#include "SettingIds.h"
//---------------------------------------------------------------------------

class clsSettingManager {
private:
#ifdef _WIN32
	CRITICAL_SECTION csSetting;
#else
	pthread_mutex_t mtxSetting;
#endif

    void CreateDefaultMOTD();
    void LoadMOTD();
    void SaveMOTD();
    void CheckMOTD();

    void Load();
public:
    static clsSettingManager * mPtr;

    enum SetPreTxtIds {
        SETPRETXT_HUB_SEC,
        SETPRETXT_MOTD,
        SETPRETXT_HUB_NAME_WLCM,
        SETPRETXT_HUB_NAME,
        SETPRETXT_REDIRECT_ADDRESS,
        SETPRETXT_REG_ONLY_MSG,
        SETPRETXT_SHARE_LIMIT_MSG,
        SETPRETXT_SLOTS_LIMIT_MSG,
        SETPRETXT_HUB_SLOT_RATIO_MSG,
        SETPRETXT_MAX_HUBS_LIMIT_MSG,
        SETPRETXT_NO_TAG_MSG,
        SETPRETXT_TEMP_BAN_REDIR_ADDRESS, 
        SETPRETXT_PERM_BAN_REDIR_ADDRESS, 
        SETPRETXT_NICK_LIMIT_MSG, 
        SETPRETXT_HUB_BOT_MYINFO, 
        SETPRETXT_OP_CHAT_HELLO, 
        SETPRETXT_OP_CHAT_MYINFO, 
        SETPRETXT_IDS_END
    };//SETPRETXT_,

    uint64_t ui64MinShare; //SettingManager->ui64MinShare
    uint64_t ui64MaxShare; //SettingManager->ui64MaxShare

    char * sMOTD;

    char * sPreTexts[SETPRETXT_IDS_END]; //SettingManager->sPreTexts[]
    char * sTexts[SETTXT_IDS_END]; //SettingManager->sTexts[]

    int16_t iShorts[SETSHORT_IDS_END]; //SettingManager->iShorts[]

    uint16_t ui16MOTDLen;
    uint16_t ui16PreTextsLens[SETPRETXT_IDS_END]; //SettingManager->ui16PreTextsLens[]
    uint16_t ui16TextsLens[SETTXT_IDS_END]; //SettingManager->ui16TextsLens[]
    
    uint16_t iPortNumbers[25]; //SettingManager->iPortNumbers[0]

    bool bBools[SETBOOL_IDS_END]; //SettingManager->bBools[]
    
    // PPK ... same nick for bot and opchat bool
	bool bBotsSameNick; //SettingManager->bBotsSameNick
	
	bool bUpdateLocked; //SettingManager->bUpdateLocked

	bool bFirstRun;
	
    uint8_t ui8FullMyINFOOption; //SettingManager->ui8FullMyINFOOption;

    clsSettingManager(void);
    ~clsSettingManager(void);

    bool GetBool(const size_t &szBoolId);
    uint16_t GetFirstPort();
    int16_t GetShort(const size_t &szShortId);
    void GetText(const size_t &szTxtId, char * sMsg);

    void SetBool(const size_t &szBoolId, const bool &bValue); //SettingManager->SetBool()
    void SetMOTD(char * sTxt, const size_t &szLen);
    void SetShort(const size_t &szShortId, const int16_t &iValue);
    void SetText(const size_t &szTxtId, char * sTxt);
    void SetText(const size_t &szTxtId, const char * sTxt);
    void SetText(const size_t &szTxtId, const char * sTxt, const size_t &szLen);
    void SetText(const size_t &szTxtId, const string & sTxt);

    void UpdateBot(const bool &bNickChanged = true);
    void DisableBot(const bool &bNickChanged = true, const bool &bRemoveMyINFO = true);
    void UpdateOpChat(const bool &bNickChanged = true);
    void DisableOpChat(const bool &bNickChanged = true);

    void Save();
    void UpdateAll();

    void UpdateHubSec();
    void UpdateMOTD();
    void UpdateHubNameWelcome();
    void UpdateHubName();
    void UpdateRedirectAddress();
    void UpdateRegOnlyMessage();
    void UpdateShareLimitMessage();
    void UpdateSlotsLimitMessage();
    void UpdateHubSlotRatioMessage();
    void UpdateMaxHubsLimitMessage();
    void UpdateNoTagMessage();
    void UpdateTempBanRedirAddress();
    void UpdatePermBanRedirAddress();
    void UpdateNickLimitMessage();
    void UpdateMinShare();
    void UpdateMaxShare();
    void UpdateTCPPorts();
    void UpdateBotsSameNick();
    void UpdateLanguage();
    void UpdateUDPPort();
    void UpdateScripting() const;
};
//---------------------------------------------------------------------------

#endif
