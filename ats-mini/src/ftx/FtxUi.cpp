#include "FtxUi.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "FtxDecoderService.h"
#include "../audio/AudioCaptureService.h"
#include "Common.h"
#include "Menu.h"
#include "../radio/DecoderFrequencyList.h"
#include "Themes.h"
#include "Utils.h"

namespace {
FtxDecoderService decoderService;
bool pageActive = false;
uint8_t restoreNetworkMode = NET_OFF;
uint32_t lastPageRefresh = 0;
FtxMode pageMode = FtxMode::FT8;

FtxDecodedMessage recentMessages[4]{};
uint32_t decodedMessageCount = 0;
uint64_t lastProcessedSlot = 0;
bool haveProcessedSlot = false;

// Temporarily raise receiver audio while the digital decoder is active.
// The original user volume is restored when leaving the decoder page.
constexpr uint8_t kFtxPlaybackVolume = 50;
uint8_t volumeBeforeFtx = 0;
bool ftxVolumeBoostActive = false;
// Toggle only the external speaker amplifier; keep SI473x audio and GPIO11 ADC capture alive.
bool ftxSpeakerAmpBefore = true;
bool ftxAudioMuteBefore = true;
bool ftxSpeakerEnabled = true;
bool ftxActionsOpen = false;
bool ftxPresetMenuOpen = false;
uint8_t ftxPresetSelection = 0;
uint8_t ftxPresetScroll = 0;
DecoderKind ftxPresetDecoder = DecoderKind::FT8;
constexpr uint32_t kFtxPresetPreviewDelayMs = 2000;
VfoState ftxPresetOriginalState{};
int16_t ftxPresetOriginalBfo = 0;
bool ftxPresetOriginalValid = false;
Band ftxPresetTargetBandOriginal{};
int ftxPresetTargetBandIndex = -1;
bool ftxPresetTargetBandSnapshotValid = false;
bool ftxPresetPreviewPending = false;
bool ftxPresetPreviewActive = false;
uint32_t ftxPresetPreviewStartedAt = 0;
bool ftxPresetPreviewSpeakerWasEnabled = true;
bool ftxPresetPreviewSpeakerForced = false;
bool ftxAutoScroll = true;
uint8_t ftxMenuSelection = 0;
constexpr uint8_t kFtxMenuItems = 9;
bool ftxOfflineSyncPreference = false;
bool ftxOfflineSyncActive = false;
bool ftxLogEnabled = false;
char ftxLogPath[40] = {};

const char* countryForCall(const char* call)
{
  if(!call || !*call) return "";
  char prefix[4] = {};
  size_t n = 0;
  while(call[n] && !isdigit(static_cast<unsigned char>(call[n])) && n < 3) {
    prefix[n] = static_cast<char>(toupper(static_cast<unsigned char>(call[n])));
    ++n;
  }
  if(!n && isdigit(static_cast<unsigned char>(call[0]))) {
    prefix[0] = call[0];
    prefix[1] = call[1] ? static_cast<char>(toupper(static_cast<unsigned char>(call[1]))) : 0;
  }
  // Common amateur-radio prefixes. Unknown/ambiguous calls deliberately have no flag.
  if(!strcmp(prefix,"DL") || !strcmp(prefix,"DA") || !strcmp(prefix,"DB") ||
     !strcmp(prefix,"DC") || !strcmp(prefix,"DD") || !strcmp(prefix,"DF") ||
     !strcmp(prefix,"DG") || !strcmp(prefix,"DH") || !strcmp(prefix,"DJ") ||
     !strcmp(prefix,"DK") || !strcmp(prefix,"DM") || !strcmp(prefix,"DO") ||
     !strcmp(prefix,"DP") || !strcmp(prefix,"DQ") || !strcmp(prefix,"DR")) return "DE";
  if(!strcmp(prefix,"G") || !strcmp(prefix,"M") || !strcmp(prefix,"2E") ||
     !strcmp(prefix,"2I") || !strcmp(prefix,"2M") || !strcmp(prefix,"2W")) return "GB";
  if(!strcmp(prefix,"F") || !strcmp(prefix,"TM") || !strcmp(prefix,"TO")) return "FR";
  if(!strcmp(prefix,"I") || !strcmp(prefix,"IK") || !strcmp(prefix,"IZ")) return "IT";
  if(!strcmp(prefix,"EA") || !strcmp(prefix,"EB") || !strcmp(prefix,"EC") ||
     !strcmp(prefix,"ED") || !strcmp(prefix,"EE") || !strcmp(prefix,"EF") ||
     !strcmp(prefix,"EG") || !strcmp(prefix,"EH")) return "ES";
  if(!strcmp(prefix,"CT") || !strcmp(prefix,"CS")) return "PT";
  if(!strcmp(prefix,"ON")) return "BE";
  if(!strcmp(prefix,"PA") || !strcmp(prefix,"PB") || !strcmp(prefix,"PC") ||
     !strcmp(prefix,"PD") || !strcmp(prefix,"PE") || !strcmp(prefix,"PF") ||
     !strcmp(prefix,"PG") || !strcmp(prefix,"PH") || !strcmp(prefix,"PI")) return "NL";
  if(!strcmp(prefix,"HB") || !strcmp(prefix,"HE")) return "CH";
  if(!strcmp(prefix,"OE")) return "AT";
  if(!strcmp(prefix,"OK") || !strcmp(prefix,"OL")) return "CZ";
  if(!strcmp(prefix,"OM")) return "SK";
  if(!strcmp(prefix,"SP") || !strcmp(prefix,"SQ") || !strcmp(prefix,"SR")) return "PL";
  if(!strcmp(prefix,"SM") || !strcmp(prefix,"SA") || !strcmp(prefix,"SB") ||
     !strcmp(prefix,"SC") || !strcmp(prefix,"SD") || !strcmp(prefix,"SE") ||
     !strcmp(prefix,"SF") || !strcmp(prefix,"SG") || !strcmp(prefix,"SH") ||
     !strcmp(prefix,"SI") || !strcmp(prefix,"SJ") || !strcmp(prefix,"SK") ||
     !strcmp(prefix,"SL") || !strcmp(prefix,"7S") || !strcmp(prefix,"8S")) return "SE";
  if(!strcmp(prefix,"OZ")) return "DK";
  if(!strcmp(prefix,"LA") || !strcmp(prefix,"LB") || !strcmp(prefix,"LC") ||
     !strcmp(prefix,"LD") || !strcmp(prefix,"LE") || !strcmp(prefix,"LF") ||
     !strcmp(prefix,"LG") || !strcmp(prefix,"LH") || !strcmp(prefix,"LI") ||
     !strcmp(prefix,"LJ") || !strcmp(prefix,"LK") || !strcmp(prefix,"LL") ||
     !strcmp(prefix,"LM") || !strcmp(prefix,"LN")) return "NO";
  if(!strcmp(prefix,"OH") || !strcmp(prefix,"OF") || !strcmp(prefix,"OG")) return "FI";
  if(!strcmp(prefix,"K") || !strcmp(prefix,"W") || !strcmp(prefix,"N") ||
     (prefix[0]=='A' && prefix[1]>='A' && prefix[1]<='L')) return "US";
  if(!strcmp(prefix,"VE") || !strcmp(prefix,"VA") || !strcmp(prefix,"VO") ||
     !strcmp(prefix,"VY")) return "CA";
  if(!strcmp(prefix,"VK")) return "AU";
  if(!strcmp(prefix,"JA") || !strcmp(prefix,"JE") || !strcmp(prefix,"JF") ||
     !strcmp(prefix,"JG") || !strcmp(prefix,"JH") || !strcmp(prefix,"JI") ||
     !strcmp(prefix,"JJ") || !strcmp(prefix,"JK") || !strcmp(prefix,"JL") ||
     !strcmp(prefix,"JM") || !strcmp(prefix,"JN") || !strcmp(prefix,"JO") ||
     !strcmp(prefix,"JP") || !strcmp(prefix,"JQ") || !strcmp(prefix,"JR")) return "JP";
  if(!strcmp(prefix,"ZL")) return "NZ";
  if(!strcmp(prefix,"PY") || !strcmp(prefix,"PP") || !strcmp(prefix,"PQ") ||
     !strcmp(prefix,"PR") || !strcmp(prefix,"PS") || !strcmp(prefix,"PT") ||
     !strcmp(prefix,"PU") || !strcmp(prefix,"PV") || !strcmp(prefix,"PW") ||
     !strcmp(prefix,"PX")) return "BR";
  if(!strcmp(prefix,"LU") || !strcmp(prefix,"LO") || !strcmp(prefix,"LP") ||
     !strcmp(prefix,"LQ") || !strcmp(prefix,"LR") || !strcmp(prefix,"LS") ||
     !strcmp(prefix,"LT") || !strcmp(prefix,"LW")) return "AR";
  if(!strcmp(prefix,"XE") || !strcmp(prefix,"XF") || !strcmp(prefix,"4A") ||
     !strcmp(prefix,"4B") || !strcmp(prefix,"4C")) return "MX";
  if(!strcmp(prefix,"HL") || !strcmp(prefix,"DS") || !strcmp(prefix,"DT")) return "KR";
  if(!strcmp(prefix,"BY") || !strcmp(prefix,"BA") || !strcmp(prefix,"BD") ||
     !strcmp(prefix,"BG") || !strcmp(prefix,"BH") || !strcmp(prefix,"BI") ||
     !strcmp(prefix,"BJ") || !strcmp(prefix,"BL") || !strcmp(prefix,"BT") ||
     !strcmp(prefix,"BU") || !strcmp(prefix,"BV") || !strcmp(prefix,"BW") ||
     !strcmp(prefix,"BX") || !strcmp(prefix,"BZ")) return "CN";
  // Common additional DXCC prefixes. If a country has no dedicated
  // flag drawing below, drawFlag() displays its two-letter ISO code instead.
  if(!strcmp(prefix,"LZ")) return "BG"; // Bulgaria
  if(!strcmp(prefix,"R") || !strcmp(prefix,"RA") || !strcmp(prefix,"RC") ||
     !strcmp(prefix,"RD") || !strcmp(prefix,"RE") || !strcmp(prefix,"RF") ||
     !strcmp(prefix,"RG") || !strcmp(prefix,"RH") || !strcmp(prefix,"RI") ||
     !strcmp(prefix,"RJ") || !strcmp(prefix,"RK") || !strcmp(prefix,"RL") ||
     !strcmp(prefix,"RM") || !strcmp(prefix,"RN") || !strcmp(prefix,"RO") ||
     !strcmp(prefix,"RP") || !strcmp(prefix,"RQ") || !strcmp(prefix,"RR") ||
     !strcmp(prefix,"RT") || !strcmp(prefix,"RU") || !strcmp(prefix,"RV") ||
     !strcmp(prefix,"RW") || !strcmp(prefix,"RX") || !strcmp(prefix,"RY") ||
     !strcmp(prefix,"RZ")) return "RU"; // Russia
  if(!strcmp(prefix,"UR") || !strcmp(prefix,"US") || !strcmp(prefix,"UT") ||
     !strcmp(prefix,"UU") || !strcmp(prefix,"UV") || !strcmp(prefix,"UW") ||
     !strcmp(prefix,"UX") || !strcmp(prefix,"UY") || !strcmp(prefix,"UZ")) return "UA"; // Ukraine
  if(!strcmp(prefix,"YO") || !strcmp(prefix,"YP") || !strcmp(prefix,"YQ") ||
     !strcmp(prefix,"YR")) return "RO"; // Romania
  if(!strcmp(prefix,"HA") || !strcmp(prefix,"HG")) return "HU"; // Hungary
  if(!strcmp(prefix,"YU") || !strcmp(prefix,"YT")) return "RS"; // Serbia
  if(!strcmp(prefix,"9A")) return "HR"; // Croatia
  if(!strcmp(prefix,"S5")) return "SI"; // Slovenia
  if(!strcmp(prefix,"4O")) return "ME"; // Montenegro
  if(!strcmp(prefix,"ER")) return "MD"; // Moldova
  if(!strcmp(prefix,"ES")) return "EE"; // Estonia
  if(!strcmp(prefix,"YL")) return "LV"; // Latvia
  if(!strcmp(prefix,"LY")) return "LT"; // Lithuania
  if(!strcmp(prefix,"EW")) return "BY"; // Belarus
  if(!strcmp(prefix,"4X") || !strcmp(prefix,"4Z")) return "IL"; // Israel
  if(!strcmp(prefix,"SV") || !strcmp(prefix,"SX") || !strcmp(prefix,"J4")) return "GR"; // Greece
  if(!strcmp(prefix,"9H")) return "MT"; // Malta
  if(!strcmp(prefix,"9M") || !strcmp(prefix,"9W")) return "MY"; // Malaysia
  if(!strcmp(prefix,"VU") || !strcmp(prefix,"AT")) return "IN"; // India
  if(!strcmp(prefix,"4S")) return "LK"; // Sri Lanka
  if(!strcmp(prefix,"9V")) return "SG"; // Singapore
  if(!strcmp(prefix,"HS") || !strcmp(prefix,"E2")) return "TH"; // Thailand
  if(!strcmp(prefix,"BV") || !strcmp(prefix,"BM") || !strcmp(prefix,"BN")) return "TW"; // Taiwan
  if(!strcmp(prefix,"DU") || !strcmp(prefix,"DV") || !strcmp(prefix,"DW") ||
     !strcmp(prefix,"DX") || !strcmp(prefix,"DY") || !strcmp(prefix,"DZ")) return "PH"; // Philippines
  if(!strcmp(prefix,"A4")) return "OM"; // Oman
  if(!strcmp(prefix,"A6")) return "AE"; // UAE
  if(!strcmp(prefix,"A7")) return "QA"; // Qatar
  if(!strcmp(prefix,"A9")) return "BH"; // Bahrain
  if(!strcmp(prefix,"HZ")) return "SA"; // Saudi Arabia
  if(!strcmp(prefix,"5B")) return "CY"; // Cyprus
  if(!strcmp(prefix,"Z3")) return "MK"; // North Macedonia
  if(!strcmp(prefix,"TA") || !strcmp(prefix,"TB") || !strcmp(prefix,"TC")) return "TR"; // Turkey
  if(!strcmp(prefix,"CN")) return "MA"; // Morocco
  if(!strcmp(prefix,"9G")) return "GH"; // Ghana
  if(!strcmp(prefix,"5Z")) return "KE"; // Kenya
  if(!strcmp(prefix,"ZS") || !strcmp(prefix,"ZT") || !strcmp(prefix,"ZU")) return "ZA"; // South Africa
  if(!strcmp(prefix,"D4")) return "CV"; // Cape Verde
  if(!strcmp(prefix,"C5")) return "GM"; // Gambia
  if(!strcmp(prefix,"5A")) return "LY"; // Libya
  if(!strcmp(prefix,"3V")) return "TN"; // Tunisia
  if(!strcmp(prefix,"7X")) return "DZ"; // Algeria
  if(!strcmp(prefix,"7Q")) return "MW"; // Malawi
  if(!strcmp(prefix,"9J")) return "ZM"; // Zambia
  if(!strcmp(prefix,"5H")) return "TZ"; // Tanzania
  if(!strcmp(prefix,"JY")) return "JO"; // Jordan
  if(!strcmp(prefix,"YK")) return "SY"; // Syria
  if(!strcmp(prefix,"EP") || !strcmp(prefix,"EQ")) return "IR"; // Iran
  if(!strcmp(prefix,"AP")) return "PK"; // Pakistan
  if(!strcmp(prefix,"9K")) return "KW"; // Kuwait
  if(!strcmp(prefix,"9L")) return "SL"; // Sierra Leone
  if(!strcmp(prefix,"7P")) return "LS"; // Lesotho
  if(!strcmp(prefix,"YV") || !strcmp(prefix,"YY")) return "VE"; // Venezuela
  if(!strcmp(prefix,"HC") || !strcmp(prefix,"HD")) return "EC"; // Ecuador
  if(!strcmp(prefix,"OA")) return "PE"; // Peru
  if(!strcmp(prefix,"CP")) return "BO"; // Bolivia
  if(!strcmp(prefix,"CE")) return "CL"; // Chile
  if(!strcmp(prefix,"CX")) return "UY"; // Uruguay
  if(!strcmp(prefix,"HK")) return "CO"; // Colombia
  if(!strcmp(prefix,"TI") || !strcmp(prefix,"TE")) return "CR"; // Costa Rica
  if(!strcmp(prefix,"HP")) return "PA"; // Panama
  if(!strcmp(prefix,"TG")) return "GT"; // Guatemala
  if(!strcmp(prefix,"YS")) return "SV"; // El Salvador
  if(!strcmp(prefix,"HR")) return "HN"; // Honduras
  if(!strcmp(prefix,"YN")) return "NI"; // Nicaragua
  if(!strcmp(prefix,"ZP")) return "PY"; // Paraguay
  if(!strcmp(prefix,"AX")) return "AU"; // Australia
  if(!strcmp(prefix,"FK")) return "NC"; // New Caledonia
  if(!strcmp(prefix,"FO")) return "PF"; // French Polynesia
  if(!strcmp(prefix,"V6")) return "FM"; // Micronesia
  if(!strcmp(prefix,"T8")) return "PW"; // Palau
  if(!strcmp(prefix,"YJ")) return "VU"; // Vanuatu
  if(!strcmp(prefix,"P2")) return "PG"; // Papua New Guinea
  if(!strcmp(prefix,"A3")) return "TO"; // Tonga
  if(!strcmp(prefix,"5W")) return "WS"; // Samoa
  if(!strcmp(prefix,"ZK")) return "CK"; // Cook Islands
  if(!strcmp(prefix,"V8")) return "BN"; // Brunei
  if(!strcmp(prefix,"P4")) return "AW"; // Aruba
  if(!strcmp(prefix,"V3")) return "BZ"; // Belize
  if(!strcmp(prefix,"C6")) return "BS"; // Bahamas
  if(!strcmp(prefix,"ZF")) return "KY"; // Cayman Islands
  if(!strcmp(prefix,"6Y")) return "JM"; // Jamaica
  if(!strcmp(prefix,"HI")) return "DO"; // Dominican Republic
  if(!strcmp(prefix,"CO") || !strcmp(prefix,"CM")) return "CU"; // Cuba
  if(!strcmp(prefix,"8P")) return "BB"; // Barbados
  if(!strcmp(prefix,"8R")) return "GY"; // Guyana
  if(!strcmp(prefix,"PZ")) return "SR"; // Suriname
  if(!strcmp(prefix,"YI")) return "IQ"; // Iraq
  if(!strcmp(prefix,"XU")) return "KH"; // Cambodia
  if(!strcmp(prefix,"XW")) return "LA"; // Laos
  if(!strcmp(prefix,"A5")) return "BT"; // Bhutan
  if(!strcmp(prefix,"S2")) return "BD"; // Bangladesh
  if(!strcmp(prefix,"9N")) return "NP"; // Nepal
  if(!strcmp(prefix,"5T")) return "MR"; // Mauritania
  if(!strcmp(prefix,"5U")) return "NE"; // Niger
  if(!strcmp(prefix,"5V")) return "TG"; // Togo
  if(!strcmp(prefix,"TR")) return "GA"; // Gabon
  if(!strcmp(prefix,"TU")) return "CI"; // Cote d'Ivoire
  if(!strcmp(prefix,"TY")) return "BJ"; // Benin
  if(!strcmp(prefix,"TZ")) return "ML"; // Mali
  if(!strcmp(prefix,"XT")) return "BF"; // Burkina Faso
  if(!strcmp(prefix,"3X")) return "GN"; // Guinea
  if(!strcmp(prefix,"9U")) return "BI"; // Burundi
  if(!strcmp(prefix,"9X")) return "RW"; // Rwanda
  if(!strcmp(prefix,"ET")) return "ET"; // Ethiopia
  if(!strcmp(prefix,"ST")) return "SD"; // Sudan
  if(!strcmp(prefix,"SU")) return "EG"; // Egypt
  if(!strcmp(prefix,"5R")) return "MG"; // Madagascar
  if(!strcmp(prefix,"V5")) return "NA"; // Namibia
  if(!strcmp(prefix,"A2")) return "BW"; // Botswana
  if(!strcmp(prefix,"C9")) return "MZ"; // Mozambique
  if(!strcmp(prefix,"D2")) return "AO"; // Angola
  if(!strcmp(prefix,"D6")) return "KM"; // Comoros
  if(!strcmp(prefix,"EL")) return "LR"; // Liberia
  if(!strcmp(prefix,"J5")) return "GW"; // Guinea-Bissau
  if(!strcmp(prefix,"J2")) return "DJ"; // Djibouti
  if(!strcmp(prefix,"S9")) return "ST"; // Sao Tome and Principe
  if(!strcmp(prefix,"TL")) return "CF"; // Central African Republic
  if(!strcmp(prefix,"TN")) return "CG"; // Republic of the Congo
  if(!strcmp(prefix,"TT")) return "TD"; // Chad
  if(!strcmp(prefix,"5X")) return "UG"; // Uganda
  if(!strcmp(prefix,"9Y")) return "TT"; // Trinidad and Tobago
  if(!strcmp(prefix,"VP9")) return "BM"; // Bermuda
  if(!strcmp(prefix,"V2")) return "AG"; // Antigua and Barbuda
  if(!strcmp(prefix,"J3")) return "GD"; // Grenada
  if(!strcmp(prefix,"J6")) return "LC"; // Saint Lucia
  if(!strcmp(prefix,"J7")) return "DM"; // Dominica
  if(!strcmp(prefix,"J8")) return "VC"; // Saint Vincent
  if(!strcmp(prefix,"PJ2")) return "CW"; // Curacao
  if(!strcmp(prefix,"PJ4")) return "BQ"; // Bonaire
  if(!strcmp(prefix,"PJ5") || !strcmp(prefix,"PJ7")) return "SX"; // Sint Maarten
  if(!strcmp(prefix,"VP5")) return "TC"; // Turks and Caicos
  if(!strcmp(prefix,"FG")) return "GP"; // Guadeloupe
  if(!strcmp(prefix,"FM")) return "MQ"; // Martinique
  if(!strcmp(prefix,"FY")) return "GF"; // French Guiana
  if(!strcmp(prefix,"3B8") || !strcmp(prefix,"3B9")) return "MU"; // Mauritius
  if(!strcmp(prefix,"ZB")) return "GI"; // Gibraltar
  if(!strcmp(prefix,"ZD7") || !strcmp(prefix,"ZD8")) return "SH"; // Saint Helena / Ascension
  return "";
}

bool isLocator(const char* token)
{
  const size_t len = strlen(token);
  if(len != 4 && len != 6) return false;
  if(token[0]<'A' || token[0]>'R' || token[1]<'A' || token[1]>'R' ||
     !isdigit(static_cast<unsigned char>(token[2])) ||
     !isdigit(static_cast<unsigned char>(token[3]))) return false;
  if(len == 6 && (token[4]<'A' || token[4]>'X' || token[5]<'A' || token[5]>'X')) return false;
  return true;
}

bool locatorCenter(const char* locator, double& lat, double& lon)
{
  if(!isLocator(locator)) return false;
  lon = -180.0 + (toupper(locator[0])-'A')*20.0 +
        (locator[2]-'0')*2.0 + 1.0;
  lat = -90.0 + (toupper(locator[1])-'A')*10.0 +
        (locator[3]-'0')*1.0 + 0.5;
  if(strlen(locator) == 6) {
    lon += (toupper(locator[4])-'A')*(2.0/24.0) + (1.0/24.0);
    lat += (toupper(locator[5])-'A')*(1.0/24.0) + (1.0/48.0);
  }
  return true;
}

bool distanceBetweenKm(double lat1, double lon1, double lat2, double lon2, int& km)
{
  const double rad = 0.017453292519943295;
  const double dLat = (lat1-lat2)*rad;
  const double dLon = (lon1-lon2)*rad;
  const double a = sin(dLat/2)*sin(dLat/2) +
    cos(lat2*rad)*cos(lat1*rad)*sin(dLon/2)*sin(dLon/2);
  const double arc = 2.0*atan2(sqrt(a), sqrt(1.0-a));
  km = static_cast<int>(6371.0*arc + 0.5);
  return true;
}

bool distanceFromQthKm(const char* locator, int& km)
{
  double qthLat, qthLon, stationLat, stationLon;
  if(!locatorCenter("JO31TV", qthLat, qthLon) ||
     !locatorCenter(locator, stationLat, stationLon)) return false;
  return distanceBetweenKm(stationLat, stationLon, qthLat, qthLon, km);
}

// Approximate geographic centers (not station locations). Used only when a
// decoded callsign identifies a country but the message contains no locator.
bool countryCenter(const char* country, double& lat, double& lon)
{
  struct CountryCenter { const char* code; double lat; double lon; };
  static const CountryCenter centers[] = {
    {"DE",51.2,10.4},{"PL",52.0,19.0},{"RU",55.75,37.6},{"UA",49.0,32.0},
    {"BG",42.7,25.5},{"FR",46.2,2.2},{"GB",54.0,-2.0},{"IT",42.8,12.8},
    {"ES",40.2,-3.7},{"PT",39.5,-8.0},{"NL",52.2,5.3},{"BE",50.8,4.5},
    {"CH",46.8,8.2},{"AT",47.6,14.1},{"CZ",49.8,15.5},{"SK",48.7,19.5},
    {"SE",62.0,15.0},{"NO",62.0,10.0},{"DK",56.0,10.0},{"FI",64.0,26.0},
    {"IE",53.2,-8.0},{"IS",65.0,-18.0},{"GR",39.0,22.0},{"RO",45.8,24.9},
    {"HU",47.2,19.5},{"RS",44.0,21.0},{"HR",45.2,15.5},{"SI",46.1,14.9},
    {"ME",42.7,19.3},{"MD",47.0,28.5},{"EE",58.7,25.5},{"LV",56.9,24.6},
    {"LT",55.2,23.9},{"BY",53.7,27.9},{"IL",31.5,34.8},{"TR",39.0,35.0},
    {"CY",35.1,33.4},{"MT",35.9,14.4},{"US",39.8,-98.6},{"CA",56.0,-106.0},
    {"MX",23.6,-102.5},{"BR",-10.0,-55.0},{"AR",-38.4,-63.6},{"CL",-30.0,-71.0},
    {"CO",4.5,-74.0},{"VE",8.0,-66.0},{"PE",-9.2,-75.0},{"AU",-25.0,134.0},
    {"NZ",-41.0,174.0},{"JP",36.2,138.3},{"CN",35.0,103.0},{"KR",36.5,127.8},
    {"IN",22.0,79.0},{"ID",-2.0,118.0},{"TH",15.0,101.0},{"MY",4.2,102.0},
    {"PH",12.9,122.0},{"TW",23.7,121.0},{"SG",1.35,103.8},{"LK",7.9,80.7},
    {"PK",30.4,69.3},{"IR",32.0,53.0},{"IL",31.5,34.8},{"SA",24.0,45.0},
    {"AE",24.0,54.0},{"OM",21.0,57.0},{"QA",25.3,51.2},{"BH",26.0,50.5},
    {"JO",31.2,36.0},{"EG",26.8,30.8},{"ZA",-29.0,24.0},{"MA",31.8,-7.1},
    {"KE",0.2,37.9},{"GH",7.9,-1.0},{"NG",9.1,8.7},{"TN",34.0,9.0},
    {"DZ",28.0,1.7},{"LY",26.3,17.2},{"ET",9.1,40.5},{"TZ",-6.3,35.0},
    {"UG",1.4,32.3},{"ZM",-13.1,27.8},{"NA",-22.0,17.0},{"BW",-22.0,24.0},
    {"MG",-19.0,46.7},{"NZ",-41.0,174.0},{"EC",-1.5,-78.4},{"UY",-32.8,-56.0},
    {"BO",-17.0,-65.0},{"CR",10.0,-84.0},{"PA",8.5,-80.0},{"GT",15.5,-90.2},
    {"SV",13.8,-88.9},{"HN",15.0,-86.5},{"NI",13.0,-85.0},{"PY",-23.4,-58.4},
    {"CU",21.5,-78.0},{"DO",19.0,-70.7},{"JM",18.1,-77.3},{"TT",10.7,-61.2},
    {"IS",65.0,-18.0},{"NZ",-41.0,174.0},{"FJ",-17.8,178.1},{"PG",-6.3,145.0},
    {"VU",-16.0,167.0},{"WS",-13.8,-172.0},{"TO",-21.2,-175.1},{"CV",15.1,-23.6},
    {"MW",-13.3,34.3},{"RW",-1.9,29.9},{"BI",-3.4,29.9},{"SL",8.5,-11.8},
    {"LR",6.4,-9.4},{"GA",-0.6,11.6},{"CI",7.5,-5.5},{"BJ",9.5,2.3},
    {"ML",17.0,-4.0},{"BF",12.3,-1.5},{"GN",10.4,-10.8},{"NE",17.6,8.1},
    {"TG",8.6,0.8},{"TD",15.0,19.0},{"SD",15.0,30.0},{"AO",-12.5,18.5},
    {"MZ",-18.7,35.5},{"KM",-11.7,43.3},{"CD",-3.0,23.0},{"CG",-1.0,15.0},
    {"CF",6.6,20.9},{"ST",0.2,6.6},{"GW",12.0,-15.0},{"DJ",11.8,42.6},
    {"AG",17.1,-61.8},{"BB",13.2,-59.5},{"BS",24.3,-76.0},{"BZ",17.2,-88.7},
    {"BM",32.3,-64.7},{"KY",19.3,-81.2},{"AW",12.5,-69.9},{"CW",12.2,-69.0},
    {"BQ",12.2,-68.3},{"SX",18.0,-63.0},{"TC",21.8,-71.8},{"GP",16.2,-61.6},
    {"MQ",14.6,-61.0},{"GF",4.0,-53.0},{"GI",36.1,-5.4},{"SH",-15.9,-5.7},
    {"NC",-21.3,165.5},{"PF",-17.7,-149.4},{"FM",7.4,151.8},{"PW",7.5,134.5},
    {"CK",-21.2,-159.8},{"BN",4.5,114.7},{"BT",27.5,90.4},{"BD",24.0,90.0},
    {"NP",28.3,84.0},{"KH",12.6,105.0},{"LA",19.0,103.0},{"IQ",33.0,44.0},
    {"KH",12.6,105.0},{"LS",-29.6,28.2},{"PY",-23.4,-58.4},{"SR",4.0,-56.0},
    {"EC",-1.5,-78.4},{"PE",-9.2,-75.0},{"JO",31.2,36.0},{"SY",35.0,38.0},
    {"KW",29.3,47.5},{"MR",20.3,-10.5},{"NE",17.6,8.1},{"BF",12.3,-1.5},
    {"GN",10.4,-10.8},{"BI",-3.4,29.9},{"RW",-1.9,29.9},{"UG",1.4,32.3},
    {"TT",10.7,-61.2},{"AG",17.1,-61.8},{"GD",12.1,-61.7},{"LC",13.9,-60.9},
    {"DM",15.4,-61.4},{"VC",13.3,-61.2},{"MU",-20.2,57.5},{"GI",36.1,-5.4}
  };
  for(const CountryCenter& center : centers) {
    if(!strcmp(country, center.code)) {
      lat = center.lat;
      lon = center.lon;
      return true;
    }
  }
  return false;
}

bool distanceFromCountryKm(const char* country, int& km)
{
  double qthLat, qthLon, stationLat, stationLon;
  if(!locatorCenter("JO31TV", qthLat, qthLon) ||
     !countryCenter(country, stationLat, stationLon)) return false;
  return distanceBetweenKm(stationLat, stationLon, qthLat, qthLon, km);
}

void extractMetadata(const char* message, char* country, size_t countrySize,
                     char* locator, size_t locatorSize, int& distanceKm,
                     bool& distanceEstimated)
{
  country[0] = locator[0] = 0;
  distanceKm = -1;
  distanceEstimated = false;
  char copy[FTX_MAX_MESSAGE_LENGTH + 1] = {};
  strncpy(copy, message, sizeof(copy)-1);
  char* save = nullptr;
  for(char* token = strtok_r(copy, " ", &save); token; token = strtok_r(nullptr, " ", &save)) {
    if(!locator[0] && isLocator(token)) {
      strncpy(locator, token, locatorSize-1);
      distanceFromQthKm(locator, distanceKm);
    }
    if(!country[0]) {
      bool hasDigit = false;
      for(const char* p=token; *p; ++p) if(isdigit(static_cast<unsigned char>(*p))) hasDigit = true;
      if(hasDigit) {
        const char* found = countryForCall(token);
        if(*found) strncpy(country, found, countrySize-1);
      }
    }
  }
  // A real locator always wins. Only estimate from the country when none exists.
  if(distanceKm < 0 && country[0]) {
    distanceEstimated = distanceFromCountryKm(country, distanceKm);
  }
}

void drawFlag(const char* country, int x, int y)
{
  const uint16_t white=0xFFFF, black=0x0000, red=0xF800, gold=0xFEA0;
  const uint16_t blue=0x001F, navy=0x0010, green=0x07E0;
  spr.drawRect(x, y, 22, 14, 0x7BEF);
  if(!strcmp(country,"DE")) {
    spr.fillRect(x+1,y+1,20,4,black); spr.fillRect(x+1,y+5,20,4,red); spr.fillRect(x+1,y+9,20,4,gold);
  } else if(!strcmp(country,"FR")) {
    spr.fillRect(x+1,y+1,7,12,blue); spr.fillRect(x+8,y+1,6,12,white); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"IT")) {
    spr.fillRect(x+1,y+1,7,12,green); spr.fillRect(x+8,y+1,6,12,white); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"NL")) {
    spr.fillRect(x+1,y+1,20,4,red); spr.fillRect(x+1,y+5,20,4,white); spr.fillRect(x+1,y+9,20,4,blue);
  } else if(!strcmp(country,"BE")) {
    spr.fillRect(x+1,y+1,7,12,black); spr.fillRect(x+8,y+1,6,12,gold); spr.fillRect(x+14,y+1,7,12,red);
  } else if(!strcmp(country,"JP")) {
    spr.fillRect(x+1,y+1,20,12,white); spr.fillCircle(x+11,y+7,3,red);
  } else if(!strcmp(country,"US")) {
    for(int i=0;i<6;++i) spr.fillRect(x+1,y+1+i*2,20,1,(i&1)?white:red);
    spr.fillRect(x+1,y+1,9,6,navy);
  } else if(!strcmp(country,"GB")) {
    spr.fillRect(x+1,y+1,20,12,blue);
    spr.drawLine(x+2,y+2,x+20,y+12,white); spr.drawLine(x+20,y+2,x+2,y+12,white);
    spr.fillRect(x+9,y+1,4,12,white); spr.fillRect(x+1,y+5,20,4,white);
    spr.fillRect(x+10,y+1,2,12,red); spr.fillRect(x+1,y+6,20,2,red);
  } else if(!strcmp(country,"ES")) {
    spr.fillRect(x+1,y+1,20,3,red); spr.fillRect(x+1,y+4,20,6,gold); spr.fillRect(x+1,y+10,20,3,red);
  } else if(!strcmp(country,"CA")) {
    spr.fillRect(x+1,y+1,5,12,red); spr.fillRect(x+6,y+1,10,12,white); spr.fillRect(x+16,y+1,5,12,red);
    spr.fillTriangle(x+11,y+3,x+9,y+7,x+13,y+7,red); spr.fillTriangle(x+11,y+6,x+9,y+10,x+13,y+10,red);
  } else if(!strcmp(country,"CH")) {
    spr.fillRect(x+1,y+1,20,12,red); spr.fillRect(x+9,y+3,4,8,white); spr.fillRect(x+7,y+5,8,4,white);
  } else {
    // Unknown/unmapped flag: show only the two-letter country code.
    // Paint over the generic flag outline above so no badge/frame remains.
    spr.fillRect(x,y,22,14,0x1036);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(0x07FF);
    spr.drawString(country,x+11,y+7);
  }
}

uint8_t ftxPresetCount()
{
  uint8_t count = 0;
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i)
    if(kDecoderFrequencyPresets[i].decoder == ftxPresetDecoder) ++count;
  // The two 11-metre CB channel entries belong only to the FT8 quick-preset
  // submenu; keep the shared Decoder Frequencies list unchanged.
  if(ftxPresetDecoder == DecoderKind::FT8) count += 2;
  return count;
}

const DecoderFrequencyPreset* ftxPresetAt(uint8_t wanted)
{
  uint8_t index = 0;
  for(uint8_t i=0;i<kDecoderFrequencyPresetCount;++i) {
    if(kDecoderFrequencyPresets[i].decoder != ftxPresetDecoder) continue;
    if(index++ == wanted) return &kDecoderFrequencyPresets[i];
  }

  if(ftxPresetDecoder == DecoderKind::FT8) {
    static const DecoderFrequencyPreset cbPresets[] = {
      {"FT8 11m CH24", 27235, USB, 4, DecoderKind::FT8},
      {"FT8 11m CH25", 27245, USB, 4, DecoderKind::FT8}
    };
    const uint8_t cbIndex = wanted - index;
    if(cbIndex < sizeof(cbPresets) / sizeof(cbPresets[0]))
      return &cbPresets[cbIndex];
  }
  return nullptr;
}

bool applyFtxPreset(const DecoderFrequencyPreset& preset);

void restoreFtxPresetPreviewSpeaker()
{
  if(!ftxPresetPreviewSpeakerForced) return;
  // GPIO3 (AUDIO_MUTE) mutes only the L/R speaker path. Keep the external
  // amplifier powered because GPIO11 is the independent FT8 ADC audio tap.
  digitalWrite(AUDIO_MUTE, ftxPresetPreviewSpeakerWasEnabled ? LOW : HIGH);
  ftxSpeakerEnabled = ftxPresetPreviewSpeakerWasEnabled;
  ftxPresetPreviewSpeakerForced = false;
}

void clearFtxPresetPreviewState()
{
  ftxPresetOriginalValid = false;
  ftxPresetTargetBandIndex = -1;
  ftxPresetTargetBandSnapshotValid = false;
  ftxPresetPreviewPending = false;
  ftxPresetPreviewActive = false;
  ftxPresetPreviewStartedAt = 0;
  restoreFtxPresetPreviewSpeaker();
}

void restoreFtxPresetOriginal(bool clearSnapshot = true)
{
  if(ftxPresetOriginalValid && ftxPresetPreviewActive) {
    if(ftxPresetTargetBandSnapshotValid && ftxPresetTargetBandIndex >= 0 &&
       ftxPresetTargetBandIndex < getTotalBands())
      bands[ftxPresetTargetBandIndex] = ftxPresetTargetBandOriginal;
    Band& savedBand = bands[ftxPresetOriginalState.band];
    savedBand.currentFreq = ftxPresetOriginalState.freq;
    savedBand.bandMode = ftxPresetOriginalState.mode;
    savedBand.currentStepIdx = ftxPresetOriginalState.stepIdx;
    savedBand.bandwidthIdx = ftxPresetOriginalState.bandwidthIdx;
    selectBand(ftxPresetOriginalState.band, false);
    if(currentBFO != ftxPresetOriginalBfo)
      updateBFO(ftxPresetOriginalBfo, true);
  }
  if(clearSnapshot) {
    clearFtxPresetPreviewState();
  } else {
    ftxPresetPreviewPending = false;
    ftxPresetPreviewActive = false;
    ftxPresetPreviewStartedAt = 0;
    restoreFtxPresetPreviewSpeaker();
  }
}

void scheduleFtxPresetPreview(uint32_t now)
{
  if(!ftxPresetOriginalValid) {
    ftxPresetOriginalState = captureVfo();
    ftxPresetOriginalBfo = currentBFO;
    ftxPresetOriginalValid = true;
  }
  ftxPresetPreviewPending = true;
  ftxPresetPreviewActive = false;
  ftxPresetPreviewStartedAt = now;
}

void startFtxPresetPreview()
{
  const DecoderFrequencyPreset* preset = ftxPresetAt(ftxPresetSelection);
  if(!preset || !ftxPresetOriginalValid) {
    ftxPresetPreviewPending = false;
    return;
  }

  // Treat the tune as provisional before calling the receiver: even a
  // failed/partial tune must be rolled back to the saved VFO state.
  ftxPresetPreviewActive = true;
  if(!applyFtxPreset(*preset)) {
    restoreFtxPresetOriginal(false);
    return;
  }

  ftxPresetPreviewPending = false;
  if(!ftxSpeakerEnabled) {
    ftxPresetPreviewSpeakerWasEnabled = ftxSpeakerEnabled;
    ftxPresetPreviewSpeakerForced = true;
    ftxSpeakerEnabled = true;
    digitalWrite(AUDIO_MUTE, LOW);
  }
}

void commitFtxPresetPreview()
{
  // Keep the selected frequency, but undo any temporary speaker override.
  restoreFtxPresetPreviewSpeaker();
  ftxPresetOriginalValid = false;
  ftxPresetTargetBandIndex = -1;
  ftxPresetTargetBandSnapshotValid = false;
  ftxPresetPreviewPending = false;
  ftxPresetPreviewActive = false;
  ftxPresetPreviewStartedAt = 0;
}

bool applyFtxPreset(const DecoderFrequencyPreset& preset)
{
  // Prefer the narrowest existing HF band containing this frequency.
  // This avoids the broad ALL-SW band shadowing the dedicated amateur bands.
  int bestBand = -1;
  uint16_t bestSpan = UINT16_MAX;
  for(int i=0;i<getTotalBands();++i) {
    if(bands[i].bandType == FM_BAND_TYPE ||
       preset.frequencyKHz < bands[i].minimumFreq ||
       preset.frequencyKHz > bands[i].maximumFreq) continue;
    const uint16_t span = bands[i].maximumFreq - bands[i].minimumFreq;
    if(span < bestSpan) {
      bestSpan = span;
      bestBand = i;
    }
  }
  if(bestBand < 0) return false;

  // Keep the target band's pre-preview memory intact when auditioning another band.
  ftxPresetTargetBandIndex = bestBand;
  ftxPresetTargetBandOriginal = bands[bestBand];
  ftxPresetTargetBandSnapshotValid = true;
  bands[bestBand].currentFreq = static_cast<uint16_t>(preset.frequencyKHz);
  bands[bestBand].bandMode = preset.modulation;
  bands[bestBand].bandwidthIdx = preset.bandwidthChoice;
  currentBFO = 0;
  selectBand(static_cast<uint8_t>(bestBand), false);
  currentMode = preset.modulation;
  currentFrequency = rx.getFrequency();
  return currentFrequency == preset.frequencyKHz;
}

bool loadFtxOfflineSyncPreference()
{
  Preferences preferences;
  if(!preferences.begin("ftx", true)) return false;
  const bool offline = preferences.getBool("offline", false);
  preferences.end();
  return offline;
}

bool saveFtxOfflineSyncPreference(bool offline)
{
  Preferences preferences;
  if(!preferences.begin("ftx", false)) return false;
  const size_t written = preferences.putBool("offline", offline);
  preferences.end();
  return written > 0;
}

uint8_t ftxActionCount()
{
  return pageMode == FtxMode::FT8 ? kFtxMenuItems : kFtxMenuItems - 3;
}

bool ftxLogStart()
{
  // Logging must also work with offline signal synchronization or without UTC.
  // Use a dated filename when a valid date is available, otherwise a unique
  // untimed filename. Message timestamps remain explicitly "unknown" offline.
  uint16_t year = 0;
  uint8_t month = 0, day = 0;
  const bool haveDate = clockAvailable() &&
    clockGetDate(&year, &month, &day, nullptr);
  char base[32] = {};
  const char* modeName = pageMode == FtxMode::FT4 ? "FT4" : "FT8";
  if(haveDate) {
    snprintf(base, sizeof(base), "%s_%02u.%02u.%02u", modeName,
             static_cast<unsigned>(day), static_cast<unsigned>(month),
             static_cast<unsigned>(year % 100));
  } else {
    snprintf(base, sizeof(base), "%s_UNTIMED", modeName);
  }
  for(uint16_t suffix = 0; suffix < 1000; ++suffix) {
    if(suffix == 0) snprintf(ftxLogPath, sizeof(ftxLogPath), "/%s.txt", base);
    else snprintf(ftxLogPath, sizeof(ftxLogPath), "/%s_%u.txt", base, suffix);
    if(!LittleFS.exists(ftxLogPath)) {
      File file = LittleFS.open(ftxLogPath, "w");
      if(!file) { ftxLogPath[0] = 0; return false; }
      file.printf("# %s reception log\n# Date/time: UTC when valid; unknown for offline reception\n# QTH: JO31TV\n# Columns: UTC | Frequency kHz | Mode | Country | Locator | Distance km | SNR dB | Message\n",
                  modeName);
      file.flush();
      file.close();
      ftxLogEnabled = true;
      return true;
    }
  }
  ftxLogPath[0] = 0;
  return false;
}

void ftxLogStop()
{
  ftxLogEnabled = false;
}

void ftxLogMessage(const FtxDecodedMessage& message)
{
  if(!ftxLogEnabled || !ftxLogPath[0] || !message.text[0]) return;
  File file = LittleFS.open(ftxLogPath, "a");
  if(!file) { ftxLogStop(); return; }
  struct tm stamp = {};
  const bool haveTime = message.utcValid &&
    gmtime_r(&message.slotUtc, &stamp) != nullptr;
  char timeText[24] = "unknown";
  if(haveTime) strftime(timeText, sizeof(timeText), "%Y-%m-%d %H:%M:%S", &stamp);
  char country[4] = {}, locator[8] = {};
  int distanceKm = -1;
  bool distanceEstimated = false;
  extractMetadata(message.text, country, sizeof(country), locator, sizeof(locator), distanceKm, distanceEstimated);
  file.printf("%s | %u.%03u | %s | %s | %s | ",
              timeText,
              static_cast<unsigned>(currentFrequency / 1000),
              static_cast<unsigned>(currentFrequency % 1000),
              pageMode == FtxMode::FT4 ? "FT4" : "FT8",
              country[0] ? country : "Unknown",
              locator[0] ? locator : "N/A");
  if(distanceKm >= 0) file.printf("%s%d", distanceEstimated ? "~" : "", distanceKm);
  else file.print("N/A");
  file.printf(" | %.1f | %s\n", message.snrDb, message.text);
  file.flush();
  file.close();
}

void addRecentMessage(const FtxDecodedMessage& message)
{
  if(!message.text[0]) return;
  for(int i=3;i>0;--i) recentMessages[i]=recentMessages[i-1];
  recentMessages[0]=message;
  ++decodedMessageCount;
}

void refreshRecentMessages()
{
  FtxDecodedMessage messages[FtxDecoderService::kResultCapacity]{};
  const size_t count=decoderService.copyResults(messages,FtxDecoderService::kResultCapacity);
  if(!count) return;
  uint64_t newestSlot=lastProcessedSlot;
  for(size_t i=0;i<count;++i)
    if(messages[i].slotId>newestSlot) newestSlot=messages[i].slotId;
  if(!haveProcessedSlot || newestSlot>lastProcessedSlot) {
    for(size_t i=0;i<count;++i) {
      if(messages[i].slotId != newestSlot) continue;
      // Logging is independent of the visible auto-scroll setting.
      ftxLogMessage(messages[i]);
      if(ftxAutoScroll) addRecentMessage(messages[i]);
    }
    lastProcessedSlot=newestSlot;
    haveProcessedSlot=true;
  }
}

void drawClippedText(const char* value, int x, int y, int maxWidth, uint16_t color)
{
  char clipped[FTX_MAX_MESSAGE_LENGTH+1]={};
  size_t len=strlen(value);
  if(len>=sizeof(clipped)) len=sizeof(clipped)-1;
  memcpy(clipped,value,len);
  clipped[len]=0;
  spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
  while(len>0 && spr.textWidth(clipped)>maxWidth) clipped[--len]=0;
  spr.setTextDatum(TL_DATUM);
  spr.setTextColor(color);
  spr.drawString(clipped,x,y);
}
}

bool ftxUiOpen(FtxMode mode)
{
  pageMode = mode;
  ftxOfflineSyncPreference = loadFtxOfflineSyncPreference();
  // Signal-derived synchronization is currently an FT8-only receive mode.
  // FT4 retains its existing UTC-boundary behavior.
  ftxOfflineSyncActive = mode == FtxMode::FT8 && ftxOfflineSyncPreference;
  if(!ftxOfflineSyncActive && !clockAvailable())
  {
    statusShow("UTC time not available", "Wait for NTP or RDS CT");
    return false;
  }

  ftxActionsOpen = false;
  ftxPresetMenuOpen = false;
  ftxPresetSelection = ftxPresetScroll = 0;
  ftxPresetDecoder = DecoderKind::FT8;
  ftxMenuSelection = 0;
  ftxAutoScroll = true;
  ftxLogEnabled = false;
  ftxLogPath[0] = 0;
  restoreNetworkMode = wifiModeIdx;
  netStop();

  if(getWiFiStatus() != 0 || (!ftxOfflineSyncActive && !clockAvailable()) ||
     !decoderService.enter(mode, true, clockAvailable(), ftxOfflineSyncActive, sharedAudioCapture()))
  {
    decoderService.exit();
    netInit(restoreNetworkMode);
    ftxOfflineSyncActive = false;
    statusShow("Decoder start failed", (mode == FtxMode::FT8 && ftxOfflineSyncPreference) ? "Check audio / PSRAM" : "Check UTC and audio input");
    return false;
  }

  // Raise the SI473x output level for audible monitoring and a stronger
  // signal at the existing audio tap. The decoder samples themselves are
  // untouched; only the receiver's output volume register is changed.
  volumeBeforeFtx = volume;
  if(volume < kFtxPlaybackVolume) {
    volume = kFtxPlaybackVolume;
    rx.setVolume(volume);
  }
  ftxVolumeBoostActive = true;

  // Keep the amplifier powered for the GPIO11 decoder tap. Speaker ON/OFF
  // uses the separate active-high L/R mute line (GPIO3), not AMP_EN.
  ftxSpeakerAmpBefore = PIN_AMP_EN < 0 || digitalRead(PIN_AMP_EN) == HIGH;
  ftxAudioMuteBefore = digitalRead(AUDIO_MUTE) == HIGH;
  ftxSpeakerEnabled = ftxSpeakerAmpBefore && !ftxAudioMuteBefore;
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, HIGH);
  digitalWrite(AUDIO_MUTE, ftxSpeakerEnabled ? LOW : HIGH);

  memset(recentMessages,0,sizeof(recentMessages));
  decodedMessageCount=0;
  lastProcessedSlot=0;
  haveProcessedSlot=false;
  pageActive = true;
  lastPageRefresh = 0;
  return true;
}

void ftxUiExit()
{
  if(!pageActive) return;
  restoreFtxPresetOriginal();
  pageActive = false;
  ftxActionsOpen = false;
  ftxPresetMenuOpen = false;
  ftxLogStop();
  decoderService.exit();
  ftxOfflineSyncActive = false;
  digitalWrite(AUDIO_MUTE, ftxAudioMuteBefore ? HIGH : LOW);
  if(PIN_AMP_EN >= 0) digitalWrite(PIN_AMP_EN, ftxSpeakerAmpBefore ? HIGH : LOW);
  ftxSpeakerEnabled = ftxSpeakerAmpBefore && !ftxAudioMuteBefore;
  if(ftxVolumeBoostActive) {
    volume = volumeBeforeFtx;
    rx.setVolume(volume);
    ftxVolumeBoostActive = false;
  }
  netInit(restoreNetworkMode);
  currentCmd = CMD_NONE;
}

void ftxUiRotate(int delta)
{
  if(!pageActive || !ftxActionsOpen || !delta) return;
  if(ftxPresetMenuOpen) {
    const int count = ftxPresetCount();
    int next = static_cast<int>(ftxPresetSelection) + (delta > 0 ? 1 : -1);
    if(next < 0) next = count - 1;
    if(next >= count) next = 0;
    if(ftxPresetPreviewActive)
      restoreFtxPresetOriginal();
    ftxPresetSelection = static_cast<uint8_t>(next);
    if(ftxPresetSelection < ftxPresetScroll) ftxPresetScroll = ftxPresetSelection;
    if(ftxPresetSelection >= ftxPresetScroll + 6) ftxPresetScroll = ftxPresetSelection - 5;
    scheduleFtxPresetPreview(millis());
    return;
  }
  const int count = ftxActionCount();
  int next = static_cast<int>(ftxMenuSelection) + (delta > 0 ? 1 : -1);
  if(next < 0) next = count - 1;
  if(next >= count) next = 0;
  ftxMenuSelection = static_cast<uint8_t>(next);
}

void ftxUiClick()
{
  if(!pageActive) return;
  if(!ftxActionsOpen) {
    ftxActionsOpen = true;
    ftxPresetMenuOpen = false;
    ftxMenuSelection = 0;
    return;
  }
  if(ftxPresetMenuOpen) {
    const DecoderFrequencyPreset* preset = ftxPresetAt(ftxPresetSelection);
    bool applied = ftxPresetPreviewActive;
    if(!applied && preset) {
      // Mark the tune provisional so a receiver tune mismatch is also rolled back.
      ftxPresetPreviewActive = true;
      applied = applyFtxPreset(*preset);
    }
    if(applied)
      commitFtxPresetPreview();
    else
      restoreFtxPresetOriginal();
    ftxPresetMenuOpen = false;
    ftxActionsOpen = false;
    return;
  }

  if(pageMode == FtxMode::FT8) {
    switch(ftxMenuSelection) {
      case 0: // FT8 presets
      case 1: // FT4 presets
        ftxPresetDecoder = ftxMenuSelection == 0 ? DecoderKind::FT8 : DecoderKind::FT4;
        ftxPresetMenuOpen = true;
        ftxPresetSelection = ftxPresetScroll = 0;
        scheduleFtxPresetPreview(millis());
        break;
      case 2: { // Toggle and persist FT8 time synchronization mode
        const bool requestedOffline = !ftxOfflineSyncPreference;
        if(!requestedOffline && !clockAvailable()) {
          statusShow("UTC time not available", "Enable Wi-Fi / NTP first");
          break;
        }
        const bool previousPreference = ftxOfflineSyncPreference;
        const bool previousActive = ftxOfflineSyncActive;
        if(!saveFtxOfflineSyncPreference(requestedOffline)) {
          statusShow("Setting not saved", "Preferences storage error");
          break;
        }
        ftxOfflineSyncPreference = requestedOffline;
        ftxOfflineSyncActive = requestedOffline;
        if(!decoderService.enter(pageMode, true, clockAvailable(), ftxOfflineSyncActive, sharedAudioCapture())) {
          ftxOfflineSyncPreference = previousPreference;
          ftxOfflineSyncActive = previousActive;
          saveFtxOfflineSyncPreference(previousPreference);
          decoderService.enter(pageMode, true, clockAvailable(), previousActive, sharedAudioCapture());
          statusShow("Sync mode switch failed", "Previous mode restored");
        } else {
          // Keep an active log running across online/offline sync changes.
          // Offline messages are stored with an "unknown" timestamp.
          lastProcessedSlot = 0;
          haveProcessedSlot = false;
          statusShow(requestedOffline ? "Offline signal sync" : "Online UTC sync",
                     requestedOffline ? "Scanning for FT8 slot" : "UTC slot timing active");
        }
        break;
      }
      case 3: // Speaker
        ftxSpeakerEnabled = !ftxSpeakerEnabled;
        // Mute only the L/R speaker output; leave AMP_EN and GPIO11 capture live.
        digitalWrite(AUDIO_MUTE, ftxSpeakerEnabled ? LOW : HIGH);
        break;
      case 4: // Auto-scroll
        ftxAutoScroll = !ftxAutoScroll;
        break;
      case 5: // Clear displayed station history/counters
        memset(recentMessages, 0, sizeof(recentMessages));
        decodedMessageCount = 0;
        break;
      case 6: // Toggle per-session log
        if(ftxLogEnabled) ftxLogStop();
        else if(!ftxLogStart()) statusShow("Logging unavailable", "Could not create log file");
        break;
      case 7: // Back
        ftxActionsOpen = false;
        break;
      case 8: // Exit decoder
        ftxUiExit();
        break;
    }
  } else {
    switch(ftxMenuSelection) {
      case 0: // Speaker
        ftxSpeakerEnabled = !ftxSpeakerEnabled;
        digitalWrite(AUDIO_MUTE, ftxSpeakerEnabled ? LOW : HIGH);
        break;
      case 1: ftxAutoScroll = !ftxAutoScroll; break;
      case 2:
        memset(recentMessages, 0, sizeof(recentMessages));
        decodedMessageCount = 0;
        break;
      case 3:
        if(ftxLogEnabled) ftxLogStop();
        else if(!ftxLogStart()) statusShow("Logging unavailable", "Could not create log file");
        break;
      case 4: ftxActionsOpen = false; break;
      case 5: ftxUiExit(); break;
    }
  }
}

bool ftxUiTick(uint32_t now)
{
  if(!pageActive) return false;

  bool changed = false;
  if(ftxPresetMenuOpen && ftxPresetPreviewPending &&
     static_cast<uint32_t>(now - ftxPresetPreviewStartedAt) >= kFtxPresetPreviewDelayMs) {
    startFtxPresetPreview();
    changed = true;
  }

  if(static_cast<uint32_t>(now - lastPageRefresh) < 500) return changed;
  lastPageRefresh = now;
  refreshRecentMessages();
  return true;
}

void drawFtxDecoderPage()
{
  const uint16_t bg=0x0000, blue=0x001F, cyan=0x07FF;
  spr.fillSprite(bg);
  // Keep only the display's outer frame; message rows are borderless.
  char frequencyText[24];
  snprintf(frequencyText,sizeof(frequencyText),"%u.%03u", currentFrequency/1000,currentFrequency%1000);
  spr.setTextColor(0xFFFF); spr.setTextDatum(TL_DATUM);
  const int frequencyWidth = spr.drawString(frequencyText,8,5,&lgfx::fonts::Orbitron_Light_24);
  spr.setTextDatum(ML_DATUM); spr.setTextColor(cyan);
  spr.drawString("kHz",8+frequencyWidth+7,19,FONT_SMALL);
  spr.setTextDatum(MC_DATUM); spr.setTextColor(cyan);
  spr.drawString(ftxSpeakerEnabled?"SPK ON":"SPK OFF",205,19,FONT_SMALL);
  char counters[16];
  snprintf(counters,sizeof(counters),"OK %03lu", static_cast<unsigned long>(decodedMessageCount));
  spr.setTextDatum(TR_DATUM); spr.drawString(counters,311,10,FONT_SMALL);
  // Separated receive-progress blocks in green, yellow and red EQ zones.
  const uint32_t maxBlocks=decoderService.decoderMaxBlocks();
  const uint32_t blocks=decoderService.decoderBlocks();
  constexpr int meterSegments=37, yellowStart=25, redStart=32;
  const int activeSegments=maxBlocks ? static_cast<int>((blocks>maxBlocks?maxBlocks:blocks)*meterSegments/maxBlocks) : 0;
  spr.drawRect(8,34,304,12,blue); spr.fillRect(10,36,300,8,bg);
  for(int segment=0;segment<meterSegments;++segment) {
    const int x=10+segment*8;
    const uint16_t zoneColor=segment<yellowStart?0x07E0:(segment<redStart?0xFFE0:0xF800);
    spr.fillRect(x,36,6,8,segment<activeSegments?zoneColor:0x2104);
  }
  // Four borderless message rows use the space freed by the removed boxes.
  for(int i=0;i<4;++i) {
    const int y=51+i*28;
    if(!recentMessages[i].text[0]) {
      if(i==0) { spr.setFont(&lgfx::fonts::FreeSansBold9pt7b); spr.setTextDatum(ML_DATUM); spr.setTextColor(cyan); spr.drawString("Waiting for FT8/FT4 decodes...",14,y+8); }
      continue;
    }
    char country[4]={}, locator[8]={}; int distanceKm=-1; bool distanceEstimated=false;
    extractMetadata(recentMessages[i].text,country,sizeof(country),locator,sizeof(locator),distanceKm,distanceEstimated);
    if(country[0]) drawFlag(country,12,y+5);
    const int messageX=country[0]?40:14, distanceWidth=64;
    const int messageWidth=306-(messageX-7)-distanceWidth-8;
    drawClippedText(recentMessages[i].text,messageX,y+7,messageWidth,0xFFFF);
    char distanceText[16];
    if(distanceKm>=0) {
      if(distanceKm>=10000) snprintf(distanceText,sizeof(distanceText),">9999 km");
      else snprintf(distanceText,sizeof(distanceText),"%s%d km",distanceEstimated?"~":"",distanceKm);
    } else snprintf(distanceText,sizeof(distanceText),"---");
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b); spr.setTextDatum(TR_DATUM);
    spr.setTextColor(distanceKm>=0?cyan:0x7BEF); spr.drawString(distanceText,306,y+7);
  }
  if(ftxActionsOpen) {
    const int menuX=20, menuY=8, menuW=280, menuH=154;
    spr.fillRect(menuX,menuY,menuW,menuH,bg);
    spr.drawRect(menuX,menuY,menuW,menuH,blue);
    spr.setFont(&lgfx::fonts::FreeSansBold9pt7b);
    spr.setTextDatum(TL_DATUM);
    spr.setTextColor(cyan);
    if(ftxPresetMenuOpen) {
      spr.drawString(ftxPresetDecoder==DecoderKind::FT4 ? "FT4 PRESETS" : "FT8 PRESETS",menuX+12,menuY+7);
      const uint8_t count=ftxPresetCount();
      for(uint8_t row=0;row<6;++row) {
        const uint8_t index=ftxPresetScroll+row;
        if(index>=count) break;
        const DecoderFrequencyPreset* preset=ftxPresetAt(index);
        if(!preset) continue;
        char label[40];
        snprintf(label,sizeof(label),"%-8s %2u.%03u MHz USB",
                 preset->label+4,
                 static_cast<unsigned>(preset->frequencyKHz/1000),
                 static_cast<unsigned>(preset->frequencyKHz%1000));
        const int y=menuY+29+row*20;
        if(index==ftxPresetSelection) spr.fillRect(menuX+7,y-3,menuW-14,19,blue);
        spr.setTextColor(index==ftxPresetSelection?0xFFFF:0xBDF7);
        spr.drawString(label,menuX+13,y);
        if(index==ftxPresetSelection && (ftxPresetPreviewPending || ftxPresetPreviewActive)) {
          spr.setTextDatum(TR_DATUM);
          spr.setTextColor(ftxPresetPreviewActive ? 0x07E0 : 0xFFE0);
          spr.drawString(ftxPresetPreviewActive ? "LISTEN" : "WAIT",menuX+menuW-12,y);
          spr.setTextDatum(TL_DATUM);
        }
      }
      char page[12];
      snprintf(page,sizeof(page),"%02u/%02u",static_cast<unsigned>(ftxPresetSelection+1),static_cast<unsigned>(count));
      spr.setTextDatum(TR_DATUM);
      spr.setTextColor(0xFFFF);
      spr.drawString(page,menuX+menuW-10,menuY+7);
    } else {
      const char* labels[kFtxMenuItems] = {
        "FT8 PRESETS",
        "FT4 PRESETS",
        ftxOfflineSyncPreference ? "TIME SYNC   OFFLINE" : "TIME SYNC   ONLINE",
        ftxSpeakerEnabled ? "SPEAKER       ON" : "SPEAKER       OFF",
        ftxAutoScroll ? "AUTO SCROLL   ON" : "AUTO SCROLL   OFF",
        "CLEAR STATIONS",
        ftxLogEnabled ? "LOGGING       ON" : "LOGGING       OFF",
        "BACK",
        "EXIT FT8 + FT4"
      };
      const uint8_t count=ftxActionCount();
      spr.drawString(pageMode==FtxMode::FT4 ? "FT4 ACTIONS" : "FT8 ACTIONS",menuX+12,menuY+7);
      for(uint8_t i=0;i<count;++i) {
        const int y=menuY+27+i*14;
        if(i==ftxMenuSelection) spr.fillRect(menuX+7,y-2,menuW-14,15,blue);
        spr.setTextColor(i==ftxMenuSelection?0xFFFF:0xBDF7);
        spr.drawString(labels[i+(pageMode==FtxMode::FT4?3:0)],menuX+14,y);
      }
    }
  }
}
