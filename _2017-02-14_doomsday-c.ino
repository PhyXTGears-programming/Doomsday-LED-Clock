#include <Time.h>
#include <TimeLib.h>

/*

  Udp NTP Client

  Get the time from a Network Time Protocol (NTP) time server
  Demonstrates use of UDP sendPacket and ReceivePacket
  For more on NTP time servers and the messages needed to communicate with them,
  see http://en.wikipedia.org/wiki/Network_Time_Protocol

  created 4 Sep 2010
  by Michael Margolis
  modified 9 Apr 2012
  by Tom Igoe
  updated for the ESP8266 12 Apr 2015
  by Ivan Grokhotkov

  This code is in the public domain.

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define COUNTDOWN true

char ssid[] = "Phyxt Gears";  //  your network SSID (name)
char pass[] = "logomotion";       // your network password
int seconds = 99; // number of seconds between ntp server queries
// 600 is tested

// 59 days before march
// today is 80
//                YYYY, DD, HH, MM, SS
int endTime[5] = {2017, 80, 18, 00, 00};
int endUnixTime;

int convert(int y, int d, int h, int m, int s) {
  int UnixTime = (y - 1970) * 365 * 24 * 60 * 60;
  UnixTime += d * 24 * 60 * 60;
  UnixTime += (h + 5) * 60 * 60;
  UnixTime += m * 60;
  UnixTime += s;
  return UnixTime;
}

//bool doTimer = true;

unsigned int localPort = 2390;      // local port to listen for UDP packets

String timeString;

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

int timeLeft[4];

void serln(String p, int imp = 0);
void pixln(String p, int imp = 0);

void setup() {
  Serial.begin(115200);
  endUnixTime = convert(endTime[0], endTime[1], endTime[2], endTime[3], endTime[4]);
  serln("\n", 2);
  if (seconds < 4) {
    seconds = 4;
    serln("Seconds must be at least 4, changing to 4.", true);
  }
  serln("\n", 2);

  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  serln(ssid, 2);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  serln("", true);

  serln("WiFi connected", 2);
  serln("IP address: ", 2);
  serln(String(WiFi.localIP()), 2);

  serln("Starting UDP", 2);
  udp.begin(localPort);
  Serial.print("Local port: ");
  serln(String(udp.localPort()), 2);
}

int Day;
int Hour;
int Minute;
int Second;

// long sID = (60L * 60L) * 24L; // seconds in a day (Minute * 60 * 24)
// long sIY = (sID * 7L) * 52L; // seconds in a year (Day * 7 * 52)
// long sBEN = sIY * 17167; // Seconds from epoch to January 1, 2017

// ==================================================================================================================================================

void loop() {
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);

  printTime();

  int cb = udp.parsePacket();
  if (!cb) {
    serln("no packet yet", 2);
  }
  else {
    Serial.print("packet received, length=");
    serln(String(cb), 1);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    serln(String(secsSince1900));

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    serln(String(epoch), 1);

    //-=-=-=-=-=-=-=-=-=-=-=-=================================================================================

    epoch = endUnixTime - epoch;

    // Day
    // Day = ((epoch - sBEN) % sIY) / 86400L;
    // Day = (epoch - sBEN) / 86400L;
    // Day = Day - 31;

    epoch = epoch - (5 * 60 * 60);

    Day = day(epoch);

    // serln((epoch / sIY) / 86400L);
    serln("The day is: " + String(Day), 1);

    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Hour = (epoch % 86400L) / 3600;
    Serial.print(String(Hour)); // print the hour (86400 equals secs per day)
    Serial.print(':');
    Minute = (epoch  % 3600) / 60;
    if ( Minute < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }

    Serial.print(String(Minute)); // print the minute (3600 equals secs per hour)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Second = epoch % 60;
    serln(String(Second), 1); // print the second
  }
  runPanel(Day, Hour, Minute, Second); //--------------------------------------------------
  // wait ten seconds before asking for the time again
  // delay(10000);
}

//==============end loop====================================================================================================================================================

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  serln("sending NTP packet...", 1);
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

//===================================================================================================================================================================

String secondString;
String minuteString;
String hourString;
String dayString;

int dayl;
int houl;
int minl;
int secl;

//int *t;

void runPanel(int d, int h, int m, int s) {
  int da = d;
  int ho = h;
  int mi = m;
  int se = s + 1;
  serln("\n");
  for (int i = 1; i <= (seconds + 1); i += 1) {
    serln(String(millis()));
    if (COUNTDOWN) {
      se -= 1;
    } else {
      se += 1;
    }
    //  under
    if (se < 0) {
      se = se + 60;
      mi -= 1;
    }
    if (mi < 0) {
      mi = mi + 60;
      ho -= 1;
    }
    if (ho < 0) {
      ho = ho + 24;
      da -= 1;
    }
    //  over
    if (se > 60) {
      se = se - 60;
      mi += 1;
    }
    if (mi > 60) {
      mi = mi - 60;
      ho += 1;
    }
    if (ho > 60) {
      ho = ho - 24;
      da += 1;
    }

    //    dayl = 21 - da;
    //    houl = 23 - ho;
    //    minl = 59 - mi;
    //    secl = 59 - se;

    //    dayl = da;
    //    houl = ho;
    //    minl = mi;
    //    secl = se;

    //    getTimeLeft(da, ho, mi, se);

    //    if (doTimer) {
    //      dayl = timeLeft[0];
    //      houl = timeLeft[1];
    //      minl = timeLeft[2];
    //      secl = timeLeft[3];
    //    } else {
    //      dayl = da;
    //      houl = ho;
    //      minl = mi;
    //      secl = se;
    //    }

    if (se < 10) {
      secondString = "0" + String(se);
    }
    else {
      secondString = String(se);
    }
    // ==============================================
    if (mi < 10) {
      minuteString = "0" + String(mi);
    }
    else {
      minuteString = String(mi);
    }
    // ==============================================
    if (ho < 10) {
      hourString = "0" + String(ho);
    }
    else {
      hourString = String(ho);
    }
    // ==============================================
    if (da < 10) {
      dayString = "0" + String(da);
    }
    else {
      dayString = String(da);
    }

    if (timeLeft[0] == -1 && COUNTDOWN) {
      dayString = "Time is past";
    }

    timeString = "\n" + String(i) + ": " + dayString + ":" + hourString + ":" + minuteString + ":" + secondString + "\n";

    extraCodeWithTime(da, ho, mi, se);

    if (i <= seconds) {
      printTime();
      serln(String(millis()));

      // wait 1 second
      if (i % 3 == 0) {
        serln("less delay");
        delay(999);
      }
      else {
        delay(1000);
      }
    }
  }
  serln("\n");
}

// ======================================================================================================================================================================

void pix (String p) {
  ser(p);
  pixelOut(p);
}

void ser (String p) {
  Serial.print(p);
}

void pixelOut (String p) {
  //
}

// ==========================

void pixln (String p, int imp) {
  serln(p, imp);
}

void serln (String p, int imp) {
  if (imp != 0) {
    Serial.println(p);
  }
}

void pixln () {
  //
}

void printTime () {
  serln(timeString, 2);
}

void extraCodeWithTime(int d, int h, int m, int s) {
  // Other stuff with time.
  // Runs in runPanel().
  // Skips a second during ntp query
}

//int r[4];

//void getTimeLeft(int d, int h, int m, int s) {
//  int dL = endTime[0] - d;
//  int hL = endTime[1] - h;
//  int mL = endTime[2] - m;
//  int sL = endTime[3] - s;
//  if (sL < 0) {
//    mL = mL - 1;
//    sL = sL % 60;
//  }
//  if (mL < 0) {
//    hL = hL - 1;
//    mL = mL % 60;
//  }
//  if (hL < 0) {
//    dL = dL - 1;
//    hL = hL % 24;
//  }
//  if (dL < 0) {
//    for (int i = 0; i <= 3; i++) {
//      timeLeft[i] = -1;
//    }
//  } else {
//    timeLeft[0] = dL;
//    timeLeft[1] = hL;
//    timeLeft[2] = mL;
//    timeLeft[3] = sL;
//  }
//}
