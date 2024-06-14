#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPSRedirect.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* GScriptId = "<Enter Google Script Deployment ID>";
String gate_number = "Gate1";
const char* ssid = "<WIFI_SSID>";
const char* password = "<WIFI_Pass>";
String payload_base = "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet1\", \"values\": ";
String payload = "";
const char* host = "script.google.com";
const int httpsPort = 443;
const char* fingerprint = "";
String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;
String student_id;
int blocks[] = { 4, 5, 6, 8, 9 };
#define total_blocks (sizeof(blocks) / sizeof(blocks[0]))
#define RST_PIN 0  //D3
#define SS_PIN 2   //D4

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;

int blockNum = 2;
byte bufferLen = 18;
byte readBlockData[18];
#define GREEN_LED 1 //D1
#define RED_LED 2 //D2
void setup() {
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  Serial.begin(9600);
  delay(10);
  Serial.println('\n');
  SPI.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);  
  lcd.print("WiFi...");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    Serial.println("TRYING TO CONNECT");
  }
  Serial.println('\n');
  Serial.println("WiFi Connected!");

  Serial.println(WiFi.localIP());
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  lcd.clear();
  lcd.setCursor(0, 0);  //col=0 row=0
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);  //col=0 row=0
  lcd.print("Google ");
  delay(5000);
  Serial.print("Connecting to ");
  Serial.println(host);
  bool flag = false;
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      String msg = "Connected. OK";
      Serial.println(msg);
      lcd.clear();
      lcd.setCursor(0, 0);  //col=0 row=0
      lcd.print(msg);
      delay(2000);
      break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag) {
    lcd.clear();
    lcd.setCursor(0, 0);  //col=0 row=0
    lcd.print("Connection fail");
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    delay(5000);
    return;
  }
  delete client;     
  client = nullptr;  
}
void loop() {
  static bool flag = false;
  if (!flag) {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr) {
    if (!client->connected()) {
      int retval = client->connect(host, httpsPort);
      if (retval != 1) {
        Serial.println("Disconnected. Retrying...");
        lcd.clear();
        lcd.setCursor(0, 0);  //col=0 row=0
        lcd.print("Disconnected.");
        lcd.setCursor(0, 1);  //col=0 row=1
        lcd.print("Retrying...");
        return;   
      }
    }
  } else {
    Serial.println("Error creating client object!");
    Serial.println("else");
  }
  lcd.clear();
  lcd.setCursor(0, 0);  //col=0 row=0
  lcd.print("Scan your Tag");
  mfrc522.PCD_Init();
  if (!mfrc522.PICC_IsNewCardPresent()) { return; }
  if (!mfrc522.PICC_ReadCardSerial()) { return; }
  Serial.println();
  
  Serial.println(F("Reading last data from RFID..."));
  String values = "", data;
  for (byte i = 0; i < total_blocks; i++) {
    ReadDataFromBlock(blocks[i], readBlockData);
    if (i == 0) {
      data = String((char*)readBlockData);
      data.trim();
      student_id = data;
      values = "\"" + data + ",";
    }
    else {
      data = String((char*)readBlockData);
      data.trim();
      values += data + ",";
    }
  }
  values += gate_number + "\"}";
  payload = payload_base + values;
  lcd.clear();
  lcd.setCursor(0, 0);  //col=0 row=0
  lcd.print("Publishing Data");
  lcd.setCursor(0, 1);  //col=0 row=0
  lcd.print("Please Wait...");
  Serial.println("Publishing data...");
  Serial.println(payload);
  
  if (client->POST(url, host, payload)) {
    Serial.println("[OK] Data published.");
    lcd.clear();
    lcd.setCursor(0, 0);  //col=0 row=0
    lcd.print("Student ID: " + student_id);
    lcd.setCursor(0, 1);  //col=0 row=0
    lcd.print("Thanks");
  }
  else {
    Serial.println("Error while connecting");
    lcd.clear();
    lcd.setCursor(0, 0);  //col=0 row=0
    lcd.print("Failed.");
    lcd.setCursor(0, 1);  //col=0 row=0
    lcd.print("Try Again");
  }
  Serial.println("[TEST] delay(5000)");
  delay(3000);
  
  if(client->POST(url, host, payload)){
    digitalWrite(GREEN_LED, HIGH);
    delay(4000);
    digitalWrite(GREEN_LED, LOW);

  } else {
    digitalWrite(RED_LED, HIGH);
    delay(4000);
    digitalWrite(RED_LED, LOW);

  }
    
}

void ReadDataFromBlock(int blockNum, byte readBlockData[]) {
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockNum, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Authentication failed for Read: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else {

    Serial.println("Authentication success");
  }
  status = mfrc522.MIFARE_Read(blockNum, readBlockData, &bufferLen);
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Reading failed: ");
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }
  else {
    readBlockData[16] = ' ';
    readBlockData[17] = ' ';
    Serial.println("Block was read successfully");
  }
}
