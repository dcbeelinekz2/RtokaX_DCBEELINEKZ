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
#include "GlobalDataQueue.h"
#include "hashBanManager.h"
#include "LanguageManager.h"
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
#include "DeFlood.h"
//---------------------------------------------------------------------------
static char msg[1024];
//---------------------------------------------------------------------------

bool DeFloodCheckForFlood(User * u, const uint8_t &ui8DefloodType, const int16_t &ui16Action,
  uint16_t &ui16Count, uint64_t &ui64LastOkTick, 
  const int16_t &ui16DefloodCount, const uint32_t &ui32DefloodTime, char * sOtherNick/* = NULL*/) {
    if(ui16Count == 0) {
		ui64LastOkTick = clsServerManager::ui64ActualTick;
    } else if(ui16Count == ui16DefloodCount) {
		if((ui64LastOkTick+ui32DefloodTime) > clsServerManager::ui64ActualTick) {
            DeFloodDoAction(u, ui8DefloodType, ui16Action, ui16Count, sOtherNick);
            return true;
        } else {
            ui64LastOkTick = clsServerManager::ui64ActualTick;
			ui16Count = 0;
        }
    } else if(ui16Count > ui16DefloodCount) {
        if((ui64LastOkTick+ui32DefloodTime) > clsServerManager::ui64ActualTick) {
            if(ui16Action == 2 && ui16Count == (ui16DefloodCount*2)) {
				u->iDefloodWarnings++;

                if(DeFloodCheckForWarn(u, ui8DefloodType, sOtherNick) == true) {
                    return true;
                }
                ui16Count -= ui16DefloodCount;
            }
            ui16Count++;
            return true;
        } else {
            ui64LastOkTick = clsServerManager::ui64ActualTick;
			ui16Count = 0;
        }
    } else if((ui64LastOkTick+ui32DefloodTime) <= clsServerManager::ui64ActualTick) {
        ui64LastOkTick = clsServerManager::ui64ActualTick;
		ui16Count = 0;
    }

    ui16Count++;
    return false;
}
//---------------------------------------------------------------------------

bool DeFloodCheckForSameFlood(User * u, const uint8_t &ui8DefloodType, const int16_t &ui16Action,
  uint16_t &ui16Count, const uint64_t &ui64LastOkTick,
  const int16_t &ui16DefloodCount, const uint32_t &ui32DefloodTime, 
  char * sNewData, const size_t &ui32NewDataLen, 
  char * sOldData, const uint16_t &ui16OldDataLen, bool &bNewData, char * sOtherNick/* = NULL*/) {
	if((uint32_t)ui16OldDataLen == ui32NewDataLen && (clsServerManager::ui64ActualTick >= ui64LastOkTick &&
      (ui64LastOkTick+ui32DefloodTime) > clsServerManager::ui64ActualTick) &&
	  memcmp(sNewData, sOldData, ui16OldDataLen) == 0) {
		if(ui16Count < ui16DefloodCount) {
			ui16Count++;

            return false;
		} else if(ui16Count == ui16DefloodCount) {
			DeFloodDoAction(u, ui8DefloodType, ui16Action, ui16Count, sOtherNick);
            if(u->ui8State < User::STATE_CLOSING) {
                ui16Count++;
            }

            return true;
		} else {
			if(ui16Action == 2 && ui16Count == (ui16DefloodCount*2)) {
				u->iDefloodWarnings++;

				if(DeFloodCheckForWarn(u, ui8DefloodType, sOtherNick) == true) {
					return true;
                }
                ui16Count -= ui16DefloodCount;
            }
			ui16Count++;

            return true;
        }
	} else {
    	bNewData = true;
        return false;
    }
}
//---------------------------------------------------------------------------

bool DeFloodCheckForDataFlood(User * u, const uint8_t &ui8DefloodType, const int16_t &ui16Action,
	uint32_t &ui32Count, uint64_t &ui64LastOkTick,
	const int16_t &ui16DefloodCount, const uint32_t &ui32DefloodTime) {
	if((uint16_t)(ui32Count/1024) >= ui16DefloodCount) {
		if((ui64LastOkTick+ui32DefloodTime) > clsServerManager::ui64ActualTick) {
        	if((u->ui32BoolBits & User::BIT_RECV_FLOODER) == User::BIT_RECV_FLOODER) {
                return true;
            }
			u->ui32BoolBits |= User::BIT_RECV_FLOODER;
			uint16_t ui16Count = (uint16_t)ui32Count;
			DeFloodDoAction(u, ui8DefloodType, ui16Action, ui16Count, NULL);
            return true;
        } else {
			u->ui32BoolBits &= ~User::BIT_RECV_FLOODER;
            ui64LastOkTick = clsServerManager::ui64ActualTick;
			ui32Count = 0;
            return false;
        }
	} else if((ui64LastOkTick+ui32DefloodTime) <= clsServerManager::ui64ActualTick) {
        u->ui32BoolBits &= ~User::BIT_RECV_FLOODER;
        ui64LastOkTick = clsServerManager::ui64ActualTick;
        ui32Count = 0;
        return false;
	}

	return false;
}
//---------------------------------------------------------------------------

void DeFloodDoAction(User * u, const uint8_t &ui8DefloodType, const int16_t &ui16Action,
    uint16_t &ui16Count, char * sOtherNick) {
    int imsgLen = 0;
    if(sOtherNick != NULL) {
		imsgLen = sprintf(msg, "$To: %s From: %s $", u->sNick, sOtherNick);
		if(CheckSprintf(imsgLen, 1024, "DeFloodDoAction1") == false) {
            return;
        }
    }
    switch(ui16Action) {
        case 1: {
            int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], DeFloodGetMessage(ui8DefloodType, 0));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodDoAction2") == true) {
				u->SendCharDelayed(msg, imsgLen);
            }

            if(ui8DefloodType != DEFLOOD_MAX_DOWN) {
                ui16Count++;
            }
            return;
        }
        case 2:
			u->iDefloodWarnings++;

			if(DeFloodCheckForWarn(u, ui8DefloodType, sOtherNick) == false && ui8DefloodType != DEFLOOD_MAX_DOWN) {
                ui16Count++;
            }

            return;
        case 3: {
            int iret = sprintf(msg+imsgLen, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], DeFloodGetMessage(ui8DefloodType, 0));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodDoAction3") == true) {
				u->SendChar(msg, imsgLen);
            }

			DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_DISCONNECTED]);

			u->Close();
            return;
        }
        case 4: {
			clsBanManager::mPtr->TempBan(u, DeFloodGetMessage(ui8DefloodType, 1), NULL, 0, 0, false);
            int iret = sprintf(msg+imsgLen, "<%s> %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],  clsLanguageManager::mPtr->sTexts[LAN_YOU_BEING_KICKED_BCS],
                DeFloodGetMessage(ui8DefloodType, 1));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodDoAction4") == true) {
				u->SendChar(msg, imsgLen);
            }

            DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED]);

			u->Close();
            return;
        }
        case 5: {
			clsBanManager::mPtr->TempBan(u, DeFloodGetMessage(ui8DefloodType, 1), NULL,
                clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_TEMP_BAN_TIME], 0, false);
            int iret = sprintf(msg+imsgLen, "<%s> %s: %s %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO],
                formatTime(clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_TEMP_BAN_TIME]), clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], DeFloodGetMessage(ui8DefloodType, 1));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodDoAction5") == true) {
				u->SendChar(msg, imsgLen);
            }

            DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_TEMPORARY_BANNED]);

			u->Close();
            return;
        }
        case 6: {
			clsBanManager::mPtr->Ban(u, DeFloodGetMessage(ui8DefloodType, 1), NULL, false);
            int iret = sprintf(msg+imsgLen, "<%s> %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_BEING_BANNED_BECAUSE],
                DeFloodGetMessage(ui8DefloodType, 1));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodDoAction6") == true) {
				u->SendChar(msg, imsgLen);
            }

            DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_BANNED]);

			u->Close();
            return;
        }
    }
}
//---------------------------------------------------------------------------

bool DeFloodCheckForWarn(User * u, const uint8_t &ui8DefloodType, char * sOtherNick) {
	if(u->iDefloodWarnings < (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_WARNING_COUNT]) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], DeFloodGetMessage(ui8DefloodType, 0));
        if(CheckSprintf(imsgLen, 1024, "DeFloodCheckForWarn1") == true) {
			u->SendCharDelayed(msg, imsgLen);
        }
        return false;
    } else {
        int imsgLen = 0;
        if(sOtherNick != NULL) {
			imsgLen = sprintf(msg, "$To: %s From: %s $", u->sNick, sOtherNick);
			if(CheckSprintf(imsgLen, 1024, "DeFloodCheckForWarn2") == false) {
                return true;
            }
        }
        switch(clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_WARNING_ACTION]) {
			case 0: {
                int iret = sprintf(msg+imsgLen, "<%s> %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_BEING_DISCONNECTED_BECAUSE],
                    DeFloodGetMessage(ui8DefloodType, 1));
                imsgLen += iret;
				if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodCheckForWarn3") == true) {
					u->SendChar(msg, imsgLen);
                }

                DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_DISCONNECTED]);

				break;
			}
			case 1: {
				clsBanManager::mPtr->TempBan(u, DeFloodGetMessage(ui8DefloodType, 1), NULL, 0, 0, false);
                int iret = sprintf(msg+imsgLen, "<%s> %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_BEING_KICKED_BCS],
                    DeFloodGetMessage(ui8DefloodType, 1));
				if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodCheckForWarn4") == true) {
					u->SendChar(msg, imsgLen);
                }

                DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_KICKED]);

				break;
			}
			case 2: {
				clsBanManager::mPtr->TempBan(u, DeFloodGetMessage(ui8DefloodType, 1), NULL,
                    clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_TEMP_BAN_TIME], 0, false);
                int iret = sprintf(msg+imsgLen, "<%s> %s: %s %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_HAD_BEEN_TEMP_BANNED_TO],
                    formatTime(clsSettingManager::mPtr->iShorts[SETSHORT_DEFLOOD_TEMP_BAN_TIME]), clsLanguageManager::mPtr->sTexts[LAN_BECAUSE_LWR], DeFloodGetMessage(ui8DefloodType, 1));
				if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodCheckForWarn5") == true) {
					u->SendChar(msg, imsgLen);
                }

                DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_TEMPORARY_BANNED]);

				break;
			}
            case 3: {
				clsBanManager::mPtr->Ban(u, DeFloodGetMessage(ui8DefloodType, 1), NULL, false);
                int iret = sprintf(msg+imsgLen, "<%s> %s: %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_ARE_BEING_BANNED_BECAUSE],
                    DeFloodGetMessage(ui8DefloodType, 1));
                if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodCheckForWarn6") == true) {
					u->SendChar(msg, imsgLen);
                }
                
                DeFloodReport(u, ui8DefloodType, clsLanguageManager::mPtr->sTexts[LAN_WAS_BANNED]);

                break;
            }
        }

        u->Close();
        return true;
    }
}
//---------------------------------------------------------------------------

const char * DeFloodGetMessage(const uint8_t ui8DefloodType, const uint8_t ui8MsgId) {
    switch(ui8DefloodType) {
        case DEFLOOD_GETNICKLIST:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_GetNickList];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_GetNickList_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_GetNickList_FLOODER];
            }
        case DEFLOOD_MYINFO:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_MyINFO];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_MyINFO_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_MyINFO_FLOODER];
            }
        case DEFLOOD_SEARCH:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_SEARCHES];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SEARCH_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SEARCH_FLOODER];
            }
        case DEFLOOD_CHAT:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_CHAT];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_CHAT_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_CHAT_FLOODER];
            }
        case DEFLOOD_PM:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_PM];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_PM_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_PM_FLOODER];
            }
        case DEFLOOD_SAME_SEARCH:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_SAME_SEARCHES];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_SEARCH_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_SEARCH_FLOODER];
            }
        case DEFLOOD_SAME_PM:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_SAME_PM];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_PM_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_PM_FLOODER];
            }
        case DEFLOOD_SAME_CHAT:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_SAME_CHAT];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_CHAT_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_CHAT_FLOODER];
            }
        case DEFLOOD_SAME_MULTI_PM:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_SAME_MULTI_PM];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_MULTI_PM_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_MULTI_PM_FLOODER];
            }
        case DEFLOOD_SAME_MULTI_CHAT:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_SAME_MULTI_CHAT];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_MULTI_CHAT_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SAME_MULTI_CHAT_FLOODER];
            }
        case DEFLOOD_CTM:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_CTM];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_CTM_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_CTM_FLOODER];
            }
        case DEFLOOD_RCTM:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_RCTM];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_RCTM_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_RCTM_FLOODER];
            }
        case DEFLOOD_SR:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_SR];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_SR_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_SR_FLOODER];
            }
        case DEFLOOD_MAX_DOWN:
            switch(ui8MsgId) {
                case 0:
                    return clsLanguageManager::mPtr->sTexts[LAN_PLS_DONT_FLOOD_WITH_DATA];
                case 1:
                    return clsLanguageManager::mPtr->sTexts[LAN_DATA_FLOODING];
                case 2:
                    return clsLanguageManager::mPtr->sTexts[LAN_DATA_FLOODER];
            }
        case INTERVAL_CHAT:
            return clsLanguageManager::mPtr->sTexts[LAN_SECONDS_BEFORE_NEXT_CHAT_MSG];
        case INTERVAL_PM:
            return clsLanguageManager::mPtr->sTexts[LAN_SECONDS_BEFORE_NEXT_PM];
        case INTERVAL_SEARCH:
            return clsLanguageManager::mPtr->sTexts[LAN_SECONDS_BEFORE_NEXT_SEARCH];
	}
	return "";
}
//---------------------------------------------------------------------------

void DeFloodReport(User * u, const uint8_t ui8DefloodType, char *sAction) {
    if(clsSettingManager::mPtr->bBools[SETBOOL_DEFLOOD_REPORT] == true) {
        if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
            int imsgLen = sprintf(msg, "%s $<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                DeFloodGetMessage(ui8DefloodType, 2), u->sNick, clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], u->sIP, sAction);
            if(CheckSprintf(imsgLen, 1024, "DeFloodReport1") == true) {
				clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            }
        } else {
            int imsgLen = sprintf(msg, "<%s> *** %s %s %s %s %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], DeFloodGetMessage(ui8DefloodType, 2), u->sNick,
                clsLanguageManager::mPtr->sTexts[LAN_WITH_IP], u->sIP, sAction);
            if(CheckSprintf(imsgLen, 1024, "DeFloodReport2") == true) {
				clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
        }
    }

	int imsgLen = sprintf(msg, "[SYS] Flood type %hu from %s (%s) - user closed.", (uint16_t)ui8DefloodType, u->sNick, u->sIP);
    if(CheckSprintf(imsgLen, 1024, "DeFloodReport3") == true) {
        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
    }
}
//---------------------------------------------------------------------------

bool DeFloodCheckInterval(User * u, const uint8_t &ui8DefloodType, 
    uint16_t &ui16Count, uint64_t &ui64LastOkTick, 
    const int16_t &ui16DefloodCount, const uint32_t &ui32DefloodTime, char * sOtherNick/* = NULL*/) {
    if(ui16Count == 0) {
		ui64LastOkTick = clsServerManager::ui64ActualTick;
    } else if(ui16Count >= ui16DefloodCount) {
		if((ui64LastOkTick+ui32DefloodTime) > clsServerManager::ui64ActualTick) {
            ui16Count++;

            int imsgLen = 0;
            if(sOtherNick != NULL) {
                imsgLen = sprintf(msg, "$To: %s From: %s $", u->sNick, sOtherNick);
                if(CheckSprintf(imsgLen, 1024, "DeFloodCheckInterval1") == false) {
					return true;
                }
            }

			int iret = sprintf(msg+imsgLen, "<%s> %s %" PRIu64 " %s.|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_PLEASE_WAIT],
                (ui64LastOkTick+ui32DefloodTime)-clsServerManager::ui64ActualTick, DeFloodGetMessage(ui8DefloodType, 0));
			imsgLen += iret;
            if(CheckSprintf1(iret, imsgLen, 1024, "DeFloodCheckInterval2") == true) {
				u->SendCharDelayed(msg, imsgLen);
            }

            return true;
        } else {
            ui64LastOkTick = clsServerManager::ui64ActualTick;
			ui16Count = 0;
        }
    } else if((ui64LastOkTick+ui32DefloodTime) <= clsServerManager::ui64ActualTick) {
        ui64LastOkTick = clsServerManager::ui64ActualTick;
        ui16Count = 0;
    }

    ui16Count++;
    return false;
}
//---------------------------------------------------------------------------
