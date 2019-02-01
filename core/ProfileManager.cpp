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
#include "ProfileManager.h"
//---------------------------------------------------------------------------
#include "colUsers.h"
#include "hashRegManager.h"
#include "LanguageManager.h"
#include "ServerManager.h"
#include "UdpDebug.h"
#include "User.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
#ifdef _BUILD_GUI
    #include "../gui.win/ProfilesDialog.h"
    #include "../gui.win/RegisteredUserDialog.h"
    #include "../gui.win/RegisteredUsersDialog.h"
#endif
//---------------------------------------------------------------------------
clsProfileManager * clsProfileManager::mPtr = NULL;
//---------------------------------------------------------------------------

ProfileItem::ProfileItem() {
    sName = NULL;

	for(uint16_t ui16i = 0; ui16i < 256; ui16i++) {
        bPermissions[ui16i] = false;
    }
}
//---------------------------------------------------------------------------

ProfileItem::~ProfileItem() {
#ifdef _WIN32
    if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sName) == 0) {
        AppendDebugLog("%s - [MEM] Cannot deallocate sName in ProfileItem::~ProfileItem\n", 0);
    }
#else
	free(sName);
#endif
}
//---------------------------------------------------------------------------

clsProfileManager::clsProfileManager() {
    iProfileCount = 0;

    ProfilesTable = NULL;

#ifdef _WIN32
    TiXmlDocument doc((clsServerManager::sPath+"\\cfg\\Profiles.xml").c_str());
#else
	TiXmlDocument doc((clsServerManager::sPath+"/cfg/Profiles.xml").c_str());
#endif
	if(doc.LoadFile() == false) {
		CreateDefaultProfiles();
		if(doc.LoadFile() == false) {
#ifdef _BUILD_GUI
            ::MessageBox(NULL, clsLanguageManager::mPtr->sTexts[LAN_PROFILES_LOAD_FAIL], clsServerManager::sTitle.c_str(), MB_OK | MB_ICONERROR);
#else
			AppendLog(clsLanguageManager::mPtr->sTexts[LAN_PROFILES_LOAD_FAIL]);
#endif
			exit(EXIT_FAILURE);
		}
	}

	TiXmlHandle cfg(&doc);
	TiXmlNode *profiles = cfg.FirstChild("Profiles").Node();
	if(profiles != NULL) {
		TiXmlNode *child = NULL;
		while((child = profiles->IterateChildren(child)) != NULL) {
			TiXmlNode *profile = child->FirstChild("Name");

			if(profile == NULL || (profile = profile->FirstChild()) == NULL) {
				continue;
			}

			const char *sName = profile->Value();

			if((profile = child->FirstChildElement("Permissions")) == NULL ||
				(profile = profile->FirstChild()) == NULL) {
				continue;
			}

			const char *sRights = profile->Value();

			ProfileItem * pNewProfile = CreateProfile(sName);

			if(strlen(sRights) == 32) {
				for(uint8_t ui8i = 0; ui8i < 32; ui8i++) {
					if(sRights[ui8i] == '1') {
						pNewProfile->bPermissions[ui8i] = true;
					} else {
						pNewProfile->bPermissions[ui8i] = false;
					}
				}
			} else if(strlen(sRights) == 256) {
				for(uint16_t ui8i = 0; ui8i < 256; ui8i++) {
					if(sRights[ui8i] == '1') {
						pNewProfile->bPermissions[ui8i] = true;
					} else {
						pNewProfile->bPermissions[ui8i] = false;
					}
				}
			} else {
				delete pNewProfile;
				continue;
			}
		}
	} else {
#ifdef _BUILD_GUI
		::MessageBox(NULL, clsLanguageManager::mPtr->sTexts[LAN_PROFILES_LOAD_FAIL], clsServerManager::sTitle.c_str(), MB_OK | MB_ICONERROR);
#else
		AppendLog(clsLanguageManager::mPtr->sTexts[LAN_PROFILES_LOAD_FAIL]);
#endif
		exit(EXIT_FAILURE);
	}
}
//---------------------------------------------------------------------------

clsProfileManager::~clsProfileManager() {
    SaveProfiles();

    for(uint16_t ui16i = 0; ui16i < iProfileCount; ui16i++) {
        delete ProfilesTable[ui16i];
    }

#ifdef _WIN32
    if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ProfilesTable) == 0) {
        AppendDebugLog("%s - [MEM] Cannot deallocate ProfilesTable in clsProfileManager::~clsProfileManager\n", 0);
    }
#else
	free(ProfilesTable);
#endif
    ProfilesTable = NULL;
}
//---------------------------------------------------------------------------

void clsProfileManager::CreateDefaultProfiles() {
    const char* profilenames[] = { "Master", "Operator", "VIP", "Reg" };
    const char* profilepermisions[] = {
        "1001111111111111111111111111111111111111111110100011111100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "1001111110111111111001100011111111100000001110100011111100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "0000000000000001111000000000000110000000000000000000011100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "0000000000000000000000000000000110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    };

#ifdef _WIN32
    TiXmlDocument doc((clsServerManager::sPath+"\\cfg\\Profiles.xml").c_str());
#else
	TiXmlDocument doc((clsServerManager::sPath+"/cfg/Profiles.xml").c_str());
#endif
    doc.InsertEndChild(TiXmlDeclaration("1.0", "windows-1252", "yes"));
    TiXmlElement profiles("Profiles");

    for(uint8_t i = 0; i < 4;i++) {
        TiXmlElement name("Name");
        name.InsertEndChild(TiXmlText(profilenames[i]));
        
        TiXmlElement permisions("Permissions");
        permisions.InsertEndChild(TiXmlText(profilepermisions[i]));
        
        TiXmlElement profile("Profile");
        profile.InsertEndChild(name);
        profile.InsertEndChild(permisions);
        
        profiles.InsertEndChild(profile);
    }

    doc.InsertEndChild(profiles);
    doc.SaveFile();
}
//---------------------------------------------------------------------------

void clsProfileManager::SaveProfiles() {
    char permisionsbits[257];

#ifdef _WIN32
    TiXmlDocument doc((clsServerManager::sPath+"\\cfg\\Profiles.xml").c_str());
#else
	TiXmlDocument doc((clsServerManager::sPath+"/cfg/Profiles.xml").c_str());
#endif
    doc.InsertEndChild(TiXmlDeclaration("1.0", "windows-1252", "yes"));
    TiXmlElement profiles("Profiles");

    for(uint16_t ui16j = 0; ui16j < iProfileCount; ui16j++) {
        TiXmlElement name("Name");
        name.InsertEndChild(TiXmlText(ProfilesTable[ui16j]->sName));
        
        for(uint16_t ui16i = 0; ui16i < 256; ui16i++) {
            if(ProfilesTable[ui16j]->bPermissions[ui16i] == false) {
                permisionsbits[ui16i] = '0';
            } else {
                permisionsbits[ui16i] = '1';
            }
        }
        permisionsbits[256] = '\0';
        
        TiXmlElement permisions("Permissions");
        permisions.InsertEndChild(TiXmlText(permisionsbits));
        
        TiXmlElement profile("Profile");
        profile.InsertEndChild(name);
        profile.InsertEndChild(permisions);
        
        profiles.InsertEndChild(profile);
    }

    doc.InsertEndChild(profiles);
    doc.SaveFile();
}
//---------------------------------------------------------------------------

bool clsProfileManager::IsAllowed(User * u, const uint32_t &iOption) const {
    // profile number -1 = normal user/no profile assigned
    if(u->iProfile == -1)
        return false;
        
    // return right of the profile
    return ProfilesTable[u->iProfile]->bPermissions[iOption];
}
//---------------------------------------------------------------------------

bool clsProfileManager::IsProfileAllowed(const int32_t &iProfile, const uint32_t &iOption) const {
    // profile number -1 = normal user/no profile assigned
    if(iProfile == -1)
        return false;
        
    // return right of the profile
    return ProfilesTable[iProfile]->bPermissions[iOption];
}
//---------------------------------------------------------------------------

int32_t clsProfileManager::AddProfile(char * name) {
    for(uint16_t ui16i = 0; ui16i < iProfileCount; ui16i++) {
		if(strcasecmp(ProfilesTable[ui16i]->sName, name) == 0) {
            return -1;
        }
    }

    uint32_t ui32j = 0;
    while(true) {
        switch(name[ui32j]) {
            case '\0':
                break;
            case '|':
                return -2;
            default:
                if(name[ui32j] < 33) {
                    return -2;
                }
                
                ui32j++;
                continue;
        }

        break;
    }

    CreateProfile(name);

#ifdef _BUILD_GUI
    if(clsProfilesDialog::mPtr != NULL) {
        clsProfilesDialog::mPtr->AddProfile();
    }
#endif

#ifdef _BUILD_GUI
    if(clsRegisteredUserDialog::mPtr != NULL) {
        clsRegisteredUserDialog::mPtr->UpdateProfiles();
    }
#endif

    return (int32_t)(iProfileCount-1);
}
//---------------------------------------------------------------------------

int32_t clsProfileManager::GetProfileIndex(const char * name) {
    for(uint16_t ui16i = 0; ui16i < iProfileCount; ui16i++) {
		if(strcasecmp(ProfilesTable[ui16i]->sName, name) == 0) {
            return ui16i;
        }
    }
    
    return -1;
}
//---------------------------------------------------------------------------

// RemoveProfileByName(name)
// returns: 0 if the name doesnot exists or is a default profile idx 0-3
//          -1 if the profile is in use
//          1 on success
int32_t clsProfileManager::RemoveProfileByName(char * name) {
    for(uint16_t ui16i = 0; ui16i < iProfileCount; ui16i++) {
		if(strcasecmp(ProfilesTable[ui16i]->sName, name) == 0) {
            return (RemoveProfile(ui16i) == true ? 1 : -1);
        }
    }
    
    return 0;
}

//---------------------------------------------------------------------------

bool clsProfileManager::RemoveProfile(const uint16_t &iProfile) {
    RegUser *next = clsRegManager::mPtr->RegListS;
    while(next != NULL) {
        RegUser *curReg = next;
		next = curReg->next;
		if(curReg->ui16Profile == iProfile) {
            //Profile in use can't be deleted!
            return false;
        }
    }
    
    iProfileCount--;

#ifdef _BUILD_GUI
    if(clsProfilesDialog::mPtr != NULL) {
        clsProfilesDialog::mPtr->RemoveProfile(iProfile);
    }
#endif
    
    delete ProfilesTable[iProfile];
    
	for(uint16_t ui16i = iProfile; ui16i < iProfileCount; ui16i++) {
        ProfilesTable[ui16i] = ProfilesTable[ui16i+1];
    }

    // Update profiles for online users
    if(clsServerManager::bServerRunning == true) {
        User *nextUser = clsUsers::mPtr->llist;
        while(nextUser != NULL) {
            User *curUser = nextUser;
            nextUser = curUser->next;
            
            if(curUser->iProfile > iProfile) {
                curUser->iProfile--;
            }
        }
    }

    // Update profiles for registered users
    next = clsRegManager::mPtr->RegListS;
    while(next != NULL) {
        RegUser *curReg = next;
		next = curReg->next;
        if(curReg->ui16Profile > iProfile) {
            curReg->ui16Profile--;
        }
    }

    ProfileItem ** pOldTable = ProfilesTable;
#ifdef _WIN32
    ProfilesTable = (ProfileItem **)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldTable, iProfileCount*sizeof(ProfileItem *));
#else
	ProfilesTable = (ProfileItem **)realloc(pOldTable, iProfileCount*sizeof(ProfileItem *));
#endif
    if(ProfilesTable == NULL) {
        ProfilesTable = pOldTable;

		AppendDebugLog("%s - [MEM] Cannot reallocate ProfilesTable in clsProfileManager::RemoveProfile\n", 0);
    }

#ifdef _BUILD_GUI
    if(clsRegisteredUserDialog::mPtr != NULL) {
        clsRegisteredUserDialog::mPtr->UpdateProfiles();
    }

	if(clsRegisteredUsersDialog::mPtr != NULL) {
		clsRegisteredUsersDialog::mPtr->UpdateProfiles();
	}
#endif

    return true;
}
//---------------------------------------------------------------------------

ProfileItem * clsProfileManager::CreateProfile(const char * name) {
    ProfileItem ** pOldTable = ProfilesTable;
#ifdef _WIN32
    if(ProfilesTable == NULL) {
        ProfilesTable = (ProfileItem **)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (iProfileCount+1)*sizeof(ProfileItem *));
    } else {
        ProfilesTable = (ProfileItem **)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldTable, (iProfileCount+1)*sizeof(ProfileItem *));
    }
#else
	ProfilesTable = (ProfileItem **)realloc(pOldTable, (iProfileCount+1)*sizeof(ProfileItem *));
#endif
    if(ProfilesTable == NULL) {
#ifdef _WIN32
        HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)pOldTable);
#else
        free(pOldTable);
#endif
		AppendDebugLog("%s - [MEM] Cannot (re)allocate ProfilesTable in clsProfileManager::CreateProfile\n", 0);
        exit(EXIT_FAILURE);
    }

    ProfileItem * pNewProfile = new ProfileItem();
    if(pNewProfile == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate pNewProfile in clsProfileManager::CreateProfile\n", 0);
        exit(EXIT_FAILURE);
    }
 
    size_t szLen = strlen(name);
#ifdef _WIN32
    pNewProfile->sName = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szLen+1);
#else
	pNewProfile->sName = (char *)malloc(szLen+1);
#endif
    if(pNewProfile->sName == NULL) {
		AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes in clsProfileManager::CreateProfile for pNewProfile->sName\n", (uint64_t)szLen);

        exit(EXIT_FAILURE);
    } 
    memcpy(pNewProfile->sName, name, szLen);
    pNewProfile->sName[szLen] = '\0';

    for(uint16_t ui16i = 0; ui16i < 256; ui16i++) {
        pNewProfile->bPermissions[ui16i] = false;
    }
    
    iProfileCount++;

    ProfilesTable[iProfileCount-1] = pNewProfile;

    return pNewProfile;
}
//---------------------------------------------------------------------------

void clsProfileManager::MoveProfileDown(const uint16_t &iProfile) {
    ProfileItem *first = ProfilesTable[iProfile];
    ProfileItem *second = ProfilesTable[iProfile+1];
    
    ProfilesTable[iProfile+1] = first;
    ProfilesTable[iProfile] = second;

    RegUser *nextReg = clsRegManager::mPtr->RegListS;

	while(nextReg != NULL) {
        RegUser *curReg = nextReg;
		nextReg = curReg->next;

		if(curReg->ui16Profile == iProfile) {
			curReg->ui16Profile++;
		} else if(curReg->ui16Profile == iProfile+1) {
			curReg->ui16Profile--;
		}
	}

#ifdef _BUILD_GUI
    if(clsProfilesDialog::mPtr != NULL) {
        clsProfilesDialog::mPtr->MoveDown(iProfile);
    }

    if(clsRegisteredUserDialog::mPtr != NULL) {
        clsRegisteredUserDialog::mPtr->UpdateProfiles();
    }

	if(clsRegisteredUsersDialog::mPtr != NULL) {
		clsRegisteredUsersDialog::mPtr->UpdateProfiles();
	}
#endif

    if(clsUsers::mPtr == NULL) {
        return;
    }

    User *nextUser = clsUsers::mPtr->llist;

	while(nextUser != NULL) {
        User *curUser = nextUser;
		nextUser = curUser->next;

		if(curUser->iProfile == (int32_t)iProfile) {
			curUser->iProfile++;
		} else if(curUser->iProfile == (int32_t)(iProfile+1)) {
			curUser->iProfile--;
		}
    }
}
//---------------------------------------------------------------------------

void clsProfileManager::MoveProfileUp(const uint16_t &iProfile) {
    ProfileItem *first = ProfilesTable[iProfile];
    ProfileItem *second = ProfilesTable[iProfile-1];
    
    ProfilesTable[iProfile-1] = first;
    ProfilesTable[iProfile] = second;

	RegUser *nextReg = clsRegManager::mPtr->RegListS;

	while(nextReg != NULL) {
		RegUser *curReg = nextReg;
		nextReg = curReg->next;

        if(curReg->ui16Profile == iProfile) {
			curReg->ui16Profile--;
		} else if(curReg->ui16Profile == iProfile-1) {
			curReg->ui16Profile++;
		}
	}

#ifdef _BUILD_GUI
    if(clsProfilesDialog::mPtr != NULL) {
        clsProfilesDialog::mPtr->MoveUp(iProfile);
    }

    if(clsRegisteredUserDialog::mPtr != NULL) {
        clsRegisteredUserDialog::mPtr->UpdateProfiles();
    }

	if(clsRegisteredUsersDialog::mPtr != NULL) {
		clsRegisteredUsersDialog::mPtr->UpdateProfiles();
	}
#endif

    if(clsUsers::mPtr == NULL) {
        return;
    }

    User *nextUser = clsUsers::mPtr->llist;

    while(nextUser != NULL) {
        User *curUser = nextUser;
		nextUser = curUser->next;

		if(curUser->iProfile == (int32_t)iProfile) {
			curUser->iProfile--;
		} else if(curUser->iProfile == (int32_t)(iProfile-1)) {
			curUser->iProfile++;
		}
    }
}
//---------------------------------------------------------------------------

void clsProfileManager::ChangeProfileName(const uint16_t &iProfile, char * sName, const size_t &szLen) {
    char * sOldName = ProfilesTable[iProfile]->sName;

#ifdef _WIN32
    ProfilesTable[iProfile]->sName = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldName, szLen+1);
#else
	ProfilesTable[iProfile]->sName = (char *)realloc(sOldName, szLen+1);
#endif
    if(ProfilesTable[iProfile]->sName == NULL) {
        ProfilesTable[iProfile]->sName = sOldName;

		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsProfileManager::ChangeProfileName for ProfilesTable[iProfile]->sName\n", (uint64_t)szLen);

        return;
    } 
	memcpy(ProfilesTable[iProfile]->sName, sName, szLen);
    ProfilesTable[iProfile]->sName[szLen] = '\0';

#ifdef _BUILD_GUI
    if(clsRegisteredUserDialog::mPtr != NULL) {
        clsRegisteredUserDialog::mPtr->UpdateProfiles();
    }

	if(clsRegisteredUsersDialog::mPtr != NULL) {
		clsRegisteredUsersDialog::mPtr->UpdateProfiles();
	}
#endif
}
//---------------------------------------------------------------------------

void clsProfileManager::ChangeProfilePermission(const uint16_t &iProfile, const size_t &szId, const bool &bValue) {
    ProfilesTable[iProfile]->bPermissions[szId] = bValue;
}
//---------------------------------------------------------------------------
