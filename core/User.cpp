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
#include "User.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "DcCommands.h"
#include "GlobalDataQueue.h"
#include "hashUsrManager.h"
#include "LanguageManager.h"
#include "LuaScriptManager.h"
#include "ProfileManager.h"
#include "ServerManager.h"
#include "SettingManager.h"
#include "utility.h"
#include "UdpDebug.h"
#include "ZlibUtility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#include "DeFlood.h"
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
	#include "../gui.win/GuiUtil.h"
    #include "../gui.win/MainWindowPageUsersChat.h"
#endif
//---------------------------------------------------------------------------
static const size_t ZMINDATALEN = 128;
static const char * sBadTag = "BAD TAG!"; // 8
static const char * sOtherNoTag = "OTHER (NO TAG)"; // 14
static const char * sUnknownTag = "UNKNOWN TAG"; // 11
static const char * sDefaultNick = "<unknown>"; // 9
//---------------------------------------------------------------------------
static char msg[1024];
//---------------------------------------------------------------------------

static bool UserProcessLines(User * u, const uint32_t &iStrtLen) {
	// nothing to process?
	if(u->recvbuf[0] == '\0')
        return false;

    char c;
    
    char *buffer = u->recvbuf;

    for(uint32_t ui32i = iStrtLen; ui32i < u->rbdatalen; ui32i++) {
        if(u->recvbuf[ui32i] == '|') {
            // look for pipes in the data - process lines one by one
            c = u->recvbuf[ui32i+1];
            u->recvbuf[ui32i+1] = '\0';
            uint32_t ui32iCommandLen = (uint32_t)(((u->recvbuf+ui32i)-buffer)+1);
            if(buffer[0] == '|') {
                //UdpDebug->Broadcast("[SYS] heartbeat from " + string(u->Nick, u->NickLen));
                //send(Sck, "|", 1, 0);
            } else if(ui32iCommandLen < (u->ui8State < User::STATE_ADDME ? 1024U : 65536U)) {
        		clsDcCommands::mPtr->PreProcessData(u, buffer, true, ui32iCommandLen);
        	} else {
                int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_CMD_TOO_LONG]);
                if(CheckSprintf(imsgLen, 1024, "UserProcessLines1") == true) {
                    u->SendCharDelayed(msg, imsgLen);
                }
				u->Close();
				clsUdpDebug::mPtr->Broadcast("[SYS] " + string(u->sNick, u->ui8NickLen) + " (" + string(u->sIP, u->ui8IpLen) + ") Received command too long. User disconnected.");
                return false;
            }
        	u->recvbuf[ui32i+1] = c;
        	buffer += ui32iCommandLen;
            if(u->ui8State >= User::STATE_CLOSING) {
                return true;
            }
        } else if(u->recvbuf[ui32i] == '\0') {
            // look for NULL character and replace with zero
            u->recvbuf[ui32i] = '0';
            continue;
        }
	}

	u->rbdatalen -= (uint32_t)(buffer-u->recvbuf);

	if(u->rbdatalen == 0) {
        clsDcCommands::mPtr->ProcessCmds(u);
        u->recvbuf[0] = '\0';
        return false;
    } else if(u->rbdatalen != 1) {
        if(u->rbdatalen > unsigned (u->ui8State < User::STATE_ADDME ? 1024 : 65536)) {
            // PPK ... we don't want commands longer than 64 kB, drop this user !
            int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_CMD_TOO_LONG]);
            if(CheckSprintf(imsgLen, 1024, "UserProcessLines2") == true) {
                u->SendCharDelayed(msg, imsgLen);
            }
            u->Close();
			clsUdpDebug::mPtr->Broadcast("[SYS] " + string(u->sNick, u->ui8NickLen) + " (" + string(u->sIP, u->ui8IpLen) +") RecvBuffer overflow. User disconnected.");
        	return false;
        }
        clsDcCommands::mPtr->ProcessCmds(u);
        memmove(u->recvbuf, buffer, u->rbdatalen);
        u->recvbuf[u->rbdatalen] = '\0';
        return true;
    } else {
        clsDcCommands::mPtr->ProcessCmds(u);
        if(buffer[0] == '|') {
            u->recvbuf[0] = '\0';
            u->rbdatalen = 0;
            return false;
        } else {
            u->recvbuf[0] = buffer[0];
            u->recvbuf[1] = '\0';
            return true;
        }
    }
}
//------------------------------------------------------------------------------

static void UserSetBadTag(User * u, char * Descr, uint8_t DescrLen) {
    // PPK ... clear all tag related things
    u->sTagVersion = NULL;
    u->ui8TagVersionLen = 0;

    u->sModes[0] = '\0';
    u->Hubs = u->Slots = u->OLimit = u->LLimit = u->DLimit = u->iNormalHubs = u->iRegHubs = u->iOpHubs = 0;
    u->ui32BoolBits |= User::BIT_OLDHUBSTAG;
    u->ui32BoolBits |= User::BIT_HAVE_BADTAG;
    
    u->sDescription = Descr;
    u->ui8DescriptionLen = (uint8_t)DescrLen;

    // PPK ... clear (fake) tag
    u->sTag = NULL;
    u->ui8TagLen = 0;

    // PPK ... set bad tag
    u->sClient = (char *)sBadTag;
    u->ui8ClientLen = 8;

    // PPK ... send report to udp debug
    int imsgLen = sprintf(msg, "[SYS] User %s (%s) have bad TAG (%s) ?!?.", u->sNick, u->sIP, u->sMyInfoOriginal);
    if(CheckSprintf(imsgLen, 1024, "SetBadTag") == true) {
        clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
    }
}
//---------------------------------------------------------------------------

static void UserParseMyInfo(User * u) {
    memcpy(msg, u->sMyInfoOriginal, u->ui16MyInfoOriginalLen);

    char *sMyINFOParts[] = { NULL, NULL, NULL, NULL, NULL };
    uint16_t iMyINFOPartsLen[] = { 0, 0, 0, 0, 0 };

    unsigned char cPart = 0;

    sMyINFOParts[cPart] = msg+14+u->ui8NickLen; // desription start


    for(uint32_t ui32i = 14+u->ui8NickLen; ui32i < u->ui16MyInfoOriginalLen-1u; ui32i++) {
        if(msg[ui32i] == '$') {
            msg[ui32i] = '\0';
            iMyINFOPartsLen[cPart] = (uint16_t)((msg+ui32i)-sMyINFOParts[cPart]);

            // are we on end of myinfo ???
            if(cPart == 4)
                break;

            cPart++;
            sMyINFOParts[cPart] = msg+ui32i+1;
        }
    }

    // check if we have all myinfo parts, connection and sharesize must have length more than 0 !
    if(sMyINFOParts[0] == NULL || sMyINFOParts[1] == NULL || iMyINFOPartsLen[1] != 1 || sMyINFOParts[2] == NULL || iMyINFOPartsLen[2] == 0 ||
        sMyINFOParts[3] == NULL || sMyINFOParts[4] == NULL || iMyINFOPartsLen[4] == 0) {
        int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_YOU_MyINFO_IS_CORRUPTED]);
        if(CheckSprintf(imsgLen, 1024, "UserParseMyInfo1") == true) {
            u->SendChar(msg, imsgLen);
        }
        imsgLen = sprintf(msg, "[SYS] User %s (%s) with bad MyINFO (%s) disconnected.", u->sNick, u->sIP, u->sMyInfoOriginal);
        if(CheckSprintf(imsgLen, 1024, "UserParseMyInfo2") == true) {
            clsUdpDebug::mPtr->Broadcast(msg, imsgLen);
        }
        u->Close();
        return;
    }

    // connection
    u->MagicByte = sMyINFOParts[2][iMyINFOPartsLen[2]-1];
    u->sConnection = u->sMyInfoOriginal+(sMyINFOParts[2]-msg);
    u->ui8ConnectionLen = (uint8_t)(iMyINFOPartsLen[2]-1);

    // email
    if(iMyINFOPartsLen[3] != 0) {
        u->sEmail = u->sMyInfoOriginal+(sMyINFOParts[3]-msg);
        u->ui8EmailLen = (uint8_t)iMyINFOPartsLen[3];
    }
    
    // share
    // PPK ... check for valid numeric share, kill fakers !
    if(HaveOnlyNumbers(sMyINFOParts[4], iMyINFOPartsLen[4]) == false) {
        //clsUdpDebug::mPtr->Broadcast("[SYS] User " + string(u->Nick, u->NickLen) + " (" + string(u->IP, u->ui8IpLen) + ") with non-numeric sharesize disconnected.");
        u->Close();
        return;
    }
            
    if(((u->ui32BoolBits & User::BIT_HAVE_SHARECOUNTED) == User::BIT_HAVE_SHARECOUNTED) == true) {
        clsServerManager::ui64TotalShare -= u->ui64SharedSize;
#ifdef _WIN32
        u->ui64SharedSize = _strtoui64(sMyINFOParts[4], NULL, 10);
#else
		u->ui64SharedSize = strtoull(sMyINFOParts[4], NULL, 10);
#endif
        clsServerManager::ui64TotalShare += u->ui64SharedSize;
    } else {
#ifdef _WIN32
        u->ui64SharedSize = _strtoui64(sMyINFOParts[4], NULL, 10);
#else
		u->ui64SharedSize = strtoull(sMyINFOParts[4], NULL, 10);
#endif
    }

    // Reset all tag infos...
    u->sModes[0] = '\0';
    u->Hubs = 0;
    u->iNormalHubs = 0;
    u->iRegHubs = 0;
    u->iOpHubs =0;
    u->Slots = 0;
    u->OLimit = 0;
    u->LLimit = 0;
    u->DLimit = 0;
    
    // description
    if(iMyINFOPartsLen[0] != 0) {
        if(sMyINFOParts[0][iMyINFOPartsLen[0]-1] == '>') {
            char *DCTag = strrchr(sMyINFOParts[0], '<');
            if(DCTag == NULL) {               
                u->sDescription = u->sMyInfoOriginal+(sMyINFOParts[0]-msg);
                u->ui8DescriptionLen = (uint8_t)iMyINFOPartsLen[0];

                u->sClient = (char*)sOtherNoTag;
                u->ui8ClientLen = 14;
                return;
            }

            u->sTag = u->sMyInfoOriginal+(DCTag-msg);
            u->ui8TagLen = (uint8_t)(iMyINFOPartsLen[0]-(DCTag-sMyINFOParts[0]));

            static const uint16_t ui16plusplus = *((uint16_t *)"++");
            if(DCTag[3] == ' ' && *((uint16_t *)(DCTag+1)) == ui16plusplus) {
                u->ui32SupportBits |= User::SUPPORTBIT_NOHELLO;
            }

            static const uint16_t ui16V = *((uint16_t *)"V:");

            char * sTemp = strchr(DCTag, ' ');

            if(sTemp != NULL && *((uint16_t *)(sTemp+1)) == ui16V) {
                sTemp[0] = '\0';
                u->sClient = u->sMyInfoOriginal+((DCTag+1)-msg);
                u->ui8ClientLen = (uint8_t)((sTemp-DCTag)-1);
            } else {
                u->sClient = (char *)sUnknownTag;
                u->ui8ClientLen = 11;
                u->sTag = NULL;
                u->ui8TagLen = 0;
                sMyINFOParts[0][iMyINFOPartsLen[0]-1] = '>'; // not valid DC Tag, add back > tag ending
                u->sDescription = u->sMyInfoOriginal+(sMyINFOParts[0]-msg);
                u->ui8DescriptionLen = (uint8_t)iMyINFOPartsLen[0];
                return;
            }

            size_t szTagPattLen = ((sTemp-DCTag)+1);

            sMyINFOParts[0][iMyINFOPartsLen[0]-1] = ','; // terminate tag end with ',' for easy tag parsing

            uint32_t reqVals = 0;
            char * sTagPart = DCTag+szTagPattLen;

            for(size_t szi = szTagPattLen; szi < (size_t)(iMyINFOPartsLen[0]-(DCTag-sMyINFOParts[0])); szi++) {
                if(DCTag[szi] == ',') {
                    DCTag[szi] = '\0';
                    if(sTagPart[1] != ':') {
                        UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                        return;
                    }

                    switch(sTagPart[0]) {
                        case 'V':
                            // PPK ... fix for potencial memory leak with fake tag
                            if(sTagPart[2] == '\0' || u->sTagVersion) {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->sTagVersion = u->sMyInfoOriginal+((sTagPart+2)-msg);
                            u->ui8TagVersionLen = (uint8_t)((DCTag+szi)-(sTagPart+2));
                            reqVals++;
                            break;
                        case 'M':
                            if((u->ui32BoolBits & User::BIT_IPV6) == User::BIT_IPV6 && (u->ui32SupportBits & User::SUPPORTBIT_IP64) == User::SUPPORTBIT_IP64) {
                                if(sTagPart[2] == '\0' || sTagPart[3] == '\0' || sTagPart[4] != '\0') {
                                    UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                    return;
                                }
                                u->sModes[0] = sTagPart[2];
                                u->sModes[1] = sTagPart[3];
                                u->sModes[2] = '\0';

                                if(toupper(sTagPart[3]) == 'A') {
                                    u->ui32BoolBits |= User::BIT_IPV6_ACTIVE;
                                } else {
                                    u->ui32BoolBits &= ~User::BIT_IPV6_ACTIVE;
                                }
                            } else {
                                if(sTagPart[2] == '\0' || sTagPart[3] != '\0') {
                                    UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                    return;
                                }
                                u->sModes[0] = sTagPart[2];
                                u->sModes[1] = '\0';
                            }

                            if(toupper(sTagPart[2]) == 'A') {
                                u->ui32BoolBits |= User::BIT_IPV4_ACTIVE;
                            } else {
                                u->ui32BoolBits &= ~User::BIT_IPV4_ACTIVE;
                            }

                            reqVals++;
                            break;
                        case 'H': {
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }

                            DCTag[szi] = '/';

                            char *sHubsParts[] = { NULL, NULL, NULL };
                            uint16_t iHubsPartsLen[] = { 0, 0, 0 };

                            uint8_t cPart = 0;

                            sHubsParts[cPart] = sTagPart+2;


                            for(uint32_t ui32j = 3; ui32j < (uint32_t)((DCTag+szi+1)-sTagPart); ui32j++) {
                                if(sTagPart[ui32j] == '/') {
                                    sTagPart[ui32j] = '\0';
                                    iHubsPartsLen[cPart] = (uint16_t)((sTagPart+ui32j)-sHubsParts[cPart]);

                                    // are we on end of hubs tag part ???
                                    if(cPart == 2)
                                        break;

                                    cPart++;
                                    sHubsParts[cPart] = sTagPart+ui32j+1;
                                }
                            }

                            if(sHubsParts[0] != NULL && sHubsParts[1] != NULL && sHubsParts[2] != NULL) {
                                if(iHubsPartsLen[0] != 0 && iHubsPartsLen[1] != 0 && iHubsPartsLen[2] != 0) {
                                    if(HaveOnlyNumbers(sHubsParts[0], iHubsPartsLen[0]) == false ||
                                        HaveOnlyNumbers(sHubsParts[1], iHubsPartsLen[1]) == false ||
                                        HaveOnlyNumbers(sHubsParts[2], iHubsPartsLen[2]) == false) {
                                        UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                        return;
                                    }
                                    u->iNormalHubs = atoi(sHubsParts[0]);
                                    u->iRegHubs = atoi(sHubsParts[1]);
                                    u->iOpHubs = atoi(sHubsParts[2]);
                                    u->Hubs = u->iNormalHubs+u->iRegHubs+u->iOpHubs;
                                    // PPK ... kill LAM3R with fake hubs
                                    if(u->Hubs != 0) {
                                        u->ui32BoolBits &= ~User::BIT_OLDHUBSTAG;
                                        reqVals++;
                                        break;
                                    }
                                }
                            } else if(sHubsParts[1] == DCTag+szi+1 && sHubsParts[2] == NULL) {
                                DCTag[szi] = '\0';
                                u->Hubs = atoi(sHubsParts[0]);
                                reqVals++;
                                u->ui32BoolBits |= User::BIT_OLDHUBSTAG;
                                break;
                            }
                            int imsgLen = sprintf(msg, "<%s> %s!|", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsLanguageManager::mPtr->sTexts[LAN_FAKE_TAG]);
                            if(CheckSprintf(imsgLen, 1024, "UserParseMyInfo3") == true) {
                                u->SendChar(msg, imsgLen);
                            }
                            imsgLen = sprintf(msg, "[SYS] User %s (%s) with fake Tag disconnected: ", u->sNick, u->sIP);
                            if(CheckSprintf(imsgLen, 1024, "UserParseMyInfo4") == true) {
                                memcpy(msg+imsgLen, u->sTag, u->ui8TagLen);
                                clsUdpDebug::mPtr->Broadcast(msg, imsgLen+(int)u->ui8TagLen);
                            }
                            u->Close();
                            return;
                        }
                        case 'S':
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            if(HaveOnlyNumbers(sTagPart+2, (uint16_t)strlen(sTagPart+2)) == false) {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->Slots = atoi(sTagPart+2);
                            reqVals++;
                            break;
                        case 'O':
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->OLimit = atoi(sTagPart+2);
                            break;
                        case 'B':
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->LLimit = atoi(sTagPart+2);
                            break;
                        case 'L':
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->LLimit = atoi(sTagPart+2);
                            break;
                        case 'D':
                            if(sTagPart[2] == '\0') {
                                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                                return;
                            }
                            u->DLimit = atoi(sTagPart+2);
                            break;
                        default:
                            //clsUdpDebug::mPtr->Broadcast("[SYS] "+string(u->Nick, u->NickLen)+": Extra info in DC tag: "+string(sTag));
                            break;
                    }
                    sTagPart = DCTag+szi+1;
                }
            }
                
            if(reqVals < 4) {
                UserSetBadTag(u, u->sMyInfoOriginal+(sMyINFOParts[0]-msg), (uint8_t)iMyINFOPartsLen[0]);
                return;
            } else {
                u->sDescription = u->sMyInfoOriginal+(sMyINFOParts[0]-msg);
                u->ui8DescriptionLen = (uint8_t)(DCTag-sMyINFOParts[0]);
                return;
            }
        } else {
            u->sDescription = u->sMyInfoOriginal+(sMyINFOParts[0]-msg);
            u->ui8DescriptionLen = (uint8_t)iMyINFOPartsLen[0];
        }
    }

    u->sClient = (char *)sOtherNoTag;
    u->ui8ClientLen = 14;

    u->sTag = NULL;
    u->ui8TagLen = 0;

    u->sTagVersion = NULL;
    u->ui8TagVersionLen = 0;

    u->sModes[0] = '\0';
    u->Hubs = 0;
    u->iNormalHubs = 0;
    u->iRegHubs = 0;
    u->iOpHubs =0;
    u->Slots = 0;
    u->OLimit = 0;
    u->LLimit = 0;
    u->DLimit = 0;
}
//---------------------------------------------------------------------------

UserBan::UserBan() {
    sMessage = NULL;

    ui32Len = 0;
    ui32NickHash = 0;
}
//---------------------------------------------------------------------------

UserBan::~UserBan() {
#ifdef _WIN32
    if(sMessage != NULL && HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sMessage) == 0) {
        AppendDebugLog("%s - [MEM] Cannot deallocate sMessage in UserBan::~UserBan\n", 0);
    }
#else
	free(sMessage);
#endif
    sMessage = NULL;
}
//---------------------------------------------------------------------------

UserBan * UserBan::CreateUserBan(char * sMess, const uint32_t &iMessLen, const uint32_t &ui32Hash) {
    UserBan * pUserBan = new UserBan();

    if(pUserBan == NULL) {
        AppendDebugLog("%s - [MEM] Cannot allocate new pUserBan in UserBan::CreateUserBan\n", 0);

        return NULL;
    }

#ifdef _WIN32
    pUserBan->sMessage = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, iMessLen+1);
#else
	pUserBan->sMessage = (char *)malloc(iMessLen+1);
#endif
    if(pUserBan->sMessage == NULL) {
        AppendDebugLog("%s - [MEM] UserBan::CreateUserBan cannot allocate %" PRIu64 " bytes for sMessage\n", (uint64_t)(iMessLen+1));

        delete pUserBan;
        return NULL;
    }

    memcpy(pUserBan->sMessage, sMess, iMessLen);
    pUserBan->sMessage[iMessLen] = '\0';

    pUserBan->ui32Len = iMessLen;
    pUserBan->ui32NickHash = ui32Hash;

    return pUserBan;
}
//---------------------------------------------------------------------------

LoginLogout::LoginLogout() {
    uBan = NULL;

    pBuffer = NULL;

    iToCloseLoops = 0;
    iUserConnectedLen = 0;

    logonClk = 0;
    ui64IPv4CheckTick = 0;
}
//---------------------------------------------------------------------------

LoginLogout::~LoginLogout() {
    delete uBan;

#ifdef _WIN32
    if(pBuffer != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pBuffer) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate pBuffer in LoginLogout::~LoginLogout\n", 0);
        }
    }
#else
	free(pBuffer);
#endif
}
//---------------------------------------------------------------------------

User::User() {
	iSendCalled = 0;
	iRecvCalled = 0;

#ifdef _WIN32
	Sck = INVALID_SOCKET;
#else
	Sck = -1;
#endif

	prev = NULL;
	next = NULL;
	hashtableprev = NULL;
	hashtablenext = NULL;
	hashiptableprev = NULL;
	hashiptablenext = NULL;

    sMyInfoOriginal = NULL;
    sMyInfoShort = NULL;
    sMyInfoLong = NULL;
	sLastChat = NULL;
	sLastPM = NULL;
	sendbuf = NULL;
	recvbuf = NULL;
	sbplayhead = NULL;
	sLastSearch = NULL;
	sNick = (char *)sDefaultNick;
	sIP[0] = '\0';
	sIPv4[0] = '\0';
	sVersion = NULL;
	sClient = (char *)sOtherNoTag;
	sTag = NULL;
	sDescription = NULL;
	sConnection = NULL;
	MagicByte = '\0';
	sEmail = NULL;
	sTagVersion = NULL;
	sModes[0] = '\0';

    sChangedDescriptionShort = NULL;
    sChangedDescriptionLong = NULL;
    sChangedTagShort = NULL;
    sChangedTagLong = NULL;
    sChangedConnectionShort = NULL;
    sChangedConnectionLong = NULL;
    sChangedEmailShort = NULL;
    sChangedEmailLong = NULL;

	ui8IpLen = 0;
	ui8IPv4Len = 0;

    ui16MyInfoOriginalLen = 0;
    ui16MyInfoShortLen = 0;
    ui16MyInfoLongLen = 0;
	ui16GetNickLists = 0;
	ui16MyINFOs = 0;
	ui16Searchs = 0;
	ui16ChatMsgs = 0;
	ui16PMs = 0;
	ui16SameSearchs = 0;
	ui16LastSearchLen = 0;
	ui16SamePMs = 0;
	ui16LastPMLen = 0;
	ui16SameChatMsgs = 0;
	ui16LastChatLen = 0;
	ui16LastPmLines = 0;
	ui16SameMultiPms = 0;
	ui16LastChatLines = 0;
	ui16SameMultiChats = 0;
	ui16ChatMsgs2 = 0;
	ui16PMs2 = 0;
	ui16Searchs2 = 0;
	ui16MyINFOs2 = 0;
	ui16CTMs = 0;
	ui16CTMs2 = 0;
	ui16RCTMs = 0;
	ui16RCTMs2 = 0;
	ui16SRs = 0;
	ui16SRs2 = 0;
	ui16ChatIntMsgs = 0;
	ui16PMsInt = 0;
	ui16SearchsInt = 0;

    ui32Recvs = 0;
    ui32Recvs2 = 0;

	ui8NickLen = 9;
	iSR = 0;
	iDefloodWarnings = 0;
	Hubs = 0;
	Slots = 0;
	OLimit = 0;
	LLimit = 0;
	DLimit = 0;
	iNormalHubs = 0;
	iRegHubs = 0;
	iOpHubs = 0;
	ui8State = User::STATE_SOCKET_ACCEPTED;

	iProfile = -1;
	sendbuflen = 0;
	recvbuflen = 0;
	sbdatalen = 0;
	rbdatalen = 0;

	iReceivedPmCount = 0;

	time(&LoginTime);

	ui32NickHash = 0;

    memset(&ui128IpHash, 0, 16);
    ui16IpTableIdx = 0;

	ui32BoolBits = 0;
	ui32BoolBits |= User::BIT_IPV4_ACTIVE;
	ui32BoolBits |= User::BIT_OLDHUBSTAG;

    ui32InfoBits = 0;
    ui32SupportBits = 0;

	ui8ConnectionLen = 0;
	ui8DescriptionLen = 0;
	ui8EmailLen = 0;
	ui8TagLen = 0;
	ui8ClientLen = 14;
	ui8TagVersionLen = 0;

    ui8ChangedDescriptionShortLen = 0;
    ui8ChangedDescriptionLongLen = 0;
    ui8ChangedTagShortLen = 0;
    ui8ChangedTagLongLen = 0;
    ui8ChangedConnectionShortLen = 0;
    ui8ChangedConnectionLongLen = 0;
    ui8ChangedEmailShortLen = 0;
    ui8ChangedEmailLongLen = 0;

    ui8Country = 246;

	ui64SharedSize = 0;
	ui64ChangedSharedSizeShort = 0;
    ui64ChangedSharedSizeLong = 0;

	ui64GetNickListsTick = 0;
	ui64MyINFOsTick = 0;
	ui64SearchsTick = 0;
	ui64ChatMsgsTick = 0;
	ui64PMsTick = 0;
	ui64SameSearchsTick = 0;
	ui64SamePMsTick = 0;
	ui64SameChatsTick = 0;
	ui64ChatMsgsTick2 = 0;
	ui64PMsTick2 = 0;
	ui64SearchsTick2 = 0;
	ui64MyINFOsTick2 = 0;
	ui64CTMsTick = 0;
	ui64CTMsTick2 = 0;
	ui64RCTMsTick = 0;
	ui64RCTMsTick2 = 0;
	ui64SRsTick = 0;
	ui64SRsTick2 = 0;
	ui64RecvsTick = 0;
	ui64RecvsTick2 = 0;
	ui64ChatIntMsgsTick = 0;
	ui64PMsIntTick = 0;
	ui64SearchsIntTick = 0;

	iLastMyINFOSendTick = 0;
    iLastNicklist = 0;
	iReceivedPmTick = 0;

	cmdStrt = NULL;
	cmdEnd = NULL;

	cmdActive4Search = NULL;
	cmdActive6Search = NULL;
	cmdPassiveSearch = NULL;

	cmdToUserStrt = NULL;
	cmdToUserEnd = NULL;

	uLogInOut = NULL;
}
//---------------------------------------------------------------------------

User::~User() {
#ifdef _WIN32
	if(recvbuf != NULL) {
		if(HeapFree(clsServerManager::hRecvHeap, HEAP_NO_SERIALIZE, (void *)recvbuf) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate recvbuf in User::~User\n", 0);
        }
    }
#else
	free(recvbuf);
#endif

#ifdef _WIN32
	if(sendbuf != NULL) {
		if(HeapFree(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, (void *)sendbuf) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sendbuf in User::~User\n", 0);
        }
    }
#else
	free(sendbuf);
#endif

#ifdef _WIN32
	if(sLastChat != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastChat) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastChat in User::~User\n", 0);
        }
    }
#else
	free(sLastChat);
#endif

#ifdef _WIN32
	if(sLastPM != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastPM) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastPM in User::~User\n", 0);
        }
    }
#else
	free(sLastPM);
#endif

#ifdef _WIN32
	if(sLastSearch != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastSearch) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastSearch in User::~User\n", 0);
        }
    }
#else
	free(sLastSearch);
#endif

#ifdef _WIN32
	if(sMyInfoShort != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sMyInfoShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sMyInfoShort in User::~User\n", 0);
        }
    }
#else
	free(sMyInfoShort);
#endif

#ifdef _WIN32
	if(sMyInfoLong != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sMyInfoLong) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sMyInfoLong in User::~User\n", 0);
        }
    }
#else
	free(sMyInfoLong);
#endif

#ifdef _WIN32
	if(sMyInfoOriginal != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sMyInfoOriginal) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sMyInfoOriginal in User::~User\n", 0);
        }
    }
#else
	free(sMyInfoOriginal);
#endif

#ifdef _WIN32
	if(sVersion != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sVersion) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sVersion in User::~User\n", 0);
        }
    }
#else
	free(sVersion);
#endif

#ifdef _WIN32
	if(sChangedDescriptionShort != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedDescriptionShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedDescriptionShort in User::~User\n", 0);
        }
    }
#else
	free(sChangedDescriptionShort);
#endif

#ifdef _WIN32
	if(sChangedDescriptionLong != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedDescriptionLong) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedDescriptionLong in User::~User\n", 0);
        }
    }
#else
	free(sChangedDescriptionLong);
#endif

#ifdef _WIN32
	if(sChangedTagShort != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedTagShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedTagShort in User::~User\n", 0);
        }
    }
#else
	free(sChangedTagShort);
#endif

#ifdef _WIN32
	if(sChangedTagLong != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedTagLong) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedTagLong in User::~User\n", 0);
        }
    }
#else
	free(sChangedTagLong);
#endif

#ifdef _WIN32
	if(sChangedConnectionShort != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedConnectionShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedConnectionShort in User::~User\n", 0);
        }
    }
#else
	free(sChangedConnectionShort);
#endif

#ifdef _WIN32
	if(sChangedConnectionLong != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedConnectionLong) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedConnectionLong in User::~User\n", 0);
        }
    }
#else
	free(sChangedConnectionLong);
#endif

#ifdef _WIN32
	if(sChangedEmailShort != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedEmailShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedEmailShort in User::~User\n", 0);
        }
    }
#else
	free(sChangedEmailShort);
#endif

#ifdef _WIN32
	if(sChangedEmailLong != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sChangedEmailLong) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sChangedEmailLong in User::~User\n", 0);
        }
    }
#else
	free(sChangedEmailLong);
#endif

	if(((ui32SupportBits & User::SUPPORTBIT_ZPIPE) == User::SUPPORTBIT_ZPIPE) == true)
        clsDcCommands::mPtr->iStatZPipe--;

	clsServerManager::ui32Parts++;

#ifdef _BUILD_GUI
    if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_SHOW_COMMANDS], BM_GETCHECK, 0, 0) == BST_CHECKED) {
        RichEditAppendText(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::REDT_CHAT], ("x User removed: " + string(sNick, ui8NickLen) + " (Socket " + string(Sck) + ")").c_str());
    }
#endif

	if(sNick != sDefaultNick) {
#ifdef _WIN32
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sNick) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sNick in User::~User\n", 0);
		}
#else
		free(sNick);
#endif
	}
        
	delete uLogInOut;
    
	if(cmdActive4Search != NULL) {
        User::DeletePrcsdUsrCmd(cmdActive4Search);
		cmdActive4Search = NULL;
    }

	if(cmdActive6Search != NULL) {
        User::DeletePrcsdUsrCmd(cmdActive6Search);
		cmdActive6Search = NULL;
    }

	if(cmdPassiveSearch != NULL) {
        User::DeletePrcsdUsrCmd(cmdPassiveSearch);
		cmdPassiveSearch = NULL;
    }
                
	PrcsdUsrCmd *next = cmdStrt;
        
    while(next != NULL) {
        PrcsdUsrCmd *cur = next;
        next = cur->next;

#ifdef _WIN32
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand in User::~User\n", 0);
        }
#else
		free(cur->sCommand);
#endif
		cur->sCommand = NULL;

        delete cur;
	}

	cmdStrt = NULL;
	cmdEnd = NULL;

	PrcsdToUsrCmd *nextto = cmdToUserStrt;
                    
    while(nextto != NULL) {
        PrcsdToUsrCmd *curto = nextto;
        nextto = curto->next;

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curto->sCommand) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate curto->sCommand in User::~User\n", 0);
        }
#else
		free(curto->sCommand);
#endif
        curto->sCommand = NULL;

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curto->ToNick) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate curto->ToNick in User::~User\n", 0);
        }
#else
		free(curto->ToNick);
#endif
        curto->ToNick = NULL;

        delete curto;
	}
    

	cmdToUserStrt = NULL;
	cmdToUserEnd = NULL;
}
//---------------------------------------------------------------------------

bool User::MakeLock() {
    size_t szAllignLen = Allign1024(63);
#ifdef _WIN32
    sendbuf = (char *)HeapAlloc(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, szAllignLen);
#else
	sendbuf = (char *)malloc(szAllignLen);
#endif
    if(sendbuf == NULL) {
        sbdatalen = 0;

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes in User::MakeLock\n", (uint64_t)szAllignLen);

        return false;
    }
    sendbuflen = (uint32_t)(szAllignLen-1);
	sbplayhead = sendbuf;

    // This code computes the valid Lock string including the Pk= string
    // For maximum speed we just find two random numbers - start and step
    // Step is added each cycle to the start and the ascii 122 boundary is
    // checked. If overflow occurs then the overflowed value is added to the
    // ascii 48 value ("0") and continues.
	// The lock has fixed length 63 bytes

#ifdef _WIN32
	#ifdef _BUILD_GUI
	    #ifndef _M_X64
	        static const char * sLock = "$Lock EXTENDEDPROTOCOL                           win Pk=PtokaX|"; // 63
	    #else
	        static const char * sLock = "$Lock EXTENDEDPROTOCOL                           wg6 Pk=PtokaX|"; // 63
	    #endif
	#else
	    #ifndef _M_X64
	        static const char * sLock = "$Lock EXTENDEDPROTOCOL                           wis Pk=PtokaX|"; // 63
	    #else
	        static const char * sLock = "$Lock EXTENDEDPROTOCOL                           ws6 Pk=PtokaX|"; // 63
	    #endif
	#endif
#else
	static const char * sLock = "$Lock EXTENDEDPROTOCOL                           nix Pk=PtokaX|"; // 63
#endif

    // append data to the buffer
    memcpy(sendbuf, sLock, 63);
    sbdatalen += 63;
    sendbuf[sbdatalen] = '\0';

	for(uint8_t ui8i = 22; ui8i < 49; ui8i++) {
#ifdef _WIN32
        sendbuf[ui8i] = (char)((rand() % 74) + 48);
#else
        sendbuf[ui8i] = (char)((random() % 74) + 48);
#endif
	}

//	Memo(string(sendbuf, sbdatalen));

#ifdef _WIN32
    uLogInOut->pBuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, 64);
#else
	uLogInOut->pBuffer = (char *)malloc(64);
#endif
    if(uLogInOut->pBuffer == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate 64 bytes for pBuffer in User::MakeLock\n", 0);
		return false;
    }
    
    memcpy(uLogInOut->pBuffer, sendbuf, 63);
	uLogInOut->pBuffer[63] = '\0';

    return true;
}
//---------------------------------------------------------------------------

bool User::DoRecv() {
    if((ui32BoolBits & BIT_ERROR) == BIT_ERROR || ui8State >= STATE_CLOSING)
        return false;

#ifdef _WIN32
	u_long iAvailBytes = 0;
	if(ioctlsocket(Sck, FIONREAD, &iAvailBytes) == SOCKET_ERROR) {
		int iError = WSAGetLastError();
#else
	int iAvailBytes = 0;
	if(ioctl(Sck, FIONREAD, &iAvailBytes) == -1) {
#endif
		clsUdpDebug::mPtr->Broadcast("[ERR] " + string(sNick, ui8NickLen) + " (" + string(sIP, ui8IpLen) +
#ifdef _WIN32
			" ): ioctlsocket(FIONREAD) error " + string(WSErrorStr(iError)) + " (" + string(iError) + "). User is being closed.");
#else
			" ): ioctlsocket(FIONREAD) error " + string(ErrnoStr(errno)) + " (" + string(errno) + "). User is being closed.");
#endif
        ui32BoolBits |= BIT_ERROR;
		Close();
        return false;
    }

    // PPK ... check flood ...
	if(iAvailBytes != 0 && clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NODEFLOODRECV) == false) {
        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_ACTION] != 0) {
    		if(ui32Recvs == 0) {
    			ui64RecvsTick = clsServerManager::ui64ActualTick;
            }

            ui32Recvs += iAvailBytes;

			if(DeFloodCheckForDataFlood(this, DEFLOOD_MAX_DOWN, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_ACTION],
			  ui32Recvs, ui64RecvsTick, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_KB],
              (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_TIME]) == true) {
				return false;
            }

    		if(ui32Recvs != 0) {
                ui32Recvs -= iAvailBytes;
            }
        }

        if(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_ACTION2] != 0) {
    		if(ui32Recvs2 == 0) {
    			ui64RecvsTick2 = clsServerManager::ui64ActualTick;
            }

            ui32Recvs2 += iAvailBytes;

			if(DeFloodCheckForDataFlood(this, DEFLOOD_MAX_DOWN, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_ACTION2],
			  ui32Recvs2, ui64RecvsTick2, clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_KB2],
			  (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_DOWN_TIME2]) == true) {
                return false;
            }

    		if(ui32Recvs2 != 0) {
                ui32Recvs2 -= iAvailBytes;
            }
        }
    }

	if(iAvailBytes == 0) {
		// we need to try recv to catch connection error or closed connection
        iAvailBytes = 16;
    } else if(iAvailBytes > 16384) {
        // receive max. 16384 bytes to receive buffer
        iAvailBytes = 16384;
    }

    size_t szAllignLen = 0;

    if(recvbuflen < rbdatalen+iAvailBytes) {
        szAllignLen = Allign512(rbdatalen+iAvailBytes);
    } else if(iRecvCalled > 60) {
        szAllignLen = Allign512(rbdatalen+iAvailBytes);
        if(recvbuflen <= szAllignLen) {
            szAllignLen = 0;
        }

        iRecvCalled = 0;
    }

    if(szAllignLen != 0) {
        char * pOldBuf = recvbuf;

#ifdef _WIN32
        if(recvbuf == NULL) {
            recvbuf = (char *)HeapAlloc(clsServerManager::hRecvHeap, HEAP_NO_SERIALIZE, szAllignLen);
        } else {
            recvbuf = (char *)HeapReAlloc(clsServerManager::hRecvHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szAllignLen);
        }
#else
        recvbuf = (char *)realloc(pOldBuf, szAllignLen);
#endif
		if(recvbuf == NULL) {
            recvbuf = pOldBuf;
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot (re)allocate %" PRIu64 " bytes for recvbuf in User::DoRecv\n", (uint64_t)szAllignLen);

			return false;
		}

		recvbuflen = (uint32_t)(szAllignLen-1);
	}
    
    // receive new data to recvbuf
	int recvlen = recv(Sck, recvbuf+rbdatalen, recvbuflen-rbdatalen, 0);
	iRecvCalled++;

#ifdef _WIN32
    if(recvlen == SOCKET_ERROR) {
		int iError = WSAGetLastError();
        if(iError != WSAEWOULDBLOCK) {
#else
    if(recvlen == -1) {
        if(errno != EAGAIN) {
#endif
			clsUdpDebug::mPtr->Broadcast("[ERR] " + string(sNick, ui8NickLen) + " (" + string(sIP, ui8IpLen) + "): recv() error " + 
#ifdef _WIN32
                string(WSErrorStr(iError)) + ". User is being closed.");
#else
				string(ErrnoStr(errno)) + " (" + string(errno) + "). User is being closed.");
#endif
			ui32BoolBits |= BIT_ERROR;
            Close();
            return false;
        } else {
            return false;
        }
    } else if(recvlen == 0) { // regular close
#ifdef _WIN32
        int iError = WSAGetLastError();
	    if(iError != 0) {
			Memo("[SYS] recvlen == 0 and iError == "+string(iError)+"! User: "+string(sNick, ui8NickLen)+" ("+string(sIP, ui8IpLen)+")");
			clsUdpDebug::mPtr->Broadcast("[SYS] recvlen == 0 and iError == " + string(iError) + " ! User: " + string(sNick, ui8NickLen) +
				" (" + string(sIP, ui8IpLen) + ").");
		}

	#ifdef _BUILD_GUI
        if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_SHOW_COMMANDS], BM_GETCHECK, 0, 0) == BST_CHECKED) {
			int iret = sprintf(msg, "- User has closed the connection: %s (%s)", sNick, sIP);
			if(CheckSprintf(iret, 1024, "User::DoRecv") == true) {
				RichEditAppendText(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::REDT_CHAT], msg);
			}
        }
    #endif
#endif

        ui32BoolBits |= BIT_ERROR;
        Close();
	    return false;
    }

    ui32Recvs += recvlen;
    ui32Recvs2 += recvlen;
	clsServerManager::ui64BytesRead += recvlen;
	rbdatalen += recvlen;
	recvbuf[rbdatalen] = '\0';
    if(UserProcessLines(this, rbdatalen-recvlen) == true) {
        return true;
    }
        
    return false;
}
//---------------------------------------------------------------------------

void User::SendChar(const char * cText, const size_t &szTextLen) {
	if(ui8State >= STATE_CLOSING || szTextLen == 0)
        return;

    if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false || szTextLen < ZMINDATALEN) {
        if(PutInSendBuf(cText, szTextLen)) {
            Try2Send();
        }
    } else {
        uint32_t iLen = 0;
        char *sData = clsZlibUtility::mPtr->CreateZPipe(cText, szTextLen, iLen);
            
        if(iLen == 0) {
            if(PutInSendBuf(cText, szTextLen)) {
                Try2Send();
            }
        } else {
            clsServerManager::ui64BytesSentSaved += szTextLen-iLen;
            if(PutInSendBuf(sData, iLen)) {
                Try2Send();
            }
        }
    }
}
//---------------------------------------------------------------------------

void User::SendCharDelayed(const char * cText, const size_t &szTextLen) {
	if(ui8State >= STATE_CLOSING || szTextLen == 0) {
        return;
    }
        
    if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false || szTextLen < ZMINDATALEN) {
        PutInSendBuf(cText, szTextLen);
    } else {
        uint32_t iLen = 0;
        char *sPipeData = clsZlibUtility::mPtr->CreateZPipe(cText, szTextLen, iLen);
        
        if(iLen == 0) {
            PutInSendBuf(cText, szTextLen);
        } else {
            PutInSendBuf(sPipeData, iLen);
            clsServerManager::ui64BytesSentSaved += szTextLen-iLen;
        }
    }
}
//---------------------------------------------------------------------------

void User::SendTextDelayed(const string &sText) {
	if(ui8State >= STATE_CLOSING || sText.size() == 0) {
        return;
    }
      
    if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false || sText.size() < ZMINDATALEN) {
        PutInSendBuf(sText.c_str(), sText.size());
    } else {
        uint32_t iLen = 0;
        char *sData = clsZlibUtility::mPtr->CreateZPipe(sText.c_str(), sText.size(), iLen);
            
        if(iLen == 0) {
            PutInSendBuf(sText.c_str(), sText.size());
        } else {
            PutInSendBuf(sData, iLen);
            clsServerManager::ui64BytesSentSaved += sText.size()-iLen;
        }
    }
}
//---------------------------------------------------------------------------

bool User::PutInSendBuf(const char * Text, const size_t &szTxtLen) {
	iSendCalled++;

    size_t szAllignLen = 0;

    if(sendbuflen < sbdatalen+szTxtLen) {
        if(sendbuf == NULL) {
            szAllignLen = Allign1024(sbdatalen+szTxtLen);
        } else {
            if((size_t)(sbplayhead-sendbuf) > szTxtLen) {
                uint32_t offset = (uint32_t)(sbplayhead-sendbuf);
                memmove(sendbuf, sbplayhead, (sbdatalen-offset));
                sbplayhead = sendbuf;
                sbdatalen = sbdatalen-offset;
            } else {
                szAllignLen = Allign1024(sbdatalen+szTxtLen);
                size_t szMaxBufLen = (size_t)(((ui32BoolBits & BIT_BIG_SEND_BUFFER) == BIT_BIG_SEND_BUFFER) == true ?
                    ((clsUsers::mPtr->myInfosTagLen > clsUsers::mPtr->myInfosLen ? clsUsers::mPtr->myInfosTagLen : clsUsers::mPtr->myInfosLen)*2) :
                    (clsUsers::mPtr->myInfosTagLen > clsUsers::mPtr->myInfosLen ? clsUsers::mPtr->myInfosTagLen : clsUsers::mPtr->myInfosLen));
                szMaxBufLen = szMaxBufLen < 262144 ? 262144 :szMaxBufLen;
                if(szAllignLen > szMaxBufLen) {
                    // does the buffer size reached the maximum
                    if(clsSettingManager::mPtr->bBools[SETBOOL_KEEP_SLOW_USERS] == false || (ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) {
                        // we want to drop the slow user
                        ui32BoolBits |= BIT_ERROR;
                        Close();

                        clsUdpDebug::mPtr->Broadcast("[SYS] " + string(sNick, ui8NickLen) + " (" + string(sIP, ui8IpLen) +") SendBuffer overflow (AL:"+string((uint64_t)szAllignLen)+
                            "[SL:"+string(sbdatalen)+"|NL:"+string((uint64_t)szTxtLen)+"|FL:"+string((uint64_t)(sbplayhead-sendbuf))+"]/ML:"+string((uint64_t)szMaxBufLen)+"). User disconnected.");
                        return false;
                    } else {
    				    clsUdpDebug::mPtr->Broadcast("[SYS] " + string(sNick, ui8NickLen) + " (" + string(sIP, ui8IpLen) +") SendBuffer overflow (AL:"+string((uint64_t)szAllignLen)+
                            "[SL:"+string(sbdatalen)+"|NL:"+string((uint64_t)szTxtLen)+"|FL:"+string((uint64_t)(sbplayhead-sendbuf))+"]/ML:"+string((uint64_t)szMaxBufLen)+
                            "). Buffer cleared - user stays online.");
                    }

                    // we want to keep the slow user online
                    // PPK ... i don't want to corrupt last command, get rest of it and add to new buffer ;)
                    char *sTemp = (char *)memchr(sbplayhead, '|', sbdatalen-(sbplayhead-sendbuf));
                    if(sTemp != NULL) {
                        uint32_t iOldSBDataLen = sbdatalen;

                        uint32_t iRestCommandLen = (uint32_t)((sTemp-sbplayhead)+1);
                        if(sendbuf != sbplayhead) {
                            memmove(sendbuf, sbplayhead, iRestCommandLen);
                        }
                        sbdatalen = iRestCommandLen;

                        // If is not needed then don't lost all data, try to find some space with removing only few oldest commands
                        if(szTxtLen < szMaxBufLen && iOldSBDataLen > (uint32_t)((sTemp+1)-sendbuf) && (iOldSBDataLen-((sTemp+1)-sendbuf)) > (uint32_t)szTxtLen) {
                            char *sTemp1;
                            // try to remove min half of send bufer
                            if(iOldSBDataLen > (sendbuflen/2) && (uint32_t)((sTemp+1+szTxtLen)-sendbuf) < (sendbuflen/2)) {
                                sTemp1 = (char *)memchr(sendbuf+(sendbuflen/2), '|', iOldSBDataLen-(sendbuflen/2));
                            } else {
                                sTemp1 = (char *)memchr(sTemp+1+szTxtLen, '|', iOldSBDataLen-((sTemp+1+szTxtLen)-sendbuf));
                            }

                            if(sTemp1 != NULL) {
                                iRestCommandLen = (uint32_t)(iOldSBDataLen-((sTemp1+1)-sendbuf));
                                memmove(sendbuf+sbdatalen, sTemp1+1, iRestCommandLen);
                                sbdatalen += iRestCommandLen;
                            }
                        }
                    } else {
                        sendbuf[0] = '|';
                        sendbuf[1] = '\0';
                        sbdatalen = 1;
                    }

                    size_t szAllignTxtLen = Allign1024(szTxtLen+sbdatalen);

                    char * pOldBuf = sendbuf;
#ifdef _WIN32
                    sendbuf = (char *)HeapReAlloc(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szAllignTxtLen);
#else
				    sendbuf = (char *)realloc(pOldBuf, szAllignTxtLen);
#endif
                    if(sendbuf == NULL) {
                        sendbuf = pOldBuf;
                        ui32BoolBits |= BIT_ERROR;
                        Close();

                        AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in User::PutInSendBuf-keepslow\n", (uint64_t)szAllignLen);

                        return false;
                    }
                    sendbuflen = (uint32_t)(szAllignTxtLen-1);
                    sbplayhead = sendbuf;

                    szAllignLen = 0;
                } else {
                    szAllignLen = Allign1024(sbdatalen+szTxtLen);
                }
        	}
        }
    } else if(iSendCalled > 100) {
        szAllignLen = Allign1024(sbdatalen+szTxtLen);
        if(sendbuflen <= szAllignLen) {
            szAllignLen = 0;
        }

        iSendCalled = 0;
    }

    if(szAllignLen != 0) {
        uint32_t offset = (sendbuf == NULL ? 0 : (uint32_t)(sbplayhead-sendbuf));

        char * pOldBuf = sendbuf;
#ifdef _WIN32
        if(sendbuf == NULL) {
            sendbuf = (char *)HeapAlloc(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, szAllignLen);
        } else {
            sendbuf = (char *)HeapReAlloc(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, szAllignLen);
        }
#else
		sendbuf = (char *)realloc(pOldBuf, szAllignLen);
#endif
        if(sendbuf == NULL) {
            sendbuf = pOldBuf;
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot (re)allocate %" PRIu64 " bytes for new sendbuf in User::PutInSendBuf\n", (uint64_t)szAllignLen);

        	return false;
        }

        sendbuflen = (uint32_t)(szAllignLen-1);
        sbplayhead = sendbuf+offset;
    }

    // append data to the buffer
    memcpy(sendbuf+sbdatalen, Text, szTxtLen);
    sbdatalen += (uint32_t)szTxtLen;
    sendbuf[sbdatalen] = '\0';

    return true;
}
//---------------------------------------------------------------------------

bool User::Try2Send() {
    if((ui32BoolBits & BIT_ERROR) == BIT_ERROR || sbdatalen == 0) {
        return false;
    }

    // compute length of unsent data
    int32_t offset = (int32_t)(sbplayhead - sendbuf);
	int32_t len = sbdatalen - offset;

	if(offset < 0 || len < 0) {
		int imsgLen = sprintf(msg, "%s - [ERR] Negative send values!\nSendBuf: %p\nPlayHead: %p\nDataLen: %u\n", "%s", sendbuf, sbplayhead, sbdatalen);
        if(CheckSprintf(imsgLen, 1024, "UserTry2Send") == true) {
    		AppendDebugLog(msg, 0);
        }

        ui32BoolBits |= BIT_ERROR;
        Close();

        return false;
    }

    int n = send(Sck, sbplayhead, len < 32768 ? len : 32768, 0);

#ifdef _WIN32
    if(n == SOCKET_ERROR) {
    	int iError = WSAGetLastError();
        if(iError != WSAEWOULDBLOCK) {
#else
	if(n == -1) {
        if(errno != EAGAIN) {
#endif
			clsUdpDebug::mPtr->Broadcast("[ERR] " + string(sNick, ui8NickLen) + " (" + string(sIP, ui8IpLen) +
#ifdef _WIN32
				"): send() error " + string(WSErrorStr(iError)) + ". User is being closed.");
#else
				"): send() error " + string(ErrnoStr(errno)) + " (" + string(errno) + "). User is being closed.");
#endif
			ui32BoolBits |= BIT_ERROR;
            Close();
            return false;
        } else {
            return true;
        }
    }

	clsServerManager::ui64BytesSent += n;

	// if buffer is sent then mark it as empty (first byte = 0)
	// else move remaining data on new place and free old buffer
	if(n < len) {
        sbplayhead += n;
		return true;
	} else {
        // PPK ... we need to free memory allocated for big buffer on login (userlist, motd...)
        if(((ui32BoolBits & BIT_BIG_SEND_BUFFER) == BIT_BIG_SEND_BUFFER) == true) {
            if(sendbuf != NULL) {
#ifdef _WIN32
               if(HeapFree(clsServerManager::hSendHeap, HEAP_NO_SERIALIZE, (void *)sendbuf) == 0) {
					AppendDebugLog("%s - [MEM] Cannot deallocate sendbuf in User::Try2Send\n", 0);
                }
#else
				free(sendbuf);
#endif
                sendbuf = NULL;
                sbplayhead = sendbuf;
                sendbuflen = 0;
                sbdatalen = 0;
            }
            ui32BoolBits &= ~BIT_BIG_SEND_BUFFER;
        } else {
    		sendbuf[0] = '\0';
            sbplayhead = sendbuf;
            sbdatalen = 0;
        }
		return false;
	}
}
//---------------------------------------------------------------------------

void User::SetIP(char * sNewIP) {
    strcpy(sIP, sNewIP);
    ui8IpLen = (uint8_t)strlen(sIP);
}
//------------------------------------------------------------------------------

void User::SetNick(char * sNewNick, const uint8_t &ui8NewNickLen) {
	if(sNick != sDefaultNick && sNick != NULL) {
#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sNick) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sNick in User::SetNick\n", 0);
        }
#else
		free(sNick);
#endif
        sNick = NULL;
    }

#ifdef _WIN32
    sNick = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, ui8NewNickLen+1);
#else
	sNick = (char *)malloc(ui8NewNickLen+1);
#endif
    if(sNick == NULL) {
        sNick = (char *)sDefaultNick;
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sNick in User::SetNick\n", (uint64_t)(ui8NewNickLen+1));

        return;
    }   
    memcpy(sNick, sNewNick, ui8NewNickLen);
    sNick[ui8NewNickLen] = '\0';
    ui8NickLen = ui8NewNickLen;
    ui32NickHash = HashNick(sNick, ui8NickLen);
}
//------------------------------------------------------------------------------

void User::SetMyInfoOriginal(char * sNewMyInfo, const uint16_t &ui16NewMyInfoLen) {
    char * sOldMyInfo = sMyInfoOriginal;

    char * sOldDescription = sDescription;
    uint8_t ui8OldDescriptionLen = ui8DescriptionLen;

    char * sOldTag = sTag;
    uint8_t ui8OldTagLen = ui8TagLen;

    char * sOldConnection = sConnection;
    uint8_t ui8OldConnectionLen = ui8ConnectionLen;

    char * sOldEmail = sEmail;
    uint8_t ui8OldEmailLen = ui8EmailLen;

    uint64_t ui64OldShareSize = ui64SharedSize;

	if(sMyInfoOriginal != NULL) {
        sConnection = NULL;
        ui8ConnectionLen = 0;

        sDescription = NULL;
        ui8DescriptionLen = 0;

        sEmail = NULL;
        ui8EmailLen = 0;

        sTag = NULL;
        ui8TagLen = 0;

        sClient = NULL;
        ui8ClientLen = 0;

        sTagVersion = NULL;
        ui8TagVersionLen = 0;

        sMyInfoOriginal = NULL;
    }

#ifdef _WIN32
    sMyInfoOriginal = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, ui16NewMyInfoLen+1);
#else
	sMyInfoOriginal = (char *)malloc(ui16NewMyInfoLen+1);
#endif
    if(sMyInfoOriginal == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sMyInfoOriginal in UserSetMyInfoOriginal\n", (uint64_t)(ui16NewMyInfoLen+1));

        return;
    }
    memcpy(sMyInfoOriginal, sNewMyInfo, ui16NewMyInfoLen);
    sMyInfoOriginal[ui16NewMyInfoLen] = '\0';
    ui16MyInfoOriginalLen = ui16NewMyInfoLen;

    UserParseMyInfo(this);

    if(ui8OldDescriptionLen != ui8DescriptionLen || (ui8DescriptionLen > 0 && memcmp(sOldDescription, sDescription, ui8DescriptionLen) != 0)) {
        ui32InfoBits |= INFOBIT_DESCRIPTION_CHANGED;
    } else {
        ui32InfoBits &= ~INFOBIT_DESCRIPTION_CHANGED;
    }

    if(ui8OldTagLen != ui8TagLen || (ui8TagLen > 0 && memcmp(sOldTag, sTag, ui8TagLen) != 0)) {
        ui32InfoBits |= INFOBIT_TAG_CHANGED;
    } else {
        ui32InfoBits &= ~INFOBIT_TAG_CHANGED;
    }

    if(ui8OldConnectionLen != ui8ConnectionLen || (ui8ConnectionLen > 0 && memcmp(sOldConnection, sConnection, ui8ConnectionLen) != 0)) {
        ui32InfoBits |= INFOBIT_CONNECTION_CHANGED;
    } else {
        ui32InfoBits &= ~INFOBIT_CONNECTION_CHANGED;
    }

    if(ui8OldEmailLen != ui8EmailLen || (ui8EmailLen > 0 && memcmp(sOldEmail, sEmail, ui8EmailLen) != 0)) {
        ui32InfoBits |= INFOBIT_EMAIL_CHANGED;
    } else {
        ui32InfoBits &= ~INFOBIT_EMAIL_CHANGED;
    }

    if(ui64OldShareSize != ui64SharedSize) {
        ui32InfoBits |= INFOBIT_SHARE_CHANGED;
    } else {
        ui32InfoBits &= ~INFOBIT_SHARE_CHANGED;
    }

    if(sOldMyInfo != NULL) {
#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldMyInfo) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate sOldMyInfo in UserSetMyInfoOriginal\n", 0);
        }
#else
        free(sOldMyInfo);
#endif
    }

    if(((ui32InfoBits & INFOBIT_SHARE_SHORT_PERM) == INFOBIT_SHARE_SHORT_PERM) == false) {
        ui64ChangedSharedSizeShort = ui64SharedSize;
    }

    if(((ui32InfoBits & INFOBIT_SHARE_LONG_PERM) == INFOBIT_SHARE_LONG_PERM) == false) {
        ui64ChangedSharedSizeLong = ui64SharedSize;
    }

}
//------------------------------------------------------------------------------

static void UserSetMyInfoLong(User * u, char * sNewMyInfoLong, const uint16_t &ui16NewMyInfoLongLen) {
	if(u->sMyInfoLong != NULL) {
        if(clsSettingManager::mPtr->ui8FullMyINFOOption != 2) {
    	    clsUsers::mPtr->DelFromMyInfosTag(u);
        }

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)u->sMyInfoLong) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate u->sMyInfoLong in UserSetMyInfoLong\n", 0);
        }
#else
        free(u->sMyInfoLong);
#endif
        u->sMyInfoLong = NULL;
    }

#ifdef _WIN32
    u->sMyInfoLong = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, ui16NewMyInfoLongLen+1);
#else
	u->sMyInfoLong = (char *)malloc(ui16NewMyInfoLongLen+1);
#endif
    if(u->sMyInfoLong == NULL) {
        u->ui32BoolBits |= User::BIT_ERROR;
        u->Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sMyInfoLong in UserSetMyInfoLong\n", (uint64_t)(ui16NewMyInfoLongLen+1));

        return;
    }   
    memcpy(u->sMyInfoLong, sNewMyInfoLong, ui16NewMyInfoLongLen);
    u->sMyInfoLong[ui16NewMyInfoLongLen] = '\0';
    u->ui16MyInfoLongLen = ui16NewMyInfoLongLen;
}
//------------------------------------------------------------------------------

static void UserSetMyInfoShort(User * u, char * sNewMyInfoShort, const uint16_t &ui16NewMyInfoShortLen) {
	if(u->sMyInfoShort != NULL) {
        if(clsSettingManager::mPtr->ui8FullMyINFOOption != 0) {
    	    clsUsers::mPtr->DelFromMyInfos(u);
        }

#ifdef _WIN32    	    
    	if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)u->sMyInfoShort) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate u->sMyInfoShort in UserSetMyInfoShort\n", 0);
        }
#else
		free(u->sMyInfoShort);
#endif
        u->sMyInfoShort = NULL;
    }

#ifdef _WIN32
    u->sMyInfoShort = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, ui16NewMyInfoShortLen+1);
#else
	u->sMyInfoShort = (char *)malloc(ui16NewMyInfoShortLen+1);
#endif
    if(u->sMyInfoShort == NULL) {
        u->ui32BoolBits |= User::BIT_ERROR;
        u->Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for MyInfoShort in UserSetMyInfoShort\n", (uint64_t)(ui16NewMyInfoShortLen+1));

        return;
    }   
    memcpy(u->sMyInfoShort, sNewMyInfoShort, ui16NewMyInfoShortLen);
    u->sMyInfoShort[ui16NewMyInfoShortLen] = '\0';
    u->ui16MyInfoShortLen = ui16NewMyInfoShortLen;
}
//------------------------------------------------------------------------------

void User::SetVersion(char * sNewVer) {
#ifdef _WIN32
	if(sVersion) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sVersion) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sVersion in User::SetVersion\n", 0);
        }
    }
#else
	free(sVersion);
#endif

    size_t szLen = strlen(sNewVer);
#ifdef _WIN32
    sVersion = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
	sVersion = (char *)malloc(szLen+1);
#endif
    if(sVersion == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for Version in User::SetVersion\n", (uint64_t)(szLen+1));

        return;
    }   
    memcpy(sVersion, sNewVer, szLen);
    sVersion[szLen] = '\0';
}
//------------------------------------------------------------------------------

void User::SetLastChat(char * sNewData, const size_t &szLen) {
#ifdef _WIN32
    if(sLastChat != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastChat) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastChat in User::SetLastChat\n", 0);
        }
    }
#else
	free(sLastChat);
#endif

#ifdef _WIN32
    sLastChat = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
	sLastChat = (char *)malloc(szLen+1);
#endif
    if(sLastChat == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sLastChat in User::SetLastChat\n", (uint64_t)(szLen+1));

        return;
    }   
    memcpy(sLastChat, sNewData, szLen);
    sLastChat[szLen] = '\0';
    ui16SameChatMsgs = 1;
    ui64SameChatsTick = clsServerManager::ui64ActualTick;
    ui16LastChatLen = (uint16_t)szLen;
    ui16SameMultiChats = 0;
    ui16LastChatLines = 0;
}
//------------------------------------------------------------------------------

void User::SetLastPM(char * sNewData, const size_t &szLen) {
#ifdef _WIN32
    if(sLastPM != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastPM) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastPM in User::SetLastPM\n", 0);
        }
    }
#else
	free(sLastPM);
#endif

#ifdef _WIN32
    sLastPM = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
	sLastPM = (char *)malloc(szLen+1);
#endif
    if(sLastPM == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sLastPM in User::SetLastPM\n", (uint64_t)(szLen+1));

        return;
    }

    memcpy(sLastPM, sNewData, szLen);
    sLastPM[szLen] = '\0';
    ui16SamePMs = 1;
    ui64SamePMsTick = clsServerManager::ui64ActualTick;
    ui16LastPMLen = (uint16_t)szLen;
    ui16SameMultiPms = 0;
    ui16LastPmLines = 0;
}
//------------------------------------------------------------------------------

void User::SetLastSearch(char * sNewData, const size_t &szLen) {
#ifdef _WIN32
    if(sLastSearch != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sLastSearch) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sLastSearch in User::SetLastSearch\n", 0);
        }
    }
#else
	free(sLastSearch);
#endif

#ifdef _WIN32
    sLastSearch = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
	sLastSearch = (char *)malloc(szLen+1);
#endif
    if(sLastSearch == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

        AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sLastSearch in User::SetLastSearch\n", (uint64_t)(szLen+1));

        return;
    }   
    memcpy(sLastSearch, sNewData, szLen);
    sLastSearch[szLen] = '\0';
    ui16SameSearchs = 1;
    ui64SameSearchsTick = clsServerManager::ui64ActualTick;
    ui16LastSearchLen = (uint16_t)szLen;
}
//------------------------------------------------------------------------------

void User::SetBuffer(char * sKickMsg, size_t szLen/* = 0*/) {
    if(szLen == 0) {
        szLen = strlen(sKickMsg);
    }

    if(uLogInOut == NULL) {
        uLogInOut = new LoginLogout();
        if(uLogInOut == NULL) {
    		ui32BoolBits |= BIT_ERROR;
    		Close();

    		AppendDebugLog("%s - [MEM] Cannot allocate new uLogInOut in User::SetBuffer\n", 0);
    		return;
        }
    }

	void * pOldBuf = uLogInOut->pBuffer;

    if(szLen < 512) {
#ifdef _WIN32
		if(uLogInOut->pBuffer == NULL) {
			uLogInOut->pBuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
		} else {
			uLogInOut->pBuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, pOldBuf, szLen+1);
		}
#else
		uLogInOut->pBuffer = (char *)realloc(pOldBuf, szLen+1);
#endif
        if(uLogInOut->pBuffer == NULL) {
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for pBuffer in User::SetBuffer\n", (uint64_t)(szLen+1));

            return;
        }
        memcpy(uLogInOut->pBuffer, sKickMsg, szLen);
        uLogInOut->pBuffer[szLen] = '\0';
    } else {
#ifdef _WIN32
		if(uLogInOut->pBuffer == NULL) {
			uLogInOut->pBuffer = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, 512);
		} else {
			uLogInOut->pBuffer = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, pOldBuf, 512);
		}
#else
		uLogInOut->pBuffer = (char *)realloc(pOldBuf, 512);
#endif
        if(uLogInOut->pBuffer == NULL) {
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot allocate 256 bytes for pBuffer in User::SetBuffer\n", 0);

            return;
        }
        memcpy(uLogInOut->pBuffer, sKickMsg, 508);
        uLogInOut->pBuffer[511] = '\0';
        uLogInOut->pBuffer[510] = '.';
        uLogInOut->pBuffer[509] = '.';
        uLogInOut->pBuffer[508] = '.';
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void User::FreeBuffer() {
    if(uLogInOut->pBuffer != NULL) {
#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)uLogInOut->pBuffer) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate uLogInOut->pBuffer in User::FreeBuffer\n", 0);
        }
#else
        free(uLogInOut->pBuffer);
#endif
        uLogInOut->pBuffer = NULL;
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void User::Close(bool bNoQuit/* = false*/) {
    if(ui8State >= STATE_CLOSING) {
        return;
    }
    
	// nick in hash table ?
	if((ui32BoolBits & BIT_HASHED) == BIT_HASHED) {
    	clsHashManager::mPtr->Remove(this);
    }

    // nick in nick/op list ?
    if(ui8State >= STATE_ADDME_2LOOP) {  
		clsUsers::mPtr->DelFromNickList(sNick, (ui32BoolBits & BIT_OPERATOR) == BIT_OPERATOR);
		clsUsers::mPtr->DelFromUserIP(this);

        // PPK ... fix for QuickList nad ghost...
        // and fixing redirect all too ;)
        // and fix disconnect on send error too =)
        if(bNoQuit == false) {         
            int imsgLen = sprintf(msg, "$Quit %s|", sNick); 
            if(CheckSprintf(imsgLen, 1024, "User::Close") == true) {
                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen, NULL, 0, clsGlobalDataQueue::CMD_QUIT);
            }

			clsUsers::mPtr->Add2RecTimes(this);
        }

#ifdef _BUILD_GUI
        if(::SendMessage(clsMainWindowPageUsersChat::mPtr->hWndPageItems[clsMainWindowPageUsersChat::BTN_AUTO_UPDATE_USERLIST], BM_GETCHECK, 0, 0) == BST_CHECKED) {
            clsMainWindowPageUsersChat::mPtr->RemoveUser(this);
        }
#endif

        //sqldb->FinalizeVisit(u);

		if(((ui32BoolBits & BIT_HAVE_SHARECOUNTED) == BIT_HAVE_SHARECOUNTED) == true) {
            clsServerManager::ui64TotalShare -= ui64SharedSize;
            ui32BoolBits &= ~BIT_HAVE_SHARECOUNTED;
		}

		clsScriptManager::mPtr->UserDisconnected(this);
	}

    if(ui8State > STATE_ADDME_2LOOP) {
        clsServerManager::ui32Logged--;
    }

	ui8State = STATE_CLOSING;
	
    if(cmdActive4Search != NULL) {
        User::DeletePrcsdUsrCmd(cmdActive4Search);
        cmdActive4Search = NULL;
    }

    if(cmdActive6Search != NULL) {
        User::DeletePrcsdUsrCmd(cmdActive6Search);
        cmdActive6Search = NULL;
    }

    if(cmdPassiveSearch != NULL) {
        User::DeletePrcsdUsrCmd(cmdPassiveSearch);
        cmdPassiveSearch = NULL;
    }
                        
    PrcsdUsrCmd *next = cmdStrt;
                        
    while(next != NULL) {
        PrcsdUsrCmd *cur = next;
        next = cur->next;

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)cur->sCommand) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate cur->sCommand in User::Close\n", 0);
        }
#else
		free(cur->sCommand);
#endif
        cur->sCommand = NULL;

        delete cur;
	}
    
    cmdStrt = NULL;
    cmdEnd = NULL;
    
    PrcsdToUsrCmd *nextto = cmdToUserStrt;
                        
    while(nextto != NULL) {
        PrcsdToUsrCmd *curto = nextto;
        nextto = curto->next;

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curto->sCommand) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate curto->sCommand in User::Close\n", 0);
        }
#else
		free(curto->sCommand);
#endif
        curto->sCommand = NULL;

#ifdef _WIN32
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)curto->ToNick) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate curto->ToNick in User::Close\n", 0);
        }
#else
		free(curto->ToNick);
#endif
        curto->ToNick = NULL;

        delete curto;
	}


    cmdToUserStrt = NULL;
    cmdToUserEnd = NULL;

    if(sMyInfoLong) {
    	if(clsSettingManager::mPtr->ui8FullMyINFOOption != 2) {
    		clsUsers::mPtr->DelFromMyInfosTag(this);
        }
    }
    
    if(sMyInfoShort) {
    	if(clsSettingManager::mPtr->ui8FullMyINFOOption != 0) {
    		clsUsers::mPtr->DelFromMyInfos(this);
        }
    }

    if(sbdatalen == 0 || (ui32BoolBits & BIT_ERROR) == BIT_ERROR) {
        ui8State = STATE_REMME;
    } else {
        if(uLogInOut == NULL) {
            uLogInOut = new LoginLogout();
            if(uLogInOut == NULL) {
                ui8State = STATE_REMME;
        		AppendDebugLog("%s - [MEM] Cannot allocate new uLogInOut in User::Close\n", 0);
        		return;
            }
        }

        uLogInOut->iToCloseLoops = 100;
    }
}
//---------------------------------------------------------------------------

void User::Add2Userlist() {
    clsUsers::mPtr->Add2NickList(this);
    clsUsers::mPtr->Add2UserIP(this);
    
    switch(clsSettingManager::mPtr->ui8FullMyINFOOption) {
        case 0: {
            GenerateMyInfoLong();
            clsUsers::mPtr->Add2MyInfosTag(this);
            return;
        }
        case 1: {
            GenerateMyInfoLong();
            clsUsers::mPtr->Add2MyInfosTag(this);
            GenerateMyInfoShort();
            clsUsers::mPtr->Add2MyInfos(this);
            return;
        }
        case 2: {
            GenerateMyInfoShort();
            clsUsers::mPtr->Add2MyInfos(this);
            return;
        }
        default:
            break;
    }
}
//------------------------------------------------------------------------------

void User::AddUserList() {
    ui32BoolBits |= BIT_BIG_SEND_BUFFER;
	iLastNicklist = clsServerManager::ui64ActualTick;

	if(((ui32SupportBits & SUPPORTBIT_NOHELLO) == SUPPORTBIT_NOHELLO) == false) {
    	if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::ALLOWEDOPCHAT) == false || (clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == false ||
            (clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true && clsSettingManager::mPtr->bBotsSameNick == true))) {
            if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                SendCharDelayed(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen);
            } else {
                if(clsUsers::mPtr->iZNickListLen == 0) {
                    clsUsers::mPtr->sZNickList = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen, clsUsers::mPtr->sZNickList,
                        clsUsers::mPtr->iZNickListLen, clsUsers::mPtr->iZNickListSize, Allign16K);
                    if(clsUsers::mPtr->iZNickListLen == 0) {
                        SendCharDelayed(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen);
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZNickList, clsUsers::mPtr->iZNickListLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->nickListLen-clsUsers::mPtr->iZNickListLen;
                    }
                } else {
                    PutInSendBuf(clsUsers::mPtr->sZNickList, clsUsers::mPtr->iZNickListLen);
                    clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->nickListLen-clsUsers::mPtr->iZNickListLen;
                }
            }
        } else {
            // PPK ... OpChat bot is now visible only for OPs ;)
            int iLen = sprintf(msg, "%s$$|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
            if(CheckSprintf(iLen, 1024, "User::AddUserList") == true) {
                if(clsUsers::mPtr->nickListSize < clsUsers::mPtr->nickListLen+iLen) {
                    char * pOldBuf = clsUsers::mPtr->nickList;
#ifdef _WIN32
                    clsUsers::mPtr->nickList = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, clsUsers::mPtr->nickListSize+NICKLISTSIZE+1);
#else
					clsUsers::mPtr->nickList = (char *)realloc(pOldBuf, clsUsers::mPtr->nickListSize+NICKLISTSIZE+1);
#endif
                    if(clsUsers::mPtr->nickList == NULL) {
                        clsUsers::mPtr->nickList = pOldBuf;
                        ui32BoolBits |= BIT_ERROR;
                        Close();

						AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for nickList in User::AddUserList\n", (uint64_t)(clsUsers::mPtr->nickListSize+NICKLISTSIZE+1));

                        return;
                    }
                    clsUsers::mPtr->nickListSize += NICKLISTSIZE;
                }
    
                memcpy(clsUsers::mPtr->nickList+clsUsers::mPtr->nickListLen-1, msg, iLen);
                clsUsers::mPtr->nickList[clsUsers::mPtr->nickListLen+(iLen-1)] = '\0';
                SendCharDelayed(clsUsers::mPtr->nickList, clsUsers::mPtr->nickListLen+(iLen-1));
                clsUsers::mPtr->nickList[clsUsers::mPtr->nickListLen-1] = '|';
                clsUsers::mPtr->nickList[clsUsers::mPtr->nickListLen] = '\0';
            }
        }
	}
	
	switch(clsSettingManager::mPtr->ui8FullMyINFOOption) {
    	case 0: {
            if(clsUsers::mPtr->myInfosTagLen == 0) {
                break;
            }

            if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
            } else {
                if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                    clsUsers::mPtr->sZMyInfosTag = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen, clsUsers::mPtr->sZMyInfosTag,
                        clsUsers::mPtr->iZMyInfosTagLen, clsUsers::mPtr->iZMyInfosTagSize, Allign128K);
                    if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                        SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                    }
                } else {
                    PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                    clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                }
            }
            break;
    	}
    	case 1: {
    		if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::SENDFULLMYINFOS) == false) {
                if(clsUsers::mPtr->myInfosLen == 0) {
                    break;
                }

                if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                    SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
                } else {
                    if(clsUsers::mPtr->iZMyInfosLen == 0) {
                        clsUsers::mPtr->sZMyInfos = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen, clsUsers::mPtr->sZMyInfos,
                            clsUsers::mPtr->iZMyInfosLen, clsUsers::mPtr->iZMyInfosSize, Allign128K);
                        if(clsUsers::mPtr->iZMyInfosLen == 0) {
                            SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
                        } else {
                            PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                        }
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                    }
                }
    		} else {
                if(clsUsers::mPtr->myInfosTagLen == 0) {
                    break;
                }

                if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                    SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
                } else {
                    if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                        clsUsers::mPtr->sZMyInfosTag = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen, clsUsers::mPtr->sZMyInfosTag,
                            clsUsers::mPtr->iZMyInfosTagLen, clsUsers::mPtr->iZMyInfosTagSize, Allign128K);
                        if(clsUsers::mPtr->iZMyInfosTagLen == 0) {
                            SendCharDelayed(clsUsers::mPtr->myInfosTag, clsUsers::mPtr->myInfosTagLen);
                        } else {
                            PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                            clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                        }
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZMyInfosTag, clsUsers::mPtr->iZMyInfosTagLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosTagLen-clsUsers::mPtr->iZMyInfosTagLen;
                    }
                }
    		}
    		break;
    	}
        case 2: {
            if(clsUsers::mPtr->myInfosLen == 0) {
                break;
            }

            if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
            } else {
                if(clsUsers::mPtr->iZMyInfosLen == 0) {
                    clsUsers::mPtr->sZMyInfos = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen, clsUsers::mPtr->sZMyInfos,
                        clsUsers::mPtr->iZMyInfosLen, clsUsers::mPtr->iZMyInfosSize, Allign128K);
                    if(clsUsers::mPtr->iZMyInfosLen == 0) {
                        SendCharDelayed(clsUsers::mPtr->myInfos, clsUsers::mPtr->myInfosLen);
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                    }
                } else {
                    PutInSendBuf(clsUsers::mPtr->sZMyInfos, clsUsers::mPtr->iZMyInfosLen);
                    clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->myInfosLen-clsUsers::mPtr->iZMyInfosLen;
                }
            }
    	}
    	default:
            break;
    }
	
	if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::ALLOWEDOPCHAT) == false || (clsSettingManager::mPtr->bBools[SETBOOL_REG_OP_CHAT] == false ||
        (clsSettingManager::mPtr->bBools[SETBOOL_REG_BOT] == true && clsSettingManager::mPtr->bBotsSameNick == true))) {
        if(clsUsers::mPtr->opListLen > 9) {
            if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                SendCharDelayed(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen);
            } else {
                if(clsUsers::mPtr->iZOpListLen == 0) {
                    clsUsers::mPtr->sZOpList = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen, clsUsers::mPtr->sZOpList,
                        clsUsers::mPtr->iZOpListLen, clsUsers::mPtr->iZOpListSize, Allign16K);
                    if(clsUsers::mPtr->iZOpListLen == 0) {
                        SendCharDelayed(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen);
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZOpList, clsUsers::mPtr->iZOpListLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->opListLen-clsUsers::mPtr->iZOpListLen;
                    }
                } else {
                    PutInSendBuf(clsUsers::mPtr->sZOpList, clsUsers::mPtr->iZOpListLen);
                    clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->opListLen-clsUsers::mPtr->iZOpListLen;
                }  
            }
        }
    } else {
        // PPK ... OpChat bot is now visible only for OPs ;)
        SendCharDelayed(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO],
            clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_OP_CHAT_MYINFO]);
        int iLen = sprintf(msg, "%s$$|", clsSettingManager::mPtr->sTexts[SETTXT_OP_CHAT_NICK]);
        if(CheckSprintf(iLen, 1024, "User::AddUserList1") == true) {
            if(clsUsers::mPtr->opListSize < clsUsers::mPtr->opListLen+iLen) {
                char * pOldBuf = clsUsers::mPtr->opList;
#ifdef _WIN32
                clsUsers::mPtr->opList = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, clsUsers::mPtr->opListSize+OPLISTSIZE+1);
#else
				clsUsers::mPtr->opList = (char *)realloc(pOldBuf, clsUsers::mPtr->opListSize+OPLISTSIZE+1);
#endif
                if(clsUsers::mPtr->opList == NULL) {
                    clsUsers::mPtr->opList = pOldBuf;
                    ui32BoolBits |= BIT_ERROR;
                    Close();

                    AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes for opList in User::AddUserList\n", (uint64_t)(clsUsers::mPtr->opListSize+OPLISTSIZE+1));

                    return;
                }
                clsUsers::mPtr->opListSize += OPLISTSIZE;
            }
    
            memcpy(clsUsers::mPtr->opList+clsUsers::mPtr->opListLen-1, msg, iLen);
            clsUsers::mPtr->opList[clsUsers::mPtr->opListLen+(iLen-1)] = '\0';
            SendCharDelayed(clsUsers::mPtr->opList, clsUsers::mPtr->opListLen+(iLen-1));
            clsUsers::mPtr->opList[clsUsers::mPtr->opListLen-1] = '|';
            clsUsers::mPtr->opList[clsUsers::mPtr->opListLen] = '\0';
        }
    }

    if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::SENDALLUSERIP) == true && ((ui32SupportBits & SUPPORTBIT_USERIP2) == SUPPORTBIT_USERIP2) == true) {
        if(clsUsers::mPtr->userIPListLen > 9) {
            if(((ui32SupportBits & SUPPORTBIT_ZPIPE) == SUPPORTBIT_ZPIPE) == false) {
                SendCharDelayed(clsUsers::mPtr->userIPList, clsUsers::mPtr->userIPListLen);
            } else {
                if(clsUsers::mPtr->iZUserIPListLen == 0) {
                    clsUsers::mPtr->sZUserIPList = clsZlibUtility::mPtr->CreateZPipe(clsUsers::mPtr->userIPList, clsUsers::mPtr->userIPListLen, clsUsers::mPtr->sZUserIPList,
                        clsUsers::mPtr->iZUserIPListLen, clsUsers::mPtr->iZUserIPListSize, Allign16K);
                    if(clsUsers::mPtr->iZUserIPListLen == 0) {
                        SendCharDelayed(clsUsers::mPtr->userIPList, clsUsers::mPtr->userIPListLen);
                    } else {
                        PutInSendBuf(clsUsers::mPtr->sZUserIPList, clsUsers::mPtr->iZUserIPListLen);
                        clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->userIPListLen-clsUsers::mPtr->iZUserIPListLen;
                    }
                } else {
                    PutInSendBuf(clsUsers::mPtr->sZUserIPList, clsUsers::mPtr->iZUserIPListLen);
                    clsServerManager::ui64BytesSentSaved += clsUsers::mPtr->userIPListLen-clsUsers::mPtr->iZUserIPListLen;
                }  
            }
        }
    }
}
//---------------------------------------------------------------------------

bool User::GenerateMyInfoLong() { // true == changed
    // Prepare myinfo with nick
    int iLen = sprintf(msg, "$MyINFO $ALL %s ", sNick);
    if(CheckSprintf(iLen, 1024, "User::GenerateMyInfoLong") == false) {
        return false;
    }

    // Add description
    if(ui8ChangedDescriptionLongLen != 0) {
        if(sChangedDescriptionLong != NULL) {
            memcpy(msg+iLen, sChangedDescriptionLong, ui8ChangedDescriptionLongLen);
            iLen += ui8ChangedDescriptionLongLen;
        }

        if(((ui32InfoBits & INFOBIT_DESCRIPTION_LONG_PERM) == INFOBIT_DESCRIPTION_LONG_PERM) == false) {
            if(sChangedDescriptionLong != NULL) {
                User::FreeInfo(sChangedDescriptionLong, "sChangedDescriptionLong");
                sChangedDescriptionLong = NULL;
            }
            ui8ChangedDescriptionLongLen = 0;
        }
    } else if(sDescription != NULL) {
        memcpy(msg+iLen, sDescription, (size_t)ui8DescriptionLen);
        iLen += ui8DescriptionLen;
    }

    // Add tag
    if(ui8ChangedTagLongLen != 0) {
        if(sChangedTagLong != NULL) {
            memcpy(msg+iLen, sChangedTagLong, ui8ChangedTagLongLen);
            iLen += ui8ChangedTagLongLen;
        }

        if(((ui32InfoBits & INFOBIT_TAG_LONG_PERM) == INFOBIT_TAG_LONG_PERM) == false) {
            if(sChangedTagLong != NULL) {
                User::FreeInfo(sChangedTagLong, "sChangedTagLong");
                sChangedTagLong = NULL;
            }
            ui8ChangedTagLongLen = 0;
        }
    } else if(sTag != NULL) {
        memcpy(msg+iLen, sTag, (size_t)ui8TagLen);
        iLen += (int)ui8TagLen;
    }

    memcpy(msg+iLen, "$ $", 3);
    iLen += 3;

    // Add connection
    if(ui8ChangedConnectionLongLen != 0) {
        if(sChangedConnectionLong != NULL) {
            memcpy(msg+iLen, sChangedConnectionLong, ui8ChangedConnectionLongLen);
            iLen += ui8ChangedConnectionLongLen;
        }

        if(((ui32InfoBits & INFOBIT_CONNECTION_LONG_PERM) == INFOBIT_CONNECTION_LONG_PERM) == false) {
            if(sChangedConnectionLong != NULL) {
                User::FreeInfo(sChangedConnectionLong, "sChangedConnectionLong");
                sChangedConnectionLong = NULL;
            }
            ui8ChangedConnectionLongLen = 0;
        }
    } else if(sConnection != NULL) {
        memcpy(msg+iLen, sConnection, ui8ConnectionLen);
        iLen += ui8ConnectionLen;
    }

    // add magicbyte
    char cMagic = MagicByte;

    if((ui32BoolBits & BIT_QUACK_SUPPORTS) == BIT_QUACK_SUPPORTS) {
        cMagic &= ~0x10; // TLSv1 support
        cMagic &= ~0x20; // TLSv1 support
    }

    if((ui32BoolBits & BIT_IPV4) == BIT_IPV4) {
        cMagic |= 0x40; // IPv4 support
    } else {
        cMagic &= ~0x40; // IPv4 support
    }

    if((ui32BoolBits & BIT_IPV6) == BIT_IPV6) {
        cMagic |= 0x80; // IPv6 support
    } else {
        cMagic &= ~0x80; // IPv6 support
    }

    msg[iLen] = cMagic;
    msg[iLen+1] = '$';
    iLen += 2;

    // Add email
    if(ui8ChangedEmailLongLen != 0) {
        if(sChangedEmailLong != NULL) {
            memcpy(msg+iLen, sChangedEmailLong, ui8ChangedEmailLongLen);
            iLen += ui8ChangedEmailLongLen;
        }

        if(((ui32InfoBits & INFOBIT_EMAIL_LONG_PERM) == INFOBIT_EMAIL_LONG_PERM) == false) {
            if(sChangedEmailLong != NULL) {
                User::FreeInfo(sChangedEmailLong, "sChangedEmailLong");
                sChangedEmailLong = NULL;
            }
            ui8ChangedEmailLongLen = 0;
        }
    } else if(sEmail != NULL) {
        memcpy(msg+iLen, sEmail, (size_t)ui8EmailLen);
        iLen += (int)ui8EmailLen;
    }

    // Add share and end of myinfo
	int iRet = sprintf(msg+iLen, "$%" PRIu64 "$|", ui64ChangedSharedSizeLong);
    iLen += iRet;

    if(((ui32InfoBits & INFOBIT_SHARE_LONG_PERM) == INFOBIT_SHARE_LONG_PERM) == false) {
        ui64ChangedSharedSizeLong = ui64SharedSize;
    }

    if(CheckSprintf1(iRet, iLen, 1024, "User::GenerateMyInfoLong2") == false) {
        return false;
    }

    if(sMyInfoLong != NULL) {
        if(ui16MyInfoLongLen == (uint16_t)iLen && memcmp(sMyInfoLong+14+ui8NickLen, msg+14+ui8NickLen, ui16MyInfoLongLen-14-ui8NickLen) == 0) {
            return false;
        }
    }

    UserSetMyInfoLong(this, msg, (uint16_t)iLen);

    return true;
}
//---------------------------------------------------------------------------

bool User::GenerateMyInfoShort() { // true == changed
    // Prepare myinfo with nick
    int iLen = sprintf(msg, "$MyINFO $ALL %s ", sNick);
    if(CheckSprintf(iLen, 1024, "User::GenerateMyInfoShort") == false) {
        return false;
    }

    // Add mode to start of description if is enabled
    if(clsSettingManager::mPtr->bBools[SETBOOL_MODE_TO_DESCRIPTION] == true && sModes[0] != 0) {
        char * sActualDescription = NULL;

        if(ui8ChangedDescriptionShortLen != 0) {
            sActualDescription = sChangedDescriptionShort;
        } else if(clsSettingManager::mPtr->bBools[SETBOOL_STRIP_DESCRIPTION] == true) {
            sActualDescription = sDescription;
        }

        if(sActualDescription == NULL) {
            msg[iLen] = sModes[0];
            iLen++;
        } else if(sActualDescription[0] != sModes[0] && sActualDescription[1] != ' ') {
            msg[iLen] = sModes[0];
            msg[iLen+1] = ' ';
            iLen += 2;
        }
    }

    // Add description
    if(ui8ChangedDescriptionShortLen != 0) {
        if(sChangedDescriptionShort != NULL) {
            memcpy(msg+iLen, sChangedDescriptionShort, ui8ChangedDescriptionShortLen);
            iLen += ui8ChangedDescriptionShortLen;
        }

        if(((ui32InfoBits & INFOBIT_DESCRIPTION_SHORT_PERM) == INFOBIT_DESCRIPTION_SHORT_PERM) == false) {
            if(sChangedDescriptionShort != NULL) {
                User::FreeInfo(sChangedDescriptionShort, "sChangedDescriptionShort");
                sChangedDescriptionShort = NULL;
            }
            ui8ChangedDescriptionShortLen = 0;
        }
    } else if(clsSettingManager::mPtr->bBools[SETBOOL_STRIP_DESCRIPTION] == false && sDescription != NULL) {
        memcpy(msg+iLen, sDescription, ui8DescriptionLen);
        iLen += ui8DescriptionLen;
    }

    // Add tag
    if(ui8ChangedTagShortLen != 0) {
        if(sChangedTagShort != NULL) {
            memcpy(msg+iLen, sChangedTagShort, ui8ChangedTagShortLen);
            iLen += ui8ChangedTagShortLen;
        }

        if(((ui32InfoBits & INFOBIT_TAG_SHORT_PERM) == INFOBIT_TAG_SHORT_PERM) == false) {
            if(sChangedTagShort != NULL) {
                User::FreeInfo(sChangedTagShort, "sChangedTagShort");
                sChangedTagShort = NULL;
            }
            ui8ChangedTagShortLen = 0;
        }
    } else if(clsSettingManager::mPtr->bBools[SETBOOL_STRIP_TAG] == false && sTag != NULL) {
        memcpy(msg+iLen, sTag, (size_t)ui8TagLen);
        iLen += (int)ui8TagLen;
    }

    // Add mode to myinfo if is enabled
    if(clsSettingManager::mPtr->bBools[SETBOOL_MODE_TO_MYINFO] == true && sModes[0] != 0) {
        int iRet = sprintf(msg+iLen, "$%c$", sModes[0]);
        iLen += iRet;
        if(CheckSprintf1(iRet, iLen, 1024, "GenerateMyInfoShort1") == false) {
            return false;
        }
    } else {
        memcpy(msg+iLen, "$ $", 3);
        iLen += 3;
    }

    // Add connection
    if(ui8ChangedConnectionShortLen != 0) {
        if(sChangedConnectionShort != NULL) {
            memcpy(msg+iLen, sChangedConnectionShort, ui8ChangedConnectionShortLen);
            iLen += ui8ChangedConnectionShortLen;
        }

        if(((ui32InfoBits & INFOBIT_CONNECTION_SHORT_PERM) == INFOBIT_CONNECTION_SHORT_PERM) == false) {
            if(sChangedConnectionShort != NULL) {
                User::FreeInfo(sChangedConnectionShort, "sChangedConnectionShort");
                sChangedConnectionShort = NULL;
            }
            ui8ChangedConnectionShortLen = 0;
        }
    } else if(clsSettingManager::mPtr->bBools[SETBOOL_STRIP_CONNECTION] == false && sConnection != NULL) {
        memcpy(msg+iLen, sConnection, ui8ConnectionLen);
        iLen += ui8ConnectionLen;
    }

    // add magicbyte
    char cMagic = MagicByte;

    if((ui32BoolBits & BIT_QUACK_SUPPORTS) == BIT_QUACK_SUPPORTS) {
        cMagic &= ~0x10; // TLSv1 support
        cMagic &= ~0x20; // TLSv1 support
    }

    if((ui32BoolBits & BIT_IPV4) == BIT_IPV4) {
        cMagic |= 0x40; // IPv4 support
    } else {
        cMagic &= ~0x40; // IPv4 support
    }

    if((ui32BoolBits & BIT_IPV6) == BIT_IPV6) {
        cMagic |= 0x80; // IPv6 support
    } else {
        cMagic &= ~0x80; // IPv6 support
    }

    msg[iLen] = cMagic;
    msg[iLen+1] = '$';
    iLen += 2;

    // Add email
    if(ui8ChangedEmailShortLen != 0) {
        if(sChangedEmailShort != NULL) {
            memcpy(msg+iLen, sChangedEmailShort, ui8ChangedEmailShortLen);
            iLen += ui8ChangedEmailShortLen;
        }

        if(((ui32InfoBits & INFOBIT_EMAIL_SHORT_PERM) == INFOBIT_EMAIL_SHORT_PERM) == false) {
            if(sChangedEmailShort != NULL) {
                User::FreeInfo(sChangedEmailShort, "sChangedEmailShort");
                sChangedEmailShort = NULL;
            }
            ui8ChangedEmailShortLen = 0;
        }
    } else if(clsSettingManager::mPtr->bBools[SETBOOL_STRIP_EMAIL] == false && sEmail != NULL) {
        memcpy(msg+iLen, sEmail, (size_t)ui8EmailLen);
        iLen += (int)ui8EmailLen;
    }

    // Add share and end of myinfo
	int iRet = sprintf(msg+iLen, "$%" PRIu64 "$|", ui64ChangedSharedSizeShort);
    iLen += iRet;

    if(((ui32InfoBits & INFOBIT_SHARE_SHORT_PERM) == INFOBIT_SHARE_SHORT_PERM) == false) {
        ui64ChangedSharedSizeShort = ui64SharedSize;
    }

    if(CheckSprintf1(iRet, iLen, 1024, "User::GenerateMyInfoShort2") == false) {
        return false;
    }

    if(sMyInfoShort != NULL) {
        if(ui16MyInfoShortLen == (uint16_t)iLen && memcmp(sMyInfoShort+14+ui8NickLen, msg+14+ui8NickLen, ui16MyInfoShortLen-14-ui8NickLen) == 0) {
            return false;
        }
    }

    UserSetMyInfoShort(this, msg, (uint16_t)iLen);

    return true;
}
//---------------------------------------------------------------------------

#ifdef _WIN32
void User::FreeInfo(char * sInfo, const char * sName) {
	if(sInfo != NULL) {
		if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sInfo) == 0) {
			AppendDebugLog(("%s - [MEM] Cannot deallocate "+string(sName)+" in User::FreeInfo\n").c_str(), 0);
        }
    }
#else
void User::FreeInfo(char * sInfo, const char */* sName*/) {
	free(sInfo);
#endif
}
//------------------------------------------------------------------------------

void User::HasSuspiciousTag() {
	if(clsSettingManager::mPtr->bBools[SETBOOL_REPORT_SUSPICIOUS_TAG] == true && clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES] == true) {
		if(clsSettingManager::mPtr->bBools[SETBOOL_SEND_STATUS_MESSAGES_AS_PM] == true) {
			int imsgLen = sprintf(msg, "%s $<%s> *** %s (%s) %s. %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC],
                sNick, sIP, clsLanguageManager::mPtr->sTexts[LAN_HAS_SUSPICIOUS_TAG_CHECK_HIM], clsLanguageManager::mPtr->sTexts[LAN_FULL_DESCRIPTION]);
            if(CheckSprintf(imsgLen, 1024, "UserHasSuspiciousTag1") == true) {
                memcpy(msg+imsgLen, sDescription, (size_t)ui8DescriptionLen);
                imsgLen += (int)ui8DescriptionLen;
                msg[imsgLen] = '|';
                clsGlobalDataQueue::mPtr->SingleItemStore(msg, imsgLen+1, NULL, 0, clsGlobalDataQueue::SI_PM2OPS);
            }
		} else {
			int imsgLen = sprintf(msg, "<%s> *** %s (%s) %s. %s: ", clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SEC], sNick, sIP,
                clsLanguageManager::mPtr->sTexts[LAN_HAS_SUSPICIOUS_TAG_CHECK_HIM], clsLanguageManager::mPtr->sTexts[LAN_FULL_DESCRIPTION]);
            if(CheckSprintf(imsgLen, 1024, "UserHasSuspiciousTag2") == true) {
                memcpy(msg+imsgLen, sDescription, (size_t)ui8DescriptionLen);
                imsgLen += (int)ui8DescriptionLen;
                msg[imsgLen] = '|';
                clsGlobalDataQueue::mPtr->AddQueueItem(msg, imsgLen+1, NULL, 0, clsGlobalDataQueue::CMD_OPS);
            }
	   }
    }
    ui32BoolBits &= ~BIT_HAVE_BADTAG;
}
//---------------------------------------------------------------------------

bool User::ProcessRules() {
    // if share limit enabled, check it
    if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NOSHARELIMIT) == false) {      
        if((clsSettingManager::mPtr->ui64MinShare != 0 && ui64SharedSize < clsSettingManager::mPtr->ui64MinShare) ||
            (clsSettingManager::mPtr->ui64MaxShare != 0 && ui64SharedSize > clsSettingManager::mPtr->ui64MaxShare)) {
            SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_SHARE_LIMIT_MSG], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_SHARE_LIMIT_MSG]);
            //clsUdpDebug::mPtr->Broadcast("[SYS] User with low or high share " + Nick + " (" + IP + ") disconnected.");
            return false;
        }
    }
    
    // no Tag? Apply rule
    if(sTag == NULL) {
        if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NOTAGCHECK) == false) {
            if(clsSettingManager::mPtr->iShorts[SETSHORT_NO_TAG_OPTION] != 0) {
                SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_NO_TAG_MSG], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_NO_TAG_MSG]);
                //clsUdpDebug::mPtr->Broadcast("[SYS] User without Tag " + Nick + (" + IP + ") redirected.");
                return false;
            }
        }
    } else {
        // min and max slot check
        if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NOSLOTCHECK) == false) {
            // TODO 2 -oPTA -ccheckers: $SR based slots fetching for no_tag users
        
			if((clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SLOTS_LIMIT] != 0 && Slots < (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MIN_SLOTS_LIMIT]) ||
				(clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SLOTS_LIMIT] != 0 && Slots > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_SLOTS_LIMIT])) {
                SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_SLOTS_LIMIT_MSG], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_SLOTS_LIMIT_MSG]);
                //clsUdpDebug::mPtr->Broadcast("[SYS] User with bad slots " + Nick + " (" + IP + ") disconnected.");
                return false;
            }
        }
    
        // slots/hub ration check
        if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NOSLOTHUBRATIO) == false && 
            clsSettingManager::mPtr->iShorts[SETSHORT_HUB_SLOT_RATIO_HUBS] != 0 && clsSettingManager::mPtr->iShorts[SETSHORT_HUB_SLOT_RATIO_SLOTS] != 0) {
            uint32_t slots = Slots;
            uint32_t hubs = Hubs > 0 ? Hubs : 1;
        	if(((double)slots / hubs) < ((double)clsSettingManager::mPtr->iShorts[SETSHORT_HUB_SLOT_RATIO_SLOTS] / clsSettingManager::mPtr->iShorts[SETSHORT_HUB_SLOT_RATIO_HUBS])) {
        	    SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_HUB_SLOT_RATIO_MSG], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_HUB_SLOT_RATIO_MSG]);
                //clsUdpDebug::mPtr->Broadcast("[SYS] User with bad hub/slot ratio " + Nick + " (" + IP + ") disconnected.");
                return false;
            }
        }
    
        // hub checker
        if(clsProfileManager::mPtr->IsAllowed(this, clsProfileManager::NOMAXHUBCHECK) == false && clsSettingManager::mPtr->iShorts[SETSHORT_MAX_HUBS_LIMIT] != 0) {
            if(Hubs > (uint32_t)clsSettingManager::mPtr->iShorts[SETSHORT_MAX_HUBS_LIMIT]) {
                SendChar(clsSettingManager::mPtr->sPreTexts[clsSettingManager::SETPRETXT_MAX_HUBS_LIMIT_MSG], clsSettingManager::mPtr->ui16PreTextsLens[clsSettingManager::SETPRETXT_MAX_HUBS_LIMIT_MSG]);
                //clsUdpDebug::mPtr->Broadcast("[SYS] User with bad hubs count " + Nick + " (" + IP + ") disconnected.");
                return false;
            }
        }
    }
    
    return true;
}

//------------------------------------------------------------------------------

void User::AddPrcsdCmd(const unsigned char &cType, char * sCommand, const size_t &szCommandLen, User * to, const bool &bIsPm/* = false*/) {
    if(cType == PrcsdUsrCmd::CTM_MCTM_RCTM_SR_TO) {
        PrcsdToUsrCmd *next = cmdToUserStrt;
        while(next != NULL) {
            PrcsdToUsrCmd *cur = next;
            next = cur->next;
            if(cur->To == to) {
                char * pOldBuf = cur->sCommand;
#ifdef _WIN32
                cur->sCommand = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldBuf, cur->iLen+szCommandLen+1);
#else
				cur->sCommand = (char *)realloc(pOldBuf, cur->iLen+szCommandLen+1);
#endif
                if(cur->sCommand == NULL) {
                    cur->sCommand = pOldBuf;
                    ui32BoolBits |= BIT_ERROR;
                    Close();

					AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in User::AddPrcsdCmd\n", (uint64_t)(cur->iLen+szCommandLen+1));

                    return;
                }
                memcpy(cur->sCommand+cur->iLen, sCommand, szCommandLen);
                cur->sCommand[cur->iLen+szCommandLen] = '\0';
                cur->iLen += (uint32_t)szCommandLen;
                cur->iPmCount += bIsPm == true ? 1 : 0;
                return;
            }
        }

        PrcsdToUsrCmd * pNewcmd = new PrcsdToUsrCmd();
        if(pNewcmd == NULL) {
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] User::AddPrcsdCmd cannot allocate new pNewcmd\n", 0);

        	return;
        }

#ifdef _WIN32
        pNewcmd->sCommand = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szCommandLen+1);
#else
		pNewcmd->sCommand = (char *)malloc(szCommandLen+1);
#endif
        if(pNewcmd->sCommand == NULL) {
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sCommand in User::AddPrcsdCmd\n", (uint64_t)(szCommandLen+1));

            return;
        }

        memcpy(pNewcmd->sCommand, sCommand, szCommandLen);
        pNewcmd->sCommand[szCommandLen] = '\0';

        pNewcmd->iLen = (uint32_t)szCommandLen;
        pNewcmd->iPmCount = bIsPm == true ? 1 : 0;
        pNewcmd->iLoops = 0;
        pNewcmd->To = to;
        
#ifdef _WIN32
        pNewcmd->ToNick = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, to->ui8NickLen+1);
#else
		pNewcmd->ToNick = (char *)malloc(to->ui8NickLen+1);
#endif
        if(pNewcmd->ToNick == NULL) {
            ui32BoolBits |= BIT_ERROR;
            Close();

			AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for ToNick in User::AddPrcsdCmd\n", (uint64_t)(to->ui8NickLen+1));

            return;
        }   

        memcpy(pNewcmd->ToNick, to->sNick, to->ui8NickLen);
        pNewcmd->ToNick[to->ui8NickLen] = '\0';
        
        pNewcmd->iToNickLen = to->ui8NickLen;
        pNewcmd->next = NULL;
               
        if(cmdToUserStrt == NULL) {
            cmdToUserStrt = pNewcmd;
            cmdToUserEnd = pNewcmd;
        } else {
            cmdToUserEnd->next = pNewcmd;
            cmdToUserEnd = pNewcmd;
        }
        return;
    }
    
    PrcsdUsrCmd * pNewcmd = new PrcsdUsrCmd();
    if(pNewcmd == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] User::AddPrcsdCmd cannot allocate new pNewcmd1\n", 0);

    	return;
    }

#ifdef _WIN32
    pNewcmd->sCommand = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szCommandLen+1);
#else
	pNewcmd->sCommand = (char *)malloc(szCommandLen+1);
#endif
    if(pNewcmd->sCommand == NULL) {
        ui32BoolBits |= BIT_ERROR;
        Close();

		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes for sCommand in User::AddPrcsdCmd1\n", (uint64_t)(szCommandLen+1));

        return;
    }

    memcpy(pNewcmd->sCommand, sCommand, szCommandLen);
    pNewcmd->sCommand[szCommandLen] = '\0';

    pNewcmd->iLen = (uint32_t)szCommandLen;
    pNewcmd->cType = cType;
    pNewcmd->next = NULL;
    pNewcmd->ptr = (void *)to;

    if(cmdStrt == NULL) {
        cmdStrt = pNewcmd;
        cmdEnd = pNewcmd;
    } else {
        cmdEnd->next = pNewcmd;
        cmdEnd = pNewcmd;
    }
}
//---------------------------------------------------------------------------

void User::AddMeOrIPv4Check() {
    if(((ui32BoolBits & BIT_IPV6) == BIT_IPV6) && ((ui32SupportBits & SUPPORTBIT_IPV4) == SUPPORTBIT_IPV4) && clsServerManager::sHubIP[0] != '\0' && clsServerManager::bUseIPv4 == true) {
        ui8State = STATE_IPV4_CHECK;

        int imsgLen = sprintf(msg, "$ConnectToMe %s %s:%s|", sNick, clsServerManager::sHubIP, string(clsSettingManager::mPtr->iPortNumbers[0]).c_str());
        if(CheckSprintf(imsgLen, 1024, "User::AddMeOrIPv4Check") == false) {
            return;
        }

        uLogInOut->ui64IPv4CheckTick = clsServerManager::ui64ActualTick;

        SendCharDelayed(msg, imsgLen);
    } else {
        ui8State = STATE_ADDME;
    }
}
//---------------------------------------------------------------------------

char * User::SetUserInfo(char * sOldData, uint8_t &ui8OldDataLen, char * sNewData, size_t &sz8NewDataLen, const char * sDataName) {
    if(sOldData != NULL) {
        User::FreeInfo(sOldData, sDataName);
        sOldData = NULL;
        ui8OldDataLen = 0;
    }

    if(sz8NewDataLen > 0) {
#ifdef _WIN32
        sOldData = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, sz8NewDataLen+1);
#else
        sOldData = (char *)malloc(sz8NewDataLen+1);
#endif
        if(sOldData == NULL) {
            AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes in User::SetUserInfo\n", (uint64_t)(sz8NewDataLen+1));
            return sOldData;
        }

        memcpy(sOldData, sNewData, sz8NewDataLen);
        sOldData[sz8NewDataLen] = '\0';
        ui8OldDataLen = (uint8_t)sz8NewDataLen;
    } else {
        ui8OldDataLen = 1;
    }

    return sOldData;
}
//---------------------------------------------------------------------------

void User::RemFromSendBuf(const char * sData, const uint32_t &iLen, const uint32_t &iSbLen) {
	char *match = strstr(sendbuf+iSbLen, sData);
    if(match != NULL) {
        memmove(match, match+iLen, sbdatalen-((match+(iLen))-sendbuf));
        sbdatalen -= iLen;
        sendbuf[sbdatalen] = '\0';
    }
}
//------------------------------------------------------------------------------

void User::DeletePrcsdUsrCmd(PrcsdUsrCmd * pCommand) {
#ifdef _WIN32
    if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pCommand->sCommand) == 0) {
        AppendDebugLog("%s - [MEM] Cannot deallocate pCommand->sCommand in User::DeletePrcsdUsrCmd\n", 0);
    }
#else
    free(pCommand->sCommand);
#endif
    delete pCommand;
}
