#ifndef PAGE_CONFIG_H
#define PAGE_CONFIG_H

#include <Arduino.h>

static const char pageConfigTitle[] PROGMEM = "ATS-Mini Config";

static const char pageConfigOverview[] PROGMEM = R"HTML(<H1>{{title}}</H1>
{{{navigation}}}
<P>Choose a configuration area. Each page saves only its own settings.</P>
<DIV CLASS="CONFIGFORM">
  <TABLE COLUMNS="2">
    <TR><TH CLASS="HEADING"><A HREF="/config/wifi">WiFi &amp; Web Login</A></TH><TD>Networks, passwords and hidden SSID scanning</TD></TR>
    <TR><TH CLASS="HEADING"><A HREF="/config/display">Display &amp; Controls</A></TH><TD>Theme, scrolling, encoder and menu zoom</TD></TR>
    <TR><TH CLASS="HEADING"><A HREF="/config/time">Date &amp; Time</A></TH><TD>Time zone and browser clock synchronization</TD></TR>
    <TR><TH CLASS="HEADING"><A HREF="/config/splash">Splash Screen</A></TH><TD>Upload or remove the startup image</TD></TR>
    <TR><TH CLASS="HEADING"><A HREF="/config/weather">Weather</A></TH><TD>German postal code for local weather</TD></TR>
    <TR><TH CLASS="HEADING"><A HREF="/config/news">RSS News</A></TH><TD>Up to four personal RSS/Atom feeds</TD></TR>
  </TABLE>
</DIV>
)HTML";

static const char pageConfigWifi[] PROGMEM = R"HTML(<H1>WiFi &amp; Web Login</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" CLASS="CONFIGFORM">
<INPUT TYPE="HIDDEN" NAME="section" VALUE="wifi">
<TABLE COLUMNS="2">
<TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 1</TH></TR>
<TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid1" VALUE="{{ssid1}}"></TD></TR>
<TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass1" VALUE="{{pass1}}"></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 2</TH></TR>
<TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid2" VALUE="{{ssid2}}"></TD></TR>
<TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass2" VALUE="{{pass2}}"></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING">WiFi Network 3</TH></TR>
<TR><TD CLASS="LABEL">SSID</TD><TD><INPUT TYPE="TEXT" NAME="wifissid3" VALUE="{{ssid3}}"></TD></TR>
<TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="wifipass3" VALUE="{{pass3}}"></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING">Web UI Login</TH></TR>
<TR><TD CLASS="LABEL">Username</TD><TD><INPUT TYPE="TEXT" NAME="username" VALUE="{{username}}"></TD></TR>
<TR><TD CLASS="LABEL">Password</TD><TD><INPUT TYPE="PASSWORD" NAME="password" VALUE="{{password}}"></TD></TR>
<TR><TD CLASS="LABEL">Scan Hidden SSIDs</TD><TD><INPUT TYPE="CHECKBOX" NAME="wifiscanhidden" VALUE="on" {{scan_hidden}}></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save WiFi settings"></TH></TR>
</TABLE></FORM>)HTML";

static const char pageConfigDisplay[] PROGMEM = R"HTML(<H1>Display &amp; Controls</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" CLASS="CONFIGFORM">
<INPUT TYPE="HIDDEN" NAME="section" VALUE="display">
<TABLE COLUMNS="2">
<TR><TD CLASS="LABEL">Theme</TD><TD><SELECT NAME="theme">{{{theme_options}}}</SELECT></TD></TR>
<TR><TD CLASS="LABEL">Reverse Scrolling</TD><TD><INPUT TYPE="CHECKBOX" NAME="scroll" VALUE="on" {{scroll}}></TD></TR>
<TR><TD CLASS="LABEL">Half-step Encoder</TD><TD><INPUT TYPE="CHECKBOX" NAME="encoderhalfstep" VALUE="on" {{half_step}}></TD></TR>
<TR><TD CLASS="LABEL">Zoomed Menu</TD><TD><INPUT TYPE="CHECKBOX" NAME="zoom" VALUE="on" {{zoom}}></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save display settings"></TH></TR>
</TABLE></FORM>)HTML";

static const char pageConfigTime[] PROGMEM = R"HTML(<H1>Date &amp; Time</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" CLASS="CONFIGFORM" ONSUBMIT="browserDateTime(true)">
<INPUT TYPE="HIDDEN" NAME="section" VALUE="time">
<TABLE COLUMNS="2">
<TR><TD CLASS="LABEL">Use Browser Date/Time</TD><TD><INPUT TYPE="CHECKBOX" ID="browserdatetime" ONCHANGE="browserDateTime()"></TD></TR>
<TR><TD CLASS="LABEL">UTC Date/Time</TD><TD><INPUT TYPE="TEXT" ID="datetime" NAME="datetime" PLACEHOLDER="YYYY-mm-dd HH:MM:SS"></TD></TR>
<TR><TD CLASS="LABEL">Time Zone</TD><TD><SELECT ID="utcoffset" NAME="utcoffset">{{{utc_options}}}</SELECT></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save time settings"></TH></TR>
</TABLE></FORM>
<SCRIPT>function browserDateTime(submit){const enabled=document.getElementById('browserdatetime').checked;const d=document.getElementById('datetime');const z=document.getElementById('utcoffset');if(enabled){const now=new Date();d.value=now.toISOString().slice(0,19).replace('T',' ');const minutes=-now.getTimezoneOffset();for(const option of z.options)if(Number(option.dataset.minutes)===minutes)z.value=option.value;}d.disabled=z.disabled=enabled&&!submit;}</SCRIPT>)HTML";

static const char pageConfigSplash[] PROGMEM = R"HTML(<H1>Splash Screen</H1>
{{{navigation}}}
<FORM ACTION="/setconfig" METHOD="POST" ENCTYPE="multipart/form-data" CLASS="CONFIGFORM">
<INPUT TYPE="HIDDEN" NAME="section" VALUE="splash">
<TABLE COLUMNS="2">
<TR><TD CLASS="LABEL">Current Image</TD><TD>{{{splash}}}</TD></TR>
<TR><TD CLASS="LABEL">Upload PNG</TD><TD><INPUT TYPE="FILE" NAME="splash" ACCEPT=".png"><BR><SMALL>Required resolution: {{resolution}} pixels; maximum size: 512 KB</SMALL></TD></TR>
<TR><TD CLASS="LABEL">Delete Image</TD><TD><INPUT TYPE="CHECKBOX" NAME="deletesplash" VALUE="on"></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save splash screen"></TH></TR>
</TABLE></FORM>)HTML";

static const char pageConfigWeather[] PROGMEM = R"HTML(<H1>Weather</H1>
{{{navigation}}}
<FORM ACTION="/weather/save" METHOD="POST" CLASS="CONFIGFORM"><TABLE COLUMNS="2">
<TR><TD CLASS="LABEL">German postal code</TD><TD><INPUT TYPE="TEXT" NAME="postal" INPUTMODE="NUMERIC" PATTERN="[0-9]{5}" MINLENGTH="5" MAXLENGTH="5" REQUIRED VALUE="{{postal}}"></TD></TR>
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save weather location"></TH></TR>
</TABLE></FORM>)HTML";

static const char pageConfigNews[] PROGMEM = R"HTML(<H1>RSS News</H1>
{{{navigation}}}
<FORM ACTION="/news/save" METHOD="POST" CLASS="CONFIGFORM"><TABLE COLUMNS="2">
<TR><TH COLSPAN="2" CLASS="HEADING">RSS/Atom feeds (4 slots)</TH></TR>
{{{news_feeds}}}
<TR><TH COLSPAN="2" CLASS="HEADING"><INPUT TYPE="SUBMIT" VALUE="Save RSS feeds"></TH></TR>
</TABLE><P><SMALL>Enter your own RSS/Atom URLs. The reader uses available content:encoded or description text.</SMALL></P></FORM>)HTML";

static const char pageSplash[] PROGMEM = R"HTML(<IMG SRC="/splash.png?{{version}}" ALT="Current splash screen" STYLE="max-width:100%;height:auto;">)HTML";
static const char pageConfigThemeOption[] PROGMEM = R"HTML(<OPTION VALUE="{{value}}" {{selected}}>{{label}}</OPTION>)HTML";
static const char pageConfigUtcOption[] PROGMEM = R"HTML(<OPTION VALUE="{{value}}" DATA-MINUTES="{{minutes}}" {{selected}}>{{label}}</OPTION>)HTML";
static const char pageConfigNewsFeed[] PROGMEM = R"HTML(<TR><TD CLASS="LABEL">Feed {{index}} <INPUT TYPE="CHECKBOX" NAME="newsenabled{{slot}}" VALUE="on" {{enabled}}> Enabled</TD><TD><INPUT TYPE="URL" NAME="newsurl{{slot}}" VALUE="{{url}}" PLACEHOLDER="https://example.com/rss.xml"></TD></TR>)HTML";

#endif // PAGE_CONFIG_H
