/*
    This file is part of the LocoBridge project.
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// This sketch requires the LocoNet ad Adafruit SSD1306 libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LocoNet.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; alternatively use the Wire.>i2x_scanner example sketch from the Arduino IDE
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//LN TX pin
#define TX_PIN   5
//#define LN_TX_ECHO 0
static LnBuf lnTXBuffer;
static lnMsg *lnPacket;

//set to 1 to hide blocked messages
#define HIDE_BLOCKED_MESSAGES 1

//text size 1 gives 8 display lines; size 2 reduces that to 4
#define DISPLAY_LINES 8
#define TEXT_SIZE 1

//pins for external serial RX/TX LED pins
#define RX_LED_PIN 12
#define TX_LED_PIN 11

//programming pin to tell the arduino to close the serial port
#define PROGRAMMING_PIN A0

#define BAUD_RATE 57600
bool serialOpen = false;

bool displayAvailable = false;

enum DisplayStates { UpdateDisplayBuffer, SendDisplayBuffer};
DisplayStates displayState = UpdateDisplayBuffer;

enum MessageStates { SerialPass, SerialBlock, SerialError, LocoPass, LocoBlock, LocoError};

String displayStrings[DISPLAY_LINES];
short displayLineIndex = 0;

unsigned long lastDisplayUpdateMs = 0;
unsigned long lastSerialReceiveMs = 0;

#define SERIAL_BYTES_LIMIT 16
byte serialBytes[SERIAL_BYTES_LIMIT];
byte serialByteIndex = 0;

void setup() {
  // put your setup code here, to run once:

  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);

 //Check if the display is present on the i2c bus
  Wire.begin();
  Wire.beginTransmission(SCREEN_ADDRESS);
  if (!Wire.endTransmission()) { //someone responded under the display address
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      displayAvailable = true;
    }
  }
  else {
    displayAvailable = false;
  }


  //using the built in led to indicate display detection
  if (displayAvailable) {
    display.clearDisplay();
    display.display();
    digitalWrite(LED_BUILTIN, LOW);
  }
  else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  //leave the serial interface alone of the programming pin is HIGH
  if (!digitalRead(PROGRAMMING_PIN)) {
    Serial.begin(BAUD_RATE);
    Serial.flush();
    serialOpen = true;
  }
  else {
    serialOpen = false;
  }

  // First initialize the LocoNet interface, specifying the TX Pin
  LocoNet.init(TX_PIN);

  // Initialize a LocoNet packet buffer to buffer bytes from the PC
  initLnBuf(&lnTXBuffer);

  pinMode(RX_LED_PIN, OUTPUT);
  digitalWrite(RX_LED_PIN, HIGH);
  pinMode(TX_LED_PIN, OUTPUT);
  digitalWrite(TX_LED_PIN, HIGH);
  displayStrings[displayLineIndex] = "LocoBridge";
  if (!serialOpen) {
    incrementDisplayLine();
    displayStrings[displayLineIndex] = "Serial closed";
  }
}

//locobuffer.com
void loop() {
  // put your main code here, to run repeatedly:
  if (serialOpen && Serial) {
    digitalWrite(RX_LED_PIN, LOW);
    digitalWrite(TX_LED_PIN, LOW);
  }
  else {
    digitalWrite(RX_LED_PIN, HIGH);
    digitalWrite(TX_LED_PIN, HIGH);
  }

  if (digitalRead(PROGRAMMING_PIN)) {
    if (serialOpen) {
      Serial.end();
      serialOpen = false;
      incrementDisplayLine();
      displayStrings[displayLineIndex] = "Serial closed";
    }
  }
  else if (!serialOpen) {
    Serial.begin(BAUD_RATE);
    Serial.flush();
    serialOpen = true;
    incrementDisplayLine();
    displayStrings[displayLineIndex] = "Serial opened";
  }

  processSerialData();
  processLocoData();

  /*
    Updating the dispay over i2c takes about 37ms.
    Updating the display buffer to show the messages takes about the same time, so the process is split
    in two phases 125ms apart so the display will update every 250ms.
  */
  if ( displayAvailable && (millis() > (lastDisplayUpdateMs + 100))) {
    switch (displayState) {
      case UpdateDisplayBuffer:
        display.clearDisplay();

        display.setTextSize(TEXT_SIZE);
        display.setTextColor(SSD1306_WHITE); // Draw white text
        display.setCursor(0, 0);     // Start at top-left corner
        display.cp437(true);         // Use full 256 char 'Code Page 437' font

        for (int i = 0; i <= displayLineIndex; i++) {
          display.println(displayStrings[i]);
        }
        displayState = SendDisplayBuffer;
        break;
      case SendDisplayBuffer:
        display.display();
        displayState = UpdateDisplayBuffer;
        break;
    } //switch displayState
    lastDisplayUpdateMs = millis();
  }

}

void processSerialData(void) {
  if (serialOpen && Serial.available()) {
    //read the incoming byte:

    digitalWrite(RX_LED_PIN, HIGH); //indicate that we received data from the PC
    lastSerialReceiveMs = millis();
    serialBytes[serialByteIndex] = Serial.read();
    serialByteIndex++;

    byte messageLength = getMessageLength(serialBytes, serialByteIndex);
    if (messageLength &&
        serialByteIndex >= messageLength) { //message complete
      switch (serialBytes[0]) {
        //blocked messages
        case OPC_SW_REQ: //Switch Request
          if ( testCheckSum(serialBytes, serialByteIndex) ) {
            updateDisplayStrings(SerialBlock, serialBytes, serialByteIndex);
          }
          else {
            updateDisplayStrings(SerialError, serialBytes, serialByteIndex);
          }

          break;
        default: //pass on everything we did not want to block
          copyMessageToTXBuffer(serialBytes, serialByteIndex);
      } //switch opcode

      serialByteIndex = 0; //reset serial buffer
    }//message complete
    else if (serialByteIndex >= SERIAL_BYTES_LIMIT) { //this should never happen
      updateDisplayStrings(SerialError, serialBytes, serialByteIndex);
      serialByteIndex = 0; //reset serial buffer
    }
  } //serial byte available
  else if ((millis() > lastSerialReceiveMs + 500)
           && serialByteIndex) { //clear buffer if we get incomplete stuff to reset communication
    updateDisplayStrings(SerialError, serialBytes, serialByteIndex);
    serialByteIndex = 0;
  }

  //send outgoing LN packet
  lnPacket = recvLnMsg(&lnTXBuffer);
  if (lnPacket) {
    // Send the received packet from the PC to the LocoNet
    LocoNet.send( lnPacket );
  }

}

void processLocoData(void) {
  // Check for any received LocoNet packets
  lnPacket = LocoNet.receive() ;
  if (lnPacket) {
    // Get the length of the received packet
    byte Length = getLnMsgSize( lnPacket ) ;
    bool passMessage = true;

    if (testCheckSum(lnPacket->data, Length)) {
      switch (lnPacket->data[0]) {
        //blocked messages from the LN side
        case OPC_RQ_SL_DATA: //Slot data request
        case OPC_LOCO_ADR: //Loco address request
        case OPC_LOCO_SPD: //Loco speed
        case OPC_LOCO_DIRF: //Loco Direction and F0-F4
        case OPC_LOCO_SND: //Loco sound functions
          passMessage = false;
          break;
        default:
          break;
      }

      if (passMessage) {
        // Send the received packet out byte by byte to the PC
        if (serialOpen) {
          digitalWrite(TX_LED_PIN, HIGH); //indicate that we are sending data to the PC
          for ( int i = 0; i < Length; i++ ) {
            Serial.write(lnPacket->data[i]);
          }
        }
        updateDisplayStrings(LocoPass, lnPacket->data, Length);
      }
      else {
        updateDisplayStrings(LocoBlock, lnPacket->data, Length);
      }
    }
    else {
      updateDisplayStrings(LocoError, lnPacket->data, Length);
    }
  }
}


byte getMessageLength(const byte bytes[], byte byteCount) {
  byte messageLength = 0;
  if ( byteCount ) {
    switch (bytes[0] & 0xE0) {
      case 0xE0: //N byte message
        if (byteCount >= 2 ) { //length byte available
          messageLength = bytes[1];
        }
        break;
      case 0xC0: //6 byte message
        messageLength = 6;
        break;
      case 0xA0: //4 byte message
        messageLength = 4;
        break;
      case 0x80: //2 byte message
        messageLength = 2;
        break;
    }
  }
  return messageLength;
}

void copyMessageToTXBuffer(const byte bytes[], byte byteCount) {
  if ( testCheckSum(bytes, byteCount) ) {
    updateDisplayStrings(SerialPass, bytes, byteCount);
    for (int i = 0; i < serialByteIndex; i++ ) {
      addByteLnBuf(&lnTXBuffer, bytes[i] & 0xFF);
    }
  }
  else {
    updateDisplayStrings(SerialError, bytes, byteCount);
  }
}

bool testCheckSum(const byte bytes[], byte byteCount) {
  byte check = 0;

  for (int i = 0; i < byteCount; i++) {
    check ^= bytes[i];
  }

  return check == 0xFF;
}

void updateDisplayStrings(MessageStates messageState, const byte bytes[], byte byteCount) {
  if ((messageState == SerialBlock || messageState == LocoBlock)
      && HIDE_BLOCKED_MESSAGES) {
    return;
  }

  incrementDisplayLine();

  switch (messageState) {
    case SerialPass:
      displayStrings[displayLineIndex]  = " > ";
      break;
    case SerialBlock:
      displayStrings[displayLineIndex]  = " >|";
      break;
    case SerialError:
      displayStrings[displayLineIndex]  = " >X";
      break;
    case LocoPass:
      displayStrings[displayLineIndex]  = " < ";
      break;
    case LocoBlock:
      displayStrings[displayLineIndex]  = "|< ";
      break;
    case LocoError:
      displayStrings[displayLineIndex]  = "X< ";
      break;
    default:
      displayStrings[displayLineIndex]  = "";
  }

  displayStrings[displayLineIndex] += ':';

  for ( int i = 0; i < byteCount; i++) {
    if ( i && !(i % 8) ) { //go to next line after 8 bytes
      incrementDisplayLine();
      displayStrings[displayLineIndex]  = "    ";
    }

    String byteString = String(bytes[i], HEX);
    byteString.toUpperCase();
    if (byteString.length() < 2) {
      byteString = "0" + byteString;
    }
    displayStrings[displayLineIndex] += byteString;
  }
}

void incrementDisplayLine() {
  displayLineIndex++;
  if (displayLineIndex >= DISPLAY_LINES) {
    if (displayStrings[DISPLAY_LINES].length()) {
      for (int i = 1; i < DISPLAY_LINES; i++) {
        displayStrings[i - 1] = displayStrings[i];
      }
    }
    displayLineIndex = DISPLAY_LINES - 1;
  }
}
