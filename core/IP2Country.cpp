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

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "stdinc.h"
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "IP2Country.h"
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "ServerManager.h"
#include "utility.h"
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
clsIpP2Country * clsIpP2Country::mPtr = NULL;
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

static const char * CountryNames[] = { "Andorra", "United Arab Emirates", "Afghanistan", "Antigua and Barbuda", "Anguilla", "Albania", "Armenia", "Angola", "Antarctica", "Argentina",
    "American Samoa", "Austria", "Australia", "Aruba", "Aland Islands", "Azerbaijan", "Bosnia and Herzegovina", "Barbados", "Bangladesh", "Belgium", "Burkina Faso", "Bulgaria", "Bahrain",
    "Burundi", "Benin", "Saint Barthelemy", "Bermuda", "Brunei Darussalam", "Bolivia", "Bonaire", "Brazil", "Bahamas", "Bhutan", "Bouvet Island", "Botswana", "Belarus", "Belize", "Canada",
    "Cocos (Keeling) Islands", "The Democratic Republic of the Congo", "Central African Republic", "Congo", "Cote D'Ivoire", "Cook Islands", "Chile", "Cameroon", "China", "Colombia",
    "Costa Rica", "Cuba", "Cape Verde", "Curacao", "Christmas Island", "Cyprus", "Czech Republic", "Germany", "Djibouti", "Denmark", "Dominica", "Dominican Republic", "Algeria", "Ecuador",
    "Estonia", "Egypt", "Western Sahara", "Eritrea", "Spain", "Ethiopia", "Finland", "Fiji", "Falkland Islands (Malvinas)", "Micronesia", "Faroe Islands", "France", "Gabon", "United Kingdom",
    "Grenada", "Georgia", "French Guiana", "Guernsey", "Ghana", "Gibraltar", "Greenland", "Gambia", "Guinea", "Guadeloupe", "Equatorial Guinea", "Greece",
    "South Georgia and the South Sandwich Islands", "Guatemala", "Guam", "Guinea-Bissau", "Guyana", "Hong Kong", "Heard Island and McDonald Islands", "Honduras", "Croatia", "Haiti", "Hungary",
    "Switzerland", "Indonesia", "Ireland", "Israel", "Isle of Man", "India", "British Indian Ocean Territory", "Iraq", "Iran", "Iceland", "Italy", "Jersey", "Jamaica", "Jordan", "Japan",
    "Kenya", "Kyrgyzstan", "Cambodia", "Kiribati", "Comoros", "Saint Kitts and Nevis", "Democratic People's Republic of Korea", "Republic of Korea", "Kuwait", "Cayman Islands", "Kazakhstan",
    "Lao People's Democratic Republic", "Lebanon", "Saint Lucia", "Liechtenstein", "Sri Lanka", "Liberia", "Lesotho", "Lithuania", "Luxembourg", "Latvia", "Libyan Arab Jamahiriya", "Morocco",
    "Monaco", "Moldova", "Montenegro", "Saint Martin", "Madagascar", "Marshall Islands", "Macedonia", "Mali", "Myanmar", "Mongolia", "Macao", "Northern Mariana Islands", "Martinique",
    "Mauritania", "Montserrat", "Malta", "Mauritius", "Maldives", "Malawi", "Mexico", "Malaysia", "Mozambique", "Namibia", "New Caledonia", "Niger", "Norfolk Island", "Nigeria", "Nicaragua",
    "Netherlands", "Norway", "Nepal", "Nauru", "Niue", "New Zealand", "Oman", "Panama", "Peru", "French Polynesia", "Papua New Guinea", "Philippines", "Pakistan", "Poland",
    "Saint Pierre and Miquelon", "Pitcairn", "Puerto Rico", "Palestinian Territory", "Portugal", "Palau", "Paraguay", "Qatar", "Reunion", "Romania", "Serbia", "Russian Federation",
    "Rwanda", "Saudi Arabia", "Solomon Islands", "Seychelles", "Sudan", "Sweden", "Singapore", "Saint Helena", "Slovenia", "Svalbard and Jan Mayen", "Slovakia", "Sierra Leone", "San Marino",
    "Senegal", "Somalia", "Suriname", "South Sudan", "Sao Tome and Principe", "El Salvador", "Sint Maarten (Dutch Part)", "Syrian Arab Republic", "Swaziland", "Turks and Caicos Islands", "Chad",
    "French Southern Territories", "Togo", "Thailand", "Tajikistan", "Tokelau", "Timor-Leste", "Turkmenistan", "Tunisia", "Tonga", "Turkey", "Trinidad and Tobago", "Tuvalu", "Taiwan",
    "Tanzania", "Ukraine", "Uganda", "United States Minor Outlying Islands", "United States", "Uruguay", "Uzbekistan", "Holy See (Vatican City State)",
    "Saint Vincent and the Grenadines", "Venezuela", "Virgin Islands, British", "Virgin Islands, U.S.", "Viet Nam", "Vanuatu", "Wallis and Futuna", "Samoa", "Yemen", "Mayotte", "South Africa",
    "Zambia", "Zimbabwe", "Netherlands Antilles", "Unknown (Asia-Pacific)", "Unknown (European Union)", "Unknown",
};
// last updated 25 sep 2011
static const char * CountryCodes[] = { "AD", "AE", "AF", "AG", "AI", "AL", "AM", "AO", "AQ", "AR",
    "AS", "AT", "AU", "AW", "AX", "AZ", "BA", "BB", "BD", "BE", "BF", "BG", "BH",
    "BI", "BJ", "BL", "BM", "BN", "BO", "BQ", "BR", "BS", "BT", "BV", "BW", "BY", "BZ", "CA",
    "CC", "CD", "CF", "CG", "CI", "CK", "CL", "CM", "CN", "CO",
    "CR", "CU", "CV", "CW", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM", "DO", "DZ", "EC",
    "EE", "EG", "EH", "ER", "ES", "ET", "FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB",
	"GD", "GE", "GF", "GG", "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR",
    "GS", "GT", "GU", "GW", "GY", "HK", "HM", "HN", "HR", "HT", "HU",
    "CH", "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO", "JP",
    "KE", "KG", "KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ",
    "LA", "LB", "LC", "LI", "LK", "LR", "LS", "LT", "LU", "LV", "LY", "MA",
    "MC", "MD", "ME", "MF", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ",
    "MR", "MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF", "NG", "NI",
    "NL", "NO", "NP", "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG", "PH", "PK", "PL",
    "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RS", "RU",
    "RW", "SA", "SB", "SC", "SD", "SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM",
    "SN", "SO", "SR", "SS", "ST", "SV", "SX", "SY", "SZ", "TC", "TD",
    "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW",
	"TZ", "UA", "UG", "UM", "US", "UY", "UZ", "VA",
    "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT", "ZA",
    "ZM", "ZW", "AN", "AP", "EU", "??",
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsIpP2Country::LoadIPv4() {
    if(clsServerManager::bUseIPv4 == false) {
        return;
    }

#ifdef _WIN32
	FILE * ip2country = fopen((clsServerManager::sPath + "\\cfg\\IpToCountry.csv").c_str(), "r");
#else
	FILE * ip2country = fopen((clsServerManager::sPath + "/cfg/IpToCountry.csv").c_str(), "r");
#endif

    if(ip2country == NULL) {
        return;
    }

    if(ui32Size == 0) {
        ui32Size = 131072;

        if(ui32RangeFrom == NULL) {
#ifdef _WIN32
            ui32RangeFrom = (uint32_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32Size * sizeof(uint32_t));
#else
            ui32RangeFrom = (uint32_t *)calloc(ui32Size, sizeof(uint32_t));
#endif

            if(ui32RangeFrom == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui32RangeFrom\n", 0);
                fclose(ip2country);
                ui32Size = 0;
                return;
            }
        }

        if(ui32RangeTo == NULL) {
#ifdef _WIN32
            ui32RangeTo = (uint32_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32Size * sizeof(uint32_t));
#else
            ui32RangeTo = (uint32_t *)calloc(ui32Size, sizeof(uint32_t));
#endif

            if(ui32RangeTo == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui32RangeTo\n", 0);
                fclose(ip2country);
                ui32Size = 0;
                return;
            }
        }

        if(ui8RangeCI == NULL) {
#ifdef _WIN32
            ui8RangeCI = (uint8_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32Size * sizeof(uint8_t));
#else
            ui8RangeCI = (uint8_t *)calloc(ui32Size, sizeof(uint8_t));
#endif

            if(ui8RangeCI == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui8RangeCI\n", 0);
                fclose(ip2country);
                ui32Size = 0;
                return;
            }
        }
    }

    char sLine[1024];

    while(fgets(sLine, 1024, ip2country) != NULL) {
        if(sLine[0] != '\"') {
            continue;
        }

        if(ui32Count == ui32Size) {
            ui32Size += 512;
            void * oldbuf = ui32RangeFrom;
#ifdef _WIN32
			ui32RangeFrom = (uint32_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint32_t));
#else
			ui32RangeFrom = (uint32_t *)realloc(oldbuf, ui32Size * sizeof(uint32_t));
#endif
            if(ui32RangeFrom == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui32RangeFrom\n", (uint64_t)ui32Size);
                ui32RangeFrom = (uint32_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}

            oldbuf = ui32RangeTo;
#ifdef _WIN32
			ui32RangeTo = (uint32_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint32_t));
#else
			ui32RangeTo = (uint32_t *)realloc(oldbuf, ui32Size * sizeof(uint32_t));
#endif
            if(ui32RangeTo == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui32RangeTo\n", (uint64_t)ui32Size);
                ui32RangeTo = (uint32_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}

            oldbuf = ui8RangeCI;
#ifdef _WIN32
            ui8RangeCI = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint8_t));
#else
			ui8RangeCI = (uint8_t *)realloc(oldbuf, ui32Size * sizeof(uint8_t));
#endif
            if(ui8RangeCI == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui8RangeCI\n", (uint64_t)ui32Size);
                ui8RangeCI = (uint8_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}
        }

        char * sStart = sLine+1;
        uint8_t ui8d = 0;

        size_t szLineLen = strlen(sLine);

		for(size_t szi = 1; szi < szLineLen; szi++) {
            if(sLine[szi] == '\"') {
                sLine[szi] = '\0';
                if(ui8d == 0) {
                    ui32RangeFrom[ui32Count] = strtoul(sStart, NULL, 10);
                } else if(ui8d == 1) {
                    ui32RangeTo[ui32Count] = strtoul(sStart, NULL, 10);
                } else if(ui8d == 4) {
                    for(uint8_t ui8i = 0; ui8i < 252; ui8i++) {
                        if(*((uint16_t *)CountryCodes[ui8i]) == *((uint16_t *)sStart)) {
                            ui8RangeCI[ui32Count] = ui8i;
                            ui32Count++;
                            break;
                        }
                    }

                    break;
                }

                ui8d++;
                szi += (uint16_t)2;
                sStart = sLine+szi+1;

            }
        }
    }

	fclose(ip2country);

    if(ui32Count < ui32Size) {
        ui32Size = ui32Count;

        void * oldbuf = ui32RangeFrom;
#ifdef _WIN32
		ui32RangeFrom = (uint32_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint32_t));
#else
		ui32RangeFrom = (uint32_t *)realloc(oldbuf, ui32Size * sizeof(uint32_t));
#endif
        if(ui32RangeFrom == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui32RangeFrom\n", (uint64_t)ui32Size);
            ui32RangeFrom = (uint32_t *)oldbuf;
    	}

        oldbuf = ui32RangeTo;
#ifdef _WIN32
		ui32RangeTo = (uint32_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint32_t));
#else
		ui32RangeTo = (uint32_t *)realloc(oldbuf, ui32Size * sizeof(uint32_t));
#endif
        if(ui32RangeTo == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui32RangeTo\n", (uint64_t)ui32Size);
            ui32RangeTo = (uint32_t *)oldbuf;
    	}

        oldbuf = ui8RangeCI;
#ifdef _WIN32
        ui8RangeCI = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32Size * sizeof(uint8_t));
#else
		ui8RangeCI = (uint8_t *)realloc(oldbuf, ui32Size * sizeof(uint8_t));
#endif
        if(ui8RangeCI == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui8RangeCI\n", (uint64_t)ui32Size);
            ui8RangeCI = (uint8_t *)oldbuf;
    	}
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsIpP2Country::LoadIPv6() {
    if(clsServerManager::bUseIPv6 == false) {
        return;
    }

#ifdef _WIN32
	FILE * ip2country = fopen((clsServerManager::sPath + "\\cfg\\IpToCountry.6R.csv").c_str(), "r");
#else
	FILE * ip2country = fopen((clsServerManager::sPath + "/cfg/IpToCountry.6R.csv").c_str(), "r");
#endif

    if(ip2country == NULL) {
        return;
    }

    if(ui32IPv6Size == 0) {
        ui32IPv6Size = 16384;

        if(ui128IPv6RangeFrom == NULL) {
#ifdef _WIN32
            ui128IPv6RangeFrom = (uint8_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
            ui128IPv6RangeFrom = (uint8_t *)calloc(ui32IPv6Size, sizeof(uint8_t) * 16);
#endif

            if(ui128IPv6RangeFrom == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui128IPv6RangeFrom\n", 0);
                fclose(ip2country);
                ui32IPv6Size = 0;
                return;
            }
        }

        if(ui128IPv6RangeTo == NULL) {
#ifdef _WIN32
            ui128IPv6RangeTo = (uint8_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
            ui128IPv6RangeTo = (uint8_t *)calloc(ui32IPv6Size, sizeof(uint8_t) * 16);
#endif

            if(ui128IPv6RangeTo == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui128IPv6RangeTo\n", 0);
                fclose(ip2country);
                ui32IPv6Size = 0;
                return;
            }
        }

        if(ui8IPv6RangeCI == NULL) {
#ifdef _WIN32
            ui8IPv6RangeCI = (uint8_t *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, ui32IPv6Size * sizeof(uint8_t));
#else
            ui8IPv6RangeCI = (uint8_t *)calloc(ui32IPv6Size, sizeof(uint8_t));
#endif

            if(ui8IPv6RangeCI == NULL) {
                AppendDebugLog("%s - [MEM] Cannot create clsIpP2Country::ui8IPv6RangeCI\n", 0);
                fclose(ip2country);
                ui32IPv6Size = 0;
                return;
            }
        }
    }

    char sLine[1024];

    while(fgets(sLine, 1024, ip2country) != NULL) {
        if(sLine[0] == '#' || sLine[0] < 32) {
            continue;
        }

        if(ui32IPv6Count == ui32IPv6Size) {
            ui32IPv6Size += 512;
            void * oldbuf = ui128IPv6RangeFrom;
#ifdef _WIN32
			ui128IPv6RangeFrom = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
			ui128IPv6RangeFrom = (uint8_t *)realloc(oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#endif
            if(ui128IPv6RangeFrom == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui128IPv6RangeFrom\n", (uint64_t)ui32IPv6Size);
                ui128IPv6RangeFrom = (uint8_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}

            oldbuf = ui128IPv6RangeTo;
#ifdef _WIN32
			ui128IPv6RangeTo = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
			ui128IPv6RangeTo = (uint8_t *)realloc(oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#endif
            if(ui128IPv6RangeTo == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui128IPv6RangeTo\n", (uint64_t)ui32IPv6Size);
                ui128IPv6RangeTo = (uint8_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}

            oldbuf = ui8IPv6RangeCI;
#ifdef _WIN32
            ui8IPv6RangeCI = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * sizeof(uint8_t));
#else
			ui8IPv6RangeCI = (uint8_t *)realloc(oldbuf, ui32IPv6Size * sizeof(uint8_t));
#endif
            if(ui8IPv6RangeCI == NULL) {
    			AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui8IPv6RangeCI\n", (uint64_t)ui32IPv6Size);
                ui8IPv6RangeCI = (uint8_t *)oldbuf;
    			fclose(ip2country);
                return;
    		}
        }

        char * sStart = sLine;
        uint8_t ui8d = 0;

        size_t szLineLen = strlen(sLine);

		for(size_t szi = 0; szi < szLineLen; szi++) {
            if(ui8d == 0 && sLine[szi] == '-') {
                sLine[szi] = '\0';
#ifdef _WIN32
                win_inet_pton(sStart, ui128IPv6RangeFrom + (ui32IPv6Count*16));
#else
                inet_pton(AF_INET6, sStart, ui128IPv6RangeFrom + (ui32IPv6Count*16));
#endif
            } else if(sLine[szi] == ',') {
                sLine[szi] = '\0';
                if(ui8d == 1) {
#ifdef _WIN32
                    win_inet_pton(sStart, ui128IPv6RangeTo + (ui32IPv6Count*16));
#else
                    inet_pton(AF_INET6, sStart, ui128IPv6RangeTo + (ui32IPv6Count*16));
#endif
                } else {
                    for(uint8_t ui8i = 0; ui8i < 252; ui8i++) {
                        if(*((uint16_t *)CountryCodes[ui8i]) == *((uint16_t *)sStart)) {
                            ui8IPv6RangeCI[ui32IPv6Count] = ui8i;
                            ui32IPv6Count++;

                            break;
                        }
                    }

                    break;
                }
            } else {
                continue;
            }

            ui8d++;
            sStart = sLine+szi+1;
        }
    }

	fclose(ip2country);

    if(ui32IPv6Count < ui32IPv6Size) {
        ui32IPv6Size = ui32IPv6Count;

        void * oldbuf = ui128IPv6RangeFrom;
#ifdef _WIN32
		ui128IPv6RangeFrom = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
		ui128IPv6RangeFrom = (uint8_t *)realloc(oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#endif
        if(ui128IPv6RangeFrom == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui128IPv6RangeFrom\n", (uint64_t)ui32IPv6Size);
            ui128IPv6RangeFrom = (uint8_t *)oldbuf;
    	}

        oldbuf = ui128IPv6RangeTo;
#ifdef _WIN32
		ui128IPv6RangeTo = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#else
		ui128IPv6RangeTo = (uint8_t *)realloc(oldbuf, ui32IPv6Size * (sizeof(uint8_t)*16));
#endif
        if(ui128IPv6RangeTo == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui128IPv6RangeTo\n", (uint64_t)ui32IPv6Size);
            ui128IPv6RangeTo = (uint8_t *)oldbuf;
    	}

        oldbuf = ui8IPv6RangeCI;
#ifdef _WIN32
        ui8IPv6RangeCI = (uint8_t *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)oldbuf, ui32IPv6Size * sizeof(uint8_t));
#else
		ui8IPv6RangeCI = (uint8_t *)realloc(oldbuf, ui32IPv6Size * sizeof(uint8_t));
#endif
        if(ui8IPv6RangeCI == NULL) {
    		AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsIpP2Country::clsIpP2Country for ui8IPv6RangeCI\n", (uint64_t)ui32IPv6Size);
            ui8IPv6RangeCI = (uint8_t *)oldbuf;
    	}
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

clsIpP2Country::clsIpP2Country() {
    ui32Count = 0;
    ui32Size = 0;

	ui32RangeFrom = NULL;
    ui32RangeTo = NULL;
    ui8RangeCI = NULL;

    ui32IPv6Count = 0;
    ui32IPv6Size = 0;

	ui128IPv6RangeFrom = NULL;
    ui128IPv6RangeTo = NULL;
    ui8IPv6RangeCI = NULL;

    LoadIPv4();
    LoadIPv6();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	
clsIpP2Country::~clsIpP2Country() {
#ifdef _WIN32
    if(ui32RangeFrom != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui32RangeFrom) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui32RangeFrom in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui32RangeFrom);
#endif

#ifdef _WIN32
    if(ui32RangeTo != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui32RangeTo) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui32RangeTo in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui32RangeTo);
#endif

#ifdef _WIN32
    if(ui8RangeCI != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui8RangeCI) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui8RangeCI in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui8RangeCI);
#endif

#ifdef _WIN32
    if(ui128IPv6RangeFrom != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui128IPv6RangeFrom) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui128IPv6RangeFrom in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui128IPv6RangeFrom);
#endif

#ifdef _WIN32
    if(ui128IPv6RangeTo != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui128IPv6RangeTo) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui128IPv6RangeTo in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui128IPv6RangeTo);
#endif

#ifdef _WIN32
    if(ui8IPv6RangeCI != NULL) {
        if(HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)ui8IPv6RangeCI) == 0) {
            AppendDebugLog("%s - [MEM] Cannot deallocate clsIpP2Country::ui8IPv6RangeCI in clsIpP2Country::~clsIpP2Country\n", 0);
        }
    }
#else
	free(ui8IPv6RangeCI);
#endif
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const char * clsIpP2Country::Find(const uint8_t * ui128IpHash, const bool &bCountryName) {
    bool bIPv4 = false;
    uint32_t ui32IpHash = 0;

    if(clsServerManager::bUseIPv6 == false || IN6_IS_ADDR_V4MAPPED((in6_addr *)ui128IpHash)) {
        bIPv4 = true;

        ui32IpHash = 16777216 * ui128IpHash[12] + 65536 * ui128IpHash[13] + 256 * ui128IpHash[14] + ui128IpHash[15];
    }

    if(bIPv4 == false) {
        if(ui128IpHash[0] == 32 && ui128IpHash[1] == 2) { // 6to4 tunnel
            bIPv4 = true;

            ui32IpHash = 16777216 * ui128IpHash[2] + 65536 * ui128IpHash[3] + 256 * ui128IpHash[4] + ui128IpHash[5];
        } else if(ui128IpHash[0] == 32 && ui128IpHash[1] == 1 && ui128IpHash[2] == 0 && ui128IpHash[3] == 0) { // teredo tunnel
            bIPv4 = true;

            ui32IpHash = (16777216 * ui128IpHash[12] + 65536 * ui128IpHash[13] + 256 * ui128IpHash[14] + ui128IpHash[15]) ^ 0xffffffff;
        }
    }

    if(bIPv4 == true) {
        for(uint32_t ui32i = 0; ui32i < ui32Count; ui32i++) {
            if(ui32RangeFrom[ui32i] <= ui32IpHash && ui32RangeTo[ui32i] >= ui32IpHash) {
                if(bCountryName == false) {
                    return CountryCodes[ui8RangeCI[ui32i]];
                } else {
                    return CountryNames[ui8RangeCI[ui32i]];
                }
            }
        }
    } else {
        for(uint32_t ui32i = 0; ui32i < ui32IPv6Count; ui32i++) {
            if(memcmp(ui128IPv6RangeFrom+(ui32i*16), ui128IpHash, 16) <= 0 && memcmp(ui128IPv6RangeTo+(ui32i*16), ui128IpHash, 16) >= 0) {
                if(bCountryName == false) {
                    return CountryCodes[ui8IPv6RangeCI[ui32i]];
                } else {
                    return CountryNames[ui8IPv6RangeCI[ui32i]];
                }
            }
        }
    }

    if(bCountryName == false) {
        return CountryCodes[252];
    } else {
        return CountryNames[252];
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

uint8_t clsIpP2Country::Find(const uint8_t * ui128IpHash) {
    bool bIPv4 = false;
    uint32_t ui32IpHash = 0;

    if(clsServerManager::bUseIPv6 == false || IN6_IS_ADDR_V4MAPPED((in6_addr *)ui128IpHash)) {
        bIPv4 = true;

        ui32IpHash = 16777216 * ui128IpHash[12] + 65536 * ui128IpHash[13] + 256 * ui128IpHash[14] + ui128IpHash[15];
    }

    if(bIPv4 == false) {
        if(ui128IpHash[0] == 32 && ui128IpHash[1] == 2) { // 6to4 tunnel
            bIPv4 = true;

            ui32IpHash = 16777216 * ui128IpHash[2] + 65536 * ui128IpHash[3] + 256 * ui128IpHash[4] + ui128IpHash[5];
        } else if(ui128IpHash[0] == 32 && ui128IpHash[1] == 1 && ui128IpHash[2] == 0 && ui128IpHash[3] == 0) { // teredo tunnel
            bIPv4 = true;

            ui32IpHash = (16777216 * ui128IpHash[12] + 65536 * ui128IpHash[13] + 256 * ui128IpHash[14] + ui128IpHash[15]) ^ 0xffffffff;
        }
    }

    if(bIPv4 == true) {
        for(uint32_t ui32i = 0; ui32i < ui32Count; ui32i++) {
            if(ui32RangeFrom[ui32i] <= ui32IpHash && ui32RangeTo[ui32i] >= ui32IpHash) {
                return ui8RangeCI[ui32i];
            }
        }
    } else {
        for(uint32_t ui32i = 0; ui32i < ui32IPv6Count; ui32i++) {
            if(memcmp(ui128IPv6RangeFrom+(ui32i*16), ui128IpHash, 16) <= 0 && memcmp(ui128IPv6RangeTo+(ui32i*16), ui128IpHash, 16) >= 0) {
                return ui8IPv6RangeCI[ui32i];
            }
        }
    }

    return 252;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

const char * clsIpP2Country::GetCountry(const uint8_t &ui8dx, const bool &bCountryName) {
    if(bCountryName == false) {
        return CountryCodes[ui8dx];
    } else {
        return CountryNames[ui8dx];
    }
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void clsIpP2Country::Reload() {
    ui32Count = 0;
    LoadIPv4();

    ui32IPv6Count = 0;
    LoadIPv6();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
