#define numpadPulse 2
#define numpadData 3
#define doorServoPin 5
#define lockServoPin 4
#define RST_PIN 9
#define SS_PIN 10

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;
Servo lockServo;

String password = "1234";
const int maxNumberOfWrong = 3;
const int hardLockTime = 30;  //second
const String message[] = { " Unlock Door", " Change Pass", " Assign RFID", " Remove RFID", "  Lock Door" };

int numberOfWrong = 0;
String prePrintPassword = "*";
String enterPassword = "";
bool locking = true;
bool hardLockMode = false;

//new pass
String newPass = "";

//key pad
int oldKey = 0;
bool pressEnter = false;
bool chosingMode = true;
int mode = 0;
int preMode = -1;
long nextTimeUnlock = 0;
long nextSendTime = 0;

//esp
String inputString = "";
bool isReadString = false;

//for play sound
// #define NOTE_C4 262
// #define NOTE_D4 294
// #define NOTE_E4 330
#define NOTE_C4 2093
#define NOTE_D4 2349
#define NOTE_E4 2637
#define soundPin 8
int melody[] = { NOTE_C4, NOTE_D4, NOTE_E4 };
int noteDurations[] = { 8, 8, 8 };

void setup() {
  Serial.begin(115200);
  pinMode(numpadPulse, OUTPUT);
  pinMode(numpadData, INPUT);

  //read password from EEPROM
  if (EEPROM.read(0) != 0xFF) {  // read data if available in EEPROM
    password = readString(0);
  }


  //init servo
  lockServo.attach(lockServoPin);
  unlockDoor();
  doorServo.attach(doorServoPin);
  closeDoor();
  lockDoor();

  //init lcd
  lcd.init();
  lcd.backlight();

  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522 card
}

void loop() {
  //if exceed number of attempt
  if (hardLockMode) {
    //set next time to unlock
    if (preMode != mode) {
      nextTimeUnlock = millis() + hardLockTime * 1000;
      preMode = mode;
      // Serial.println(millis());
      // Serial.println(nextTimeUnlock);
    }
    //if out of time
    if (nextTimeUnlock <= millis()) {
      hardLockMode = false;
      chosingMode = true;
      preMode = -1;
      numberOfWrong = 0;
      return;
    }
    //else print remain time
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wait ");
    lcd.print((nextTimeUnlock - millis()) / 1000);
    lcd.print("s");
    lcd.setCursor(0, 1);
    lcd.print("to continue!");
    delay(1000);
    return;
  }

  //send locking info to esp
  if (nextSendTime < millis()) {
    //read from esp
    while (Serial.available()) {
      // get the new byte:
      char inChar = (char)Serial.read();
      // Serial.println(inChar);
      if (inChar == '\n') {
        isReadString = true;
      } else {
        inputString += inChar;
      }
    }

    //if receive message from esp
    if (isReadString) {
      isReadString = false;
      if (inputString == "ESP") {
        preMode = -1;
        chosingMode = true;
        //send to esp
        Serial.print("\nUNO2\n");
        unlockDoor();
        closeDoor();
        lockDoor();
      }
      inputString = "";
    }

    //send locking status to esp
    if (locking) {
      //send to esp
      Serial.print("\nUNO7\n");
    } else {
      //send to esp
      Serial.print("\nUNO8\n");
    }
    nextSendTime = millis() + 1000;
  }

  int Key = getkey();
  pressEnter = (Key == 15);
  //function for each key
  if (Key != oldKey) {
    if (Key != 0) {
      switch (Key) {
        case 4:
          //next function key
          {
            if (chosingMode) {
              mode++;
              if (mode > 3) mode = 0;
            } else preMode = -1;
            chosingMode = true;
            break;
          }
        case 8:
          //previous function key
          {
            if (chosingMode) {
              mode--;
              if (mode < 0) mode = 3;
            } else preMode = -1;
            chosingMode = true;
            break;
          }
        case 12:
          //open door key
          {
            //must unlock before open door
            if (locking == false) {
              openDoor();
            } else {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Must Unlock");
              lcd.setCursor(0, 1);
              lcd.print("before Open!");
              preMode = -1;
              delay(1500);
            }
            break;
          }
        case 16:
          //close door key
          {
            //must unlock before close door
            if (locking == false) {
              closeDoor();
            } else {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Must Unlock");
              lcd.setCursor(0, 1);
              lcd.print("before Close!");
              preMode = -1;
              delay(1500);
            }
            break;
          }
        case 13:
          //delete key
          {
            if (enterPassword.length()) {
              //delete last character
              enterPassword.remove(enterPassword.length() - 1);
            } else {
              peepSound();
            }
            break;
          }
        case 15:
          //enter key
          {
            //if are chosing mode, then press enter to enter this mode
            if (chosingMode) {
              chosingMode = false;
              preMode = -1;
              pressEnter = false;
            }
            break;
          }
        default:
          //all of other number key (0 -> 9)
          {
            int num = Key;
            if (num == 14) num = 0;
            if (num > 7) num--;
            if (num > 3) num--;
            //if entering password length < password length then add new number to last position of entering password
            if (enterPassword.length() < password.length()) {
              enterPassword += char(48 + num);
            } else {
              peepSound();
            }
            break;
          }
      }
    }
  }
  oldKey = Key;

  //chosing function or doing function
  if (chosingMode) {
    chosingFunc();
  } else {
    //assign each mode
    switch (mode) {
      case 0:
        {
          if (locking) tryUnlock();
          else {
            chosingMode = true;
            //send to esp
            Serial.print("\nUNO2\n");
            unlockDoor();
            closeDoor();
            lockDoor();
            preMode = -1;
          }
          break;
        }
      case 1:
        {
          tryToChangePass();
          break;
        }
      case 2:
        {
          tryAssignCard();
          break;
        }
      case 3:
        {
          tryRemoveCard();
          break;
        }
      default:
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("a");
          delay(1000);
          break;
        }
    }
  }

  delay(10);
}

//this function will print out main screen chosing mode
//press next func or prev func to display another function name
void chosingFunc() {
  byte messageID = mode;
  if (mode != preMode) {
    //if door is unlock -> will show lock function
    //if door is lock -> will show unlock functions
    if (mode == 0 && !locking) {
      messageID = 4;
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("<");
    lcd.print(message[messageID]);
    lcd.setCursor(15, 0);
    lcd.print(">");
    preMode = mode;
  }
}

void writeString(char address, String data) {
  int _size = data.length();
  int i;
  for (i = 0; i < _size; i++) {
    EEPROM.write(address + i, data[i]);
  }
  EEPROM.write(address + _size, '\0');  //Add termination null character for String Data
}


String readString(char address) {
  char data[100];  //Max 100 Bytes
  int length = 0;
  unsigned char tempChar;
  tempChar = EEPROM.read(address);
  while (tempChar != '\0' && length < 500) {  //Read until null character
    tempChar = EEPROM.read(address + length);
    data[length] = tempChar;
    length++;
  }
  data[length] = '\0';
  return String(data);
}


bool tryWriteRFID(String str) {
  // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }

  // Serial.print(F("Card UID:"));    //Dump UID
  // for (byte i = 0; i < mfrc522.uid.size; i++) {
  //   Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
  //   Serial.print(mfrc522.uid.uidByte[i], HEX);
  // }
  // Serial.print(F(" PICC type: "));   // Dump PICC type
  // MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  // Serial.println(mfrc522.PICC_GetTypeName(piccType));

  byte buffer[34];
  byte block;
  MFRC522::StatusCode status;
  byte len;

  // Ask personal data: First name
  // Serial.println(F("Type First name, ending with #"));
  // len = Serial.readBytesUntil('#', (char *) buffer, 20) ; // read first name from serial
  for (byte i = 0; i < str.length(); i++) buffer[i] = str[i];
  for (byte i = str.length(); i < 20; i++) buffer[i] = ' ';  // pad with spaces

  block = 4;
  //Serial.println(F("Authenticating using key A..."));
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("PCD_Authenticate() failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Write block
  status = mfrc522.MIFARE_Write(block, buffer, 16);
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("MIFARE_Write() failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  // else Serial.println(F("MIFARE_Write() success: "));

  block = 5;
  //Serial.println(F("Authenticating using key A..."));
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("PCD_Authenticate() failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }

  // Write block
  status = mfrc522.MIFARE_Write(block, &buffer[16], 16);
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("MIFARE_Write() failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return false;
  }
  // else Serial.println(F("MIFARE_Write() success: "));


  // Serial.println(" ");
  mfrc522.PICC_HaltA();       // Halt PICC
  mfrc522.PCD_StopCrypto1();  // Stop encryption on PCD
  delay(10);
  return true;
}

//write "*" to card
void tryRemoveCard() {
  //init if not yet
  if (preMode != mode) {
    prePrintPassword = "*";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Put the Card");
    lcd.setCursor(0, 1);
    lcd.print("next to the Lock");
    preMode = mode;
  }
  //write "*" to card
  if (tryWriteRFID("*")) {
    //send to esp
    Serial.print("\nUNO6\n");
    peepSound();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Remove RFID Card");
    lcd.setCursor(0, 1);
    lcd.print("Successfully!");
    delay(2000);
    preMode = -1;
    chosingMode = true;
  }
}

//write password to card
void tryAssignCard() {
  //Try to unlock
  if (locking) {
    tryToUnlockFirst();
    return;
  }
  //init if not yet
  if (preMode != mode) {
    prePrintPassword = "*";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Put the Card");
    lcd.setCursor(0, 1);
    lcd.print("next to the Lock");
    preMode = mode;
  }
  //write password to card
  if (tryWriteRFID(password)) {
    //send to esp
    Serial.print("\nUNO5\n");
    peepSound();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Assign RFID Card");
    lcd.setCursor(0, 1);
    lcd.print("Successfully!");
    delay(2000);
    preMode = -1;
    chosingMode = true;
  }
}

String tryReadRFID() {
  //return string
  String ans = "";

  // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  //some variables we need
  byte block;
  byte len;
  MFRC522::StatusCode status;

  //-------------------------------------------

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return "";
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return "";
  }

  // Card detected

  // Serial.println(F("**Card Detected:**"));

  //-------------------------------------------

  // mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); //dump some details about the card

  //mfrc522.PICC_DumpToSerial(&(mfrc522.uid));      //uncomment this to see all blocks in hex

  //-------------------------------------------

  // Serial.print(F("Name: "));

  byte buffer1[18];

  block = 4;
  len = 18;

  //------------------------------------------- GET FIRST NAME
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(mfrc522.uid));  //line 834 of MFRC522.cpp file

  //Failed Authentication
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("Authentication failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return "";
  }

  //Failed Reading
  status = mfrc522.MIFARE_Read(block, buffer1, &len);
  if (status != MFRC522::STATUS_OK) {
    // Serial.print(F("Reading failed: "));
    // Serial.println(mfrc522.GetStatusCodeName(status));
    return "";
  }

  //PRINT FIRST NAME
  for (uint8_t i = 0; i < 16; i++) {
    if (buffer1[i] != 32) {
      if (char(buffer1[i]) == ' ') continue;
      ans += (char)buffer1[i];
      // Serial.write(buffer1[i]);
    }
  }
  // Serial.print(" ");

  //---------------------------------------- GET LAST NAME

  // byte buffer2[18];
  // block = 1;

  // status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(mfrc522.uid)); //line 834
  // if (status != MFRC522::STATUS_OK) {
  //   Serial.print(F("Authentication failed: "));
  //   Serial.println(mfrc522.GetStatusCodeName(status));
  //   return;
  // }

  // status = mfrc522.MIFARE_Read(block, buffer2, &len);
  // if (status != MFRC522::STATUS_OK) {
  //   Serial.print(F("Reading failed: "));
  //   Serial.println(mfrc522.GetStatusCodeName(status));
  //   return;
  // }

  //PRINT LAST NAME
  // for (uint8_t i = 0; i < 16; i++) {
  //   Serial.write(buffer2[i] );
  // }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(10);
  return ans;
}

void tryToUnlockFirst() {
  //if locking, try to unlock
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Try To Unlock");
  lcd.setCursor(0, 1);
  lcd.print("Door First!");
  delay(2000);
  mode = 0;
  preMode = -1;
  prePrintPassword = "*";
}

void tryUnlock() {
  //init if not yet
  if (preMode != mode) {
    prePrintPassword = "*";
    enterPassword = "";
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter password:");
    preMode = mode;
  }
  printPassword();

  //try read rfid card
  String rfidPass = tryReadRFID();
  if (rfidPass != "") {
    //if correct rfid card
    if (rfidPass == password) {
      correctPassword();
      return;
    } else {
      wrongPassword("Wrong Card!");
      return;
    }
  }


  //if press Enter
  if (pressEnter) {
    //if enough length
    if (pressEnter && password.length() == enterPassword.length()) {
      //if true password
      if (password == enterPassword) {
        correctPassword();
      } else {
        //if wrong password
        wrongPassword("Wrong Password!");
      }
    }
    pressEnter = false;
  }
}

void correctPassword() {
  //send to esp
  Serial.print("\nUNO1\n");
  numberOfWrong = 0;
  unlockDoor();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Door Unlocked!");
  delay(1000);
  chosingMode = true;
  preMode = -1;
}

void wrongPassword(String msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
  lcd.setCursor(0, 1);
  numberOfWrong++;
  //number of remain try
  int tmp = maxNumberOfWrong - numberOfWrong;
  //if have no try ever
  if (tmp == 0) {
    //send to esp
    setNewPass();
    Serial.print("\nUNO3");
    Serial.print(newPass + "\n");
    password = newPass;
    hardLockOn();
    newPass = "";
    return;
  }
  //send to esp
  Serial.print("\nUNO0\n");
  lcd.print(tmp);
  if (tmp >= 2) lcd.print(" tries");
  else lcd.print(" try");
  lcd.print(" remain!");
  errorSound();
  delay(2000);
  preMode = -1;
}

//init some varible for tryToChangPass function

//new length of password
byte newLength = -1;
//if not print enter password message yet, this varible will be false
bool enterLengthMess = false;

void tryToChangePass() {
  //Try to unlock before change password
  if (locking) {
    tryToUnlockFirst();
    return;
  }

  //first setup, init new password
  if (preMode != mode) {
    enterPassword = "";
    prePrintPassword = "*";
    newLength = -1;
    enterLengthMess = false;
    preMode = mode;
  }

  //if have not enter length yet
  if (newLength < 1 || newLength > 16) {

    //if not show enter length message yet
    if (!enterLengthMess) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter length: ");
      enterLengthMess = true;
    } else {
      //if are showing enter length of password

      //if entered but not valid length
      if (enterPassword != "" && (enterPassword.toInt() < 1 || enterPassword.toInt() > 16)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Length must from");
        lcd.setCursor(0, 1);
        lcd.print("1 to 16!");
        delay(2000);
        enterLengthMess = false;
        enterPassword = "";

        //if valid length
      } else {
        //if valid length and enter is pressing
        if (pressEnter) {
          //convert input string to integer
          newLength = enterPassword.toInt();
          pressEnter = false;

          //if value ok then set new length to password
          if (newLength >= 1 && newLength <= 16) {
            //init new password
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Enter new Pass:");
            password = "";
            enterPassword = "";
            for (byte i = 1; i <= newLength; i++) {
              password += '*';
            }
          }
          //if valid length and enter not pressing
        } else {
          //preprint password
          if (prePrintPassword != enterPassword) {
            lcd.setCursor(14, 0);
            lcd.print("  ");
            lcd.setCursor(14, 0);
            lcd.print(enterPassword);
            prePrintPassword = enterPassword;
          }
        }
      }
    }
  } else {
    //if entered length yet
    printPassword();
    //if pressEnter
    if (pressEnter) {
      //if enough length of password -> set new password
      if (newLength == enterPassword.length()) {
        //send to esp
        Serial.print("\nUNO4\n");
        password = enterPassword;
        writeString(0, password);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Update Password");
        lcd.setCursor(0, 1);
        lcd.print("Successfully!");
        delay(2000);
        chosingMode = true;
        preMode = -1;
      }
      pressEnter = false;
    }
  }
}

//if no try remain(exceed number of maxTryEnterPassword)
//it will turn on the hard lock mode(play alert sound and lock for a period time)
void hardLockOn() {
  closeDoor();
  lockDoor();
  preMode = -1;
  //play alert for 5s (5*5 = = 25)
  for (byte i = 0; i < 35; i++) {

    if (((i / 5) & 1) == 0) {
      if (preMode != mode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Exceed number of");
        lcd.setCursor(0, 1);
        lcd.print("Attempts!");
        preMode = mode;
      }
    } else {
      preMode = -1;
      lcd.clear();
    }

    peepSound();
  }
  preMode = -1;
  hardLockMode = true;
}

//call this function if want to lock door
void lockDoor() {
  locking = true;
  lockServo.write(70);
  chosingMode = true;
  delay(300);
}

//call this function if want to unlock door
void unlockDoor() {
  locking = false;
  lockServo.write(180);
  openSound();
  // delay(300);
}

//call this function if want to close door
void closeDoor() {
  doorServo.write(152);
  delay(300);
  doorServo.write(147);
  delay(100);
}

//call this function if want to open door
void openDoor() {
  doorServo.write(45);
  delay(300);
  doorServo.write(55);
  delay(100);
}

void peepSound() {
  tone(soundPin, 2000);
  delay(100);
  noTone(soundPin);
}

void errorSound() {
  int noteDuration = 5000 / noteDurations[0];
  tone(soundPin, melody[0], noteDuration);
  int pauseBetweenNotes = noteDuration * 1.30;
  delay(pauseBetweenNotes);
  noTone(soundPin);
}

void openSound() {
  for (int thisNote = 0; thisNote < 3; thisNote++) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(soundPin, melody[thisNote], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(soundPin);
  }
}

void closeSound() {
  for (int thisNote = 2; thisNote >= 0; thisNote--) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(soundPin, melody[thisNote], noteDuration);
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(soundPin);
  }
}

//print out password to the screen
void printPassword() {
  //if password same with previous, will not reprint
  if (prePrintPassword == enterPassword) return;
  prePrintPassword = enterPassword;
  //use firstPos to center password to screen
  byte firstPos = (16 - password.length()) / 2;
  //print number
  lcd.setCursor(firstPos, 1);
  lcd.print(enterPassword);
  //replace missing character by *
  for (byte cnt = 1; cnt <= password.length() - enterPassword.length(); cnt++) {
    lcd.print('*');
  }
}

//get the pressing button
byte getkey(void) {
  byte num = 0;
  //check each button, if press then assign num to this key
  for (byte cnt = 1; cnt <= 16; cnt++) {
    digitalWrite(numpadPulse, LOW);
    if (digitalRead(numpadData) == 0) num = cnt;
    digitalWrite(numpadPulse, HIGH);
  }
  //return the pressing key or 0 if not pressing
  return num;
}

void setNewPass() {
  for (int i = 0; i < 4; i++) {
    int digitPass = random(0, 10);
    newPass += String(digitPass);
  }
}