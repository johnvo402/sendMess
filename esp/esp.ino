#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <RtcDS1302.h>
#include <SoftwareSerial.h>
#include <Arduino.h>
#include <ThreeWire.h>

#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#elif __has_include(<WiFiNINA.h>)
#include <WiFiNINA.h>
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#elif __has_include(<WiFiS3.h>)
#include <WiFiS3.h>
#endif

#include <ESP_Mail_Client.h>

#define WIFI_SSID "Quang Vinh"
#define WIFI_PASSWORD "999999999"

/** For Gmail, the app password will be used for log in
 *  Check out https://github.com/mobizt/ESP-Mail-Client#gmail-smtp-and-imap-required-app-passwords-to-sign-in
 *
 * For Yahoo mail, log in to your yahoo mail in web browser and generate app password by go to
 * https://login.yahoo.com/account/security/app-passwords/add/confirm?src=noSrc
 *
 * To use Gmai and Yahoo's App Password to sign in, define the AUTHOR_PASSWORD with your App Password
 * and AUTHOR_EMAIL with your account email.
 */

/** The smtp host name e.g. smtp.gmail.com for GMail or smtp.office365.com for Outlook or smtp.mail.yahoo.com */
#define SMTP_HOST "smtp.gmail.com"

/** The smtp port e.g.
 * 25  or esp_mail_smtp_port_25
 * 465 or esp_mail_smtp_port_465
 * 587 or esp_mail_smtp_port_587
 */
#define SMTP_PORT 465;  // port 465 is not available for Outlook.com

/* The log in credentials */
#define AUTHOR_EMAIL "group1se1709@gmail.com"
#define AUTHOR_PASSWORD "dxcr khen euuj xdum"

/* Recipient email address */
#define RECIPIENT_EMAIL "thuvtce170522@fpt.edu.vn"

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

// const char rootCACert[] PROGMEM = "-----BEGIN CERTIFICATE-----\n"
//                                   "-----END CERTIFICATE-----\n";

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

/* Put your SSID & Password */
// const char* ssid = "ESP_Group2";    // Enter SSID here
// const char* password = "11111111";  //Enter Password here


/* Put IP Address details */
IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);
//real time clock
ThreeWire myWire(D1, D2, D4);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
SoftwareSerial s(3, 1);

String inputString = "";
bool isReadString = false;
byte numberOfHis = 0;
int startIndex = 1;

//lock info
bool locking = true;
String newPass = "";



void setup() {
  // Serial.begin(115200);
  s.begin(115200);
  myWire.begin();
  Rtc.Begin();
  EEPROM.begin(512);

  //read number of history
  numberOfHis = EEPROM.read(0);
  startIndex = EEPROM.read(1);

  // WiFi.softAP(ssid, password);
  // WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  delay(100);



#if defined(ARDUINO_ARCH_SAMD)
  while (!Serial)
    ;
#endif

  Serial.println();

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  multi.addAP(WIFI_SSID, WIFI_PASSWORD);
  multi.run();
#else
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

  Serial.print("Connecting to Wi-Fi");

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  unsigned long ms = millis();
#endif

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (millis() - ms > 10000)
      break;
#endif
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /*  Set the network reconnection option */
  MailClient.networkReconnect(true);

  // The WiFi credentials are required for Pico W
  // due to it does not have reconnect feature.
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  MailClient.clearAP();
  MailClient.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);
  server.on("/", handle_OnConnect);
  server.on("/lockDoor", handle_lockDoor);
  server.on("/removeAll", handle_removeAll);
  server.onNotFound(handle_NotFound);

  server.begin();
  // Serial.println("HTTP server started");
  set_time();
}

void loop() {

  while (s.available()) {
    // get the new byte:
    char inChar = (char)s.read();
    // Serial.println(inChar);
    if (inChar == '\n') {
      isReadString = true;
    } else {
      inputString += inChar;
    }
  }

  // Serial.println(1);
  // delay(200);

  //if receive data from arduino uno
  if (isReadString) {
    isReadString = false;
    if (inputString.length() > 4 && inputString[0] == 'U' && inputString[1] == 'N' && inputString[2] == 'O' && inputString[3] == '3') {
      //get status
      newPass = inputString.substring(4, 8);
      sendEmail();
    }
    inputString = inputString.substring(0, 4);
    inputString.trim();
    if (inputString.length() == 4 && inputString[0] == 'U' && inputString[1] == 'N' && inputString[2] == 'O') {
      //get status
      byte stt = inputString[3] - 48;
      //update info
      if (stt >= 7) {
        switch (stt) {
          case 7:
            {
              locking = true;
              break;
            }
          case 8:
            {
              locking = false;
              break;
            }
        }
        //add new info
      } else {
        if (numberOfHis < 63) {
          numberOfHis++;
          EEPROM.write(0, numberOfHis);
          EEPROM.commit();
          writeHis(numberOfHis * 8, stt);
          // Serial.println(EEPROM.read(0));
          // Serial.println(readDate(numberOfHis * 8));
        } else {
          writeHis(startIndex * 8, stt);
          startIndex++;
          EEPROM.write(1, startIndex);
          EEPROM.commit();
        }
      }
    }
    inputString = "";
    newPass = "";
  }
  server.handleClient();
  delay(100);
}

void set_time()
{
  Rtc.SetIsWriteProtected(false);
  Rtc.SetIsRunning(false);
  RtcDateTime t(2023, 10,  4,  22, 00, 00);
//      Yr  Mth  Day  Hr  Min Sec  Mon
 
  Rtc.SetDateTime(t);
}



void sendEmail() {
  /* Declare the Session_Config for user defined session credentials */
  Session_Config config;

  /* Set the session config */
  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;

  /** Assign your host name or you public IPv4 or IPv6 only
   * as this is the part of EHLO/HELO command to identify the client system
   * to prevent connection rejection.
   * If host name or public IP is not available, ignore this or
   * use loopback address "127.0.0.1".
   *
   * Assign any text to this option may cause the connection rejection.
   */
  config.login.user_domain = F("127.0.0.1");

  /** If non-secure port is prefered (not allow SSL and TLS connection), use
   *  config.secure.mode = esp_mail_secure_mode_nonsecure;
   *
   *  If SSL and TLS are always required, use
   *  config.secure.mode = esp_mail_secure_mode_ssl_tls;
   *
   *  To disable SSL permanently (use less program space), define ESP_MAIL_DISABLE_SSL in ESP_Mail_FS.h
   *  or Custom_ESP_Mail_FS.h
   */
  // config.secure.mode = esp_mail_secure_mode_nonsecure;

  /*
  Set the NTP config time
  For times east of the Prime Meridian use 0-12
  For times west of the Prime Meridian add 12 to the offset.
  Ex. American/Denver GMT would be -6. 6 + 12 = 18
  See https://en.wikipedia.org/wiki/Time_zone for a list of the GMT/UTC timezone offsets
  */
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

  /* The full message sending logs can now save to file */
  /* Since v3.0.4, the sent logs stored in smtp.sendingResult will store only the latest message logs */
  // config.sentLogs.filename = "/path/to/log/file";
  // config.sentLogs.storage_type = esp_mail_file_storage_type_flash;

  /** In ESP32, timezone environment will not keep after wake up boot from sleep.
   * The local time will equal to GMT time.
   *
   * To sync or set time with NTP server with the valid local time after wake up boot,
   * set both gmt and day light offsets to 0 and assign the timezone environment string e.g.

     config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
     config.time.gmt_offset = 0;
     config.time.day_light_offset = 0;
     config.time.timezone_env_string = "JST-9"; // for Tokyo

   * The library will get (sync) the time from NTP server without GMT time offset adjustment
   * and set the timezone environment variable later.
   *
   * This timezone environment string will be stored to flash or SD file named "/tze.txt"
   * which set via config.time.timezone_file.
   *
   * See the timezone environment string list from
   * https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
   *
   */

  /* Declare the message class */
  SMTP_Message message;
  int a = 1;
  if (a == 1) {
    /* Set the message headers */
    message.sender.name = F("Group1");
    message.sender.email = AUTHOR_EMAIL;

    /** If author and sender are not identical
  message.sender.name = F("Sender");
  message.sender.email = "sender@mail.com";
  message.author.name = F("ESP Mail");
  message.author.email = AUTHOR_EMAIL; // should be the same email as config.login.email
 */

    message.subject = F("Cảnh báo!");
    message.addRecipient(F("Someone"), RECIPIENT_EMAIL);

    String textMsg = "Nhập sai mật khẩu quá 3 lần. Vì lí do bảo mật, mật khẩu mới là: " + newPass;
    message.text.content = textMsg;

    /** If the message to send is a large string, to reduce the memory used from internal copying  while sending,
   * you can assign string to message.text.blob by cast your string to uint8_t array like this
   *
   * String myBigString = "..... ......";
   * message.text.blob.data = (uint8_t *)myBigString.c_str();
   * message.text.blob.size = myBigString.length();
   *
   * or assign string to message.text.nonCopyContent, like this
   *
   * message.text.nonCopyContent = myBigString.c_str();
   *
   * Only base64 encoding is supported for content transfer encoding in this case.
   */

    /** The Plain text message character set e.g.
   * us-ascii
   * utf-8
   * utf-7
   * The default value is utf-8
   */
    message.text.charSet = F("utf-8");

    /** The content transfer encoding e.g.
   * enc_7bit or "7bit" (not encoded)
   * enc_qp or "quoted-printable" (encoded)
   * enc_base64 or "base64" (encoded)
   * enc_binary or "binary" (not encoded)
   * enc_8bit or "8bit" (not encoded)
   * The default value is "7bit"
   */
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    // If this is a reply message
    // message.in_reply_to = "<parent message id>";
    // message.references = "<parent references> <parent message id>";

    /** The message priority
   * esp_mail_smtp_priority_high or 1
   * esp_mail_smtp_priority_normal or 3
   * esp_mail_smtp_priority_low or 5
   * The default value is esp_mail_smtp_priority_low
   */
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

    // message.response.reply_to = "someone@somemail.com";
    // message.response.return_path = "someone@somemail.com";

    /** The Delivery Status Notifications e.g.
   * esp_mail_smtp_notify_never
   * esp_mail_smtp_notify_success
   * esp_mail_smtp_notify_failure
   * esp_mail_smtp_notify_delay
   * The default value is esp_mail_smtp_notify_never
   */
    // message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

    /* Set the custom message header */
    message.addHeader(F("Message-ID: <abcde.fghij@gmail.com>"));
  }


  // For Root CA certificate verification (ESP8266 and ESP32 only)
  // config.certificate.cert_data = rootCACert;
  // or
  // config.certificate.cert_file = "/path/to/der/file";
  // config.certificate.cert_file_storage_type = esp_mail_file_storage_type_flash; // esp_mail_file_storage_type_sd
  // config.certificate.verify = true;

  // The WiFiNINA firmware the Root CA certification can be added via the option in Firmware update tool in Arduino IDE

  /* Connect to server with the session config */

  // Library will be trying to sync the time with NTP server if time is never sync or set.
  // This is 10 seconds blocking process.
  // If time reading was timed out, the error "NTP server time reading timed out" will show via debug and callback function.
  // You can manually sync time by yourself with NTP library or calling configTime in ESP32 and ESP8266.
  // Time can be set manually with provided timestamp to function smtp.setSystemTime.

  /* Set the TCP response read timeout in seconds */
  // smtp.setTCPTimeout(10);

  /* Connect to the server */
  if (!smtp.connect(&config)) {
    MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  /** Or connect without log in and log in later

     if (!smtp.connect(&config, false))
       return;

     if (!smtp.loginWithPassword(AUTHOR_EMAIL, AUTHOR_PASSWORD))
       return;
  */

  if (!smtp.isLoggedIn()) {
    Serial.println("\nNot yet logged in.");
  } else {
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
  }

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  // to clear sending result log
  // smtp.sendingResult.clear();
  newPass = "";
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()) {
    // MailClient.printf used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    MailClient.printf("Message sent success: %d\n", status.completedCount());
    MailClient.printf("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)

      MailClient.printf("Message No: %d\n", i + 1);
      MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
      MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      MailClient.printf("Recipient: %s\n", result.recipients.c_str());
      MailClient.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}

// void serialEvent() {
//   while (Serial.available()) {
//     // get the new byte:
//     char inChar = (char)Serial.read();
//     // add it to the inputString:
//     // if the incoming character is a newline, set a flag so the main loop can
//     // do something about it:
//     if (inChar == '\n') {
//       isReadString = true;
//     } else {
//       inputString += inChar;
//     }
//   }
// }

String getTime() {
  RtcDateTime now = Rtc.GetDateTime();
  char datestring[20];

  snprintf_P(datestring,
             (sizeof(datestring) / sizeof(datestring[0])),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             now.Month(),
             now.Day(),
             now.Year(),
             now.Hour(),
             now.Minute(),
             now.Second());
  return String(datestring);
}

void writeHis(int address, byte stt) {
  RtcDateTime now = Rtc.GetDateTime();
  EEPROM.write(address + 0, now.Day());
  EEPROM.write(address + 1, now.Month());
  EEPROM.write(address + 2, now.Year() / 256);
  EEPROM.write(address + 3, now.Year() % 256);
  EEPROM.write(address + 4, now.Hour());
  EEPROM.write(address + 5, now.Minute());
  EEPROM.write(address + 6, now.Second());
  EEPROM.write(address + 7, stt);
  EEPROM.commit();
}

String readDate(int address) {
  char datestring[20];

  snprintf_P(datestring,
             (sizeof(datestring) / sizeof(datestring[0])),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             EEPROM.read(address + 0),
             EEPROM.read(address + 1),
             256 * EEPROM.read(address + 2) + EEPROM.read(address + 3),
             EEPROM.read(address + 4),
             EEPROM.read(address + 5),
             EEPROM.read(address + 6));
  return String(datestring);
}

byte readStt(int address) {
  return EEPROM.read(address + 7);
}

void handle_OnConnect() {
  // Serial.println("Connect!");
  // Serial.println(numberOfHis);
  server.send(200, "text/html", SendHTML(false));
}

void handle_lockDoor() {
  locking = true;
  s.print("\nESP\n");
  server.send(200, "text/html", SendHTML(true));
}

void handle_removeAll() {
  numberOfHis = 0;
  EEPROM.write(0, 0);
  startIndex = 1;
  EEPROM.write(1, 1);
  EEPROM.commit();
  server.send(200, "text/html", SendHTML(true));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(bool removeAllURL) {
  String ptr = "";
  ptr += "<!DOCTYPE html>\n";
  ptr += "<html lang=\"en\">\n";
  ptr += "<head>\n";
  ptr += "<meta http-equiv=\"refresh\" content=\"2\">\n";
  ptr += "    <meta charset=\"UTF-8\">\n";
  ptr += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  ptr += "    <title>Smart Lock</title>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  if (removeAllURL) {
    ptr += "<script>\n";
    ptr += "window.location.href = \"/\";\n";
    ptr += "</script>\n";
  }
  ptr += "    <style>\n";
  ptr += "        html {\n";
  ptr += "            font-family: sans-serif;\n";
  ptr += "        }\n";
  ptr += "        .main {\n";
  ptr += "            display: flex;\n";
  ptr += "            flex-direction: column;\n";
  ptr += "            text-align: center;\n";
  ptr += "            align-items: center;\n";
  ptr += "        }\n";
  ptr += "        p {\n";
  ptr += "            font-weight: bold;\n";
  ptr += "            font-size: 20px;\n";
  ptr += "        }\n";
  ptr += "        button {\n";
  ptr += "            padding: 20px;\n";
  ptr += "            border: none;\n";
  ptr += "            border-radius: 12px;\n";
  ptr += "            font-size: 20px;\n";
  ptr += "            cursor: pointer;\n";
  ptr += "        }\n";
  ptr += "        .lockBtn {\n";
  ptr += "            background-color: #17a2b8;\n";
  ptr += "            color: #fff;\n";
  ptr += "        }\n";
  ptr += "        .removeBtn {\n";
  ptr += "            background-color: rgb(236, 56, 56);\n";
  ptr += "            color: #fff;\n";
  ptr += "        }\n";
  ptr += "        .green {\n";
  ptr += "            color: rgb(100, 228, 100);\n";
  ptr += "        }\n";
  ptr += "        .yellow {\n";
  ptr += "            color: #ffeb3b;\n";
  ptr += "        }\n";
  ptr += "        .red {\n";
  ptr += "            color: rgb(236, 56, 56);\n";
  ptr += "        }\n";
  ptr += "        table {\n";
  ptr += "            margin-top: 20px;\n";
  ptr += "            font-family: arial, sans-serif;\n";
  ptr += "            border-collapse: collapse;\n";
  ptr += "            width: 100%;\n";
  ptr += "        }\n";
  ptr += "        td,\n";
  ptr += "        th {\n";
  ptr += "            border: 1px solid #dddddd;\n";
  ptr += "            text-align: left;\n";
  ptr += "            padding: 8px;\n";
  ptr += "        }\n";
  ptr += "        tr:nth-child(even) {\n";
  ptr += "            background-color: rgb(248, 248, 248);\n";
  ptr += "        }\n";
  ptr += "    </style>\n";
  ptr += "    <div class=\"main\">\n";
  ptr += "        <p class=\"title\"> DOOR STATUS: \n";
  if (locking) {
    ptr += "<span class=\"green\">LOCKED</span>\n";
  } else {
    ptr += "<span class=\"red\">UNLOCKED</span>\n";
  }
  ptr += "        </p>\n";
  if (!locking) {
    ptr += "        <a href=\"/lockDoor\"><button class=\"lockBtn\">Close and Lock Door</button></a>\n";
  }
  ptr += "        <p>HISTORY</p>\n";
  //If have history
  if (numberOfHis) {
    ptr += "        <a href=\"/removeAll\"><button class=\"removeBtn\">Remove ALL</button></a>";
  }
  ptr += "        <table>\n";
  ptr += "            <tr>\n";
  ptr += "                <th>ID</th>\n";
  ptr += "                <th>ACTION</th>\n";
  ptr += "                <th>TIME</th>\n";
  ptr += "            </tr>\n";
  for (int i = 1, j = startIndex; i <= min((byte)63, numberOfHis); i++, j++) {
    if (j > 63) j = 1;
    ptr += "<tr>\n";
    ptr += "<td>" + String(i) + "</td>\n";
    switch (readStt(j * 8)) {
      case 0:
        {
          ptr += "<td class=\"yellow\">Unlock Fail</td>\n";
          break;
        }
      case 1:
        {
          ptr += "<td class=\"green\">Unlock Success</td>\n";
          break;
        }
      case 2:
        {
          ptr += "<td class=\"green\">Lock Success</td>\n";
          break;
        }
      case 3:
        {
          ptr += "<td class=\"red\">Out of attempts!</td>\n";
          break;
        }
      case 4:
        {
          ptr += "<td class=\"green\">Change Password Success</td>\n";
          break;
        }
      case 5:
        {
          ptr += "<td class=\"green\">Assign a new Card Success</td>\n";
          break;
        }
      case 6:
        {
          ptr += "<td class=\"green\">Remove a Card Success</td>\n";
          break;
        }
      default:
        {
          ptr += "<td>Unknown</td>\n";
          break;
        }
    }
    ptr += "<td>" + readDate(j * 8) + "</td>\n";
    ptr += "</tr>\n";
  }
  ptr += "        </table>\n";
  ptr += "    </div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}