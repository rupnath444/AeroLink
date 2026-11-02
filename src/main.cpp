#include <Arduino.h>

// =====================================================
// LORA UART
// =====================================================

HardwareSerial LoRaSerial(2);

// =====================================================
// DEVICE CONFIGURATION
// =====================================================

#ifndef DEVICE_ADDRESS
#define DEVICE_ADDRESS 2
#endif

#ifndef DESTINATION_ADDRESS
#define DESTINATION_ADDRESS 1
#endif

#ifndef NETWORK_ID
#define NETWORK_ID 10
#endif

const int myAddress = DEVICE_ADDRESS;
const int destinationAddress = DESTINATION_ADDRESS;

// =====================================================
// RYLR998 UART
// =====================================================

const int LORA_RX = 16;
const int LORA_TX = 17;

// =====================================================
// LED PINS
// =====================================================

const int RED_LED = 32;
const int GREEN_LED = 33;
const int YELLOW_LED = 25;

// =====================================================
// BUTTON PINS
// =====================================================

const int BUTTON1 = 18;
const int BUTTON2 = 19;
const int BUTTON3 = 21;
const int BUTTON4 = 22;

// =====================================================
// ACK / RETRY SETTINGS
// =====================================================

const int MAX_RETRIES = 3;
const unsigned long ACK_TIMEOUT = 3000;

// =====================================================
// LED TIMING
// =====================================================

const unsigned long LED_FLASH_TIME = 500;

// =====================================================
// BUTTON DEBOUNCE
// =====================================================

const unsigned long DEBOUNCE_TIME = 50;

// =====================================================
// MESSAGE QUEUE
// =====================================================

const int QUEUE_SIZE = 8;

struct QueuedMessage {
  int id;
  String message;
};

QueuedMessage messageQueue[QUEUE_SIZE];

int queueHead = 0;
int queueTail = 0;
int queueCount = 0;

// =====================================================
// MESSAGE ID
// =====================================================

int nextMessageID = 0;

// =====================================================
// ACTIVE MESSAGE
// =====================================================

bool waitingForAck = false;

int activeMessageID = -1;

String activeMessage = "";

int retryCount = 0;

unsigned long sendTime = 0;

// =====================================================
// DUPLICATE PROTECTION
// =====================================================

int lastReceivedSender = -1;
int lastReceivedMessageID = -1;

// =====================================================
// SERIAL VARIABLES
// =====================================================

String userMessage = "";
String loraResponse = "";

// =====================================================
// LED TIMER VARIABLES
// =====================================================

unsigned long greenLedStart = 0;
unsigned long yellowLedStart = 0;

bool greenLedActive = false;
bool yellowLedActive = false;

// =====================================================
// BUTTON STATE VARIABLES
// =====================================================


bool lastButton1State = HIGH;
bool lastButton2State = HIGH;
bool lastButton3State = HIGH;
bool lastButton4State = HIGH;

unsigned long lastButton1Time = 0;
unsigned long lastButton2Time = 0;
unsigned long lastButton3Time = 0;
unsigned long lastButton4Time = 0;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================

void checkButtons();

bool queueMessage(String message);

void startNextMessage();

void sendActiveMessage();

void removeCurrentMessage();

void processLoRaResponse(String response);

void parseReceivedMessage(String response);

void sendACK(String destination, String id);

void configureLoRaModule();

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // -------------------------------------------------
  // LEDs
  // -------------------------------------------------

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);

  // -------------------------------------------------
  // Buttons
  // -------------------------------------------------

  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);
  pinMode(BUTTON4, INPUT_PULLUP);

  // -------------------------------------------------
  // LoRa UART
  // -------------------------------------------------

  LoRaSerial.begin(
    115200,
    SERIAL_8N1,
    LORA_RX,
    LORA_TX
  );

  delay(1000);
  configureLoRaModule();

  // -------------------------------------------------
  // Startup information
  // -------------------------------------------------

  Serial.println();
  Serial.println("================================");
  Serial.println("   LoRa Messaging System");
  Serial.println("       QUEUE VERSION");
  Serial.println("================================");

  Serial.print("My Address: ");
  Serial.println(myAddress);

  Serial.print("Destination: ");
  Serial.println(destinationAddress);

  Serial.println();

  Serial.println("LoRa UART:");
  Serial.println("RX = GPIO16");
  Serial.println("TX = GPIO17");

  Serial.println();

  Serial.println("LEDs:");
  Serial.println("RED    = GPIO32");
  Serial.println("GREEN  = GPIO33");
  Serial.println("YELLOW = GPIO25");

  Serial.println();

  Serial.println("Buttons:");
  Serial.println("BUTTON 1 = GPIO18");
  Serial.println("BUTTON 2 = GPIO19");
  Serial.println("BUTTON 3 = GPIO21");
  Serial.println("BUTTON 4 = GPIO22");

  Serial.println();

  Serial.println("Queue:");
  Serial.print("Capacity = ");
  Serial.println(QUEUE_SIZE);

  Serial.println();

  Serial.println("ACK: ENABLED");
  Serial.println("Retry: ENABLED");
  Serial.println("Duplicate Protection: ENABLED");

  Serial.println();

  Serial.println("Ready.");
  Serial.println();
}

// =====================================================
// LORA MODULE CONFIGURATION
// =====================================================

void configureLoRaModule() {

  LoRaSerial.print("AT+ADDRESS=");
  LoRaSerial.print(myAddress);
  LoRaSerial.print("\r\n");
  delay(200);

  LoRaSerial.print("AT+NETWORKID=");
  LoRaSerial.print(NETWORK_ID);
  LoRaSerial.print("\r\n");
  delay(200);
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // -------------------------------------------------
  // Check physical buttons
  // -------------------------------------------------

  checkButtons();

  // -------------------------------------------------
  // Check Serial Monitor input
  // -------------------------------------------------

  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') {

      if (userMessage.length() > 0) {

        Serial.println();
        queueMessage(userMessage);

        userMessage = "";
      }

    } else if (c == '\b' || c == 127) {

      if (userMessage.length() > 0) {

        userMessage.remove(userMessage.length() - 1);
        Serial.print("\b \b");
      }

    } else {

      userMessage += c;
      Serial.write(c);
    }
  }

  // -------------------------------------------------
  // Check RYLR998
  // -------------------------------------------------

  while (LoRaSerial.available()) {

    char c = LoRaSerial.read();

    loraResponse += c;

    if (c == '\n') {

      processLoRaResponse(loraResponse);

      loraResponse = "";
    }
  }

  // -------------------------------------------------
  // ACK timeout
  // -------------------------------------------------

  if (waitingForAck) {

    if (millis() - sendTime >= ACK_TIMEOUT) {

      if (retryCount < MAX_RETRIES) {

        retryCount++;

        Serial.println();
        Serial.print("ACK timeout. Retry ");
        Serial.print(retryCount);
        Serial.print("/");
        Serial.println(MAX_RETRIES);

        sendActiveMessage();

      } else {

        Serial.println();
        Serial.println("================================");
        Serial.println("       DELIVERY FAILED");
        Serial.println("================================");

        Serial.print("Message ID: ");
        Serial.println(activeMessageID);

        Serial.print("Message: ");
        Serial.println(activeMessage);

        Serial.println("================================");
        Serial.println();

        digitalWrite(RED_LED, LOW);

        waitingForAck = false;

        removeCurrentMessage();

        startNextMessage();
      }
    }
  }

  // -------------------------------------------------
  // Green LED timer
  // -------------------------------------------------

  if (greenLedActive) {

    if (millis() - greenLedStart >= LED_FLASH_TIME) {

      digitalWrite(GREEN_LED, LOW);

      greenLedActive = false;
    }
  }

  // -------------------------------------------------
  // Yellow LED timer
  // -------------------------------------------------

  if (yellowLedActive) {

    if (millis() - yellowLedStart >= LED_FLASH_TIME) {

      digitalWrite(YELLOW_LED, LOW);

      yellowLedActive = false;
    }
  }
}


// =====================================================
// MESSAGE QUEUE
// =====================================================

bool queueMessage(String message) {

  // -------------------------------------------------
  // Check if queue is full
  // -------------------------------------------------

  if (queueCount >= QUEUE_SIZE) {

    Serial.println();
    Serial.println("================================");
    Serial.println("         QUEUE FULL");
    Serial.println("================================");

    Serial.println(
      "Message was NOT added."
    );

    Serial.println("================================");
    Serial.println();

    return false;
  }

  // -------------------------------------------------
  // Generate message ID
  // -------------------------------------------------

  nextMessageID++;

  if (nextMessageID > 9999) {

    nextMessageID = 1;
  }

  // -------------------------------------------------
  // Add message
  // -------------------------------------------------

  messageQueue[queueTail].id =
    nextMessageID;

  messageQueue[queueTail].message =
    message;

  queueTail++;

  if (queueTail >= QUEUE_SIZE) {

    queueTail = 0;
  }

  queueCount++;

  // -------------------------------------------------
  // Display queue information
  // -------------------------------------------------

  Serial.println();
  Serial.println("================================");
  Serial.println("        MESSAGE QUEUED");
  Serial.println("================================");

  Serial.print("Message ID: ");
  Serial.println(nextMessageID);

  Serial.print("Message: ");
  Serial.println(message);

  Serial.print("Queue Size: ");
  Serial.print(queueCount);
  Serial.print("/");
  Serial.println(QUEUE_SIZE);

  Serial.println("================================");
  Serial.println();

  // -------------------------------------------------
  // If nothing is currently being transmitted,
  // start this message immediately.
  // -------------------------------------------------

  if (!waitingForAck) {

    startNextMessage();
  }

  return true;
}


// =====================================================
// START NEXT MESSAGE
// =====================================================

void startNextMessage() {

  // -------------------------------------------------
  // Nothing in queue
  // -------------------------------------------------

  if (queueCount == 0) {

    waitingForAck = false;

    digitalWrite(RED_LED, LOW);

    return;
  }

  // -------------------------------------------------
  // Already transmitting
  // -------------------------------------------------

  if (waitingForAck) {

    return;
  }

  // -------------------------------------------------
  // Load queue head
  // -------------------------------------------------

  activeMessageID =
    messageQueue[queueHead].id;

  activeMessage =
    messageQueue[queueHead].message;

  retryCount = 0;

  waitingForAck = true;

  // -------------------------------------------------
  // RED = transmission active
  // -------------------------------------------------

  digitalWrite(RED_LED, HIGH);

  Serial.println();
  Serial.println("================================");
  Serial.println("        SENDING MESSAGE");
  Serial.println("================================");

  Serial.print("Message ID: ");
  Serial.println(activeMessageID);

  Serial.print("To: ");
  Serial.println(destinationAddress);

  Serial.print("Message: ");
  Serial.println(activeMessage);

  Serial.print("Queue Remaining: ");
  Serial.println(queueCount);

  Serial.println("================================");
  Serial.println();

  sendActiveMessage();
}


// =====================================================
// SEND ACTIVE MESSAGE
// =====================================================

void sendActiveMessage() {

  String payload =
    "MSG|" +
    String(activeMessageID) +
    "|" +
    activeMessage;

  int payloadLength =
    payload.length();

  String command =
    "AT+SEND=" +
    String(destinationAddress) +
    "," +
    String(payloadLength) +
    "," +
    payload;

  Serial.println();

  Serial.print("Sending: ");
  Serial.println(command);

  LoRaSerial.print(command);
  LoRaSerial.print("\r\n");

  sendTime = millis();
}


// =====================================================
// REMOVE CURRENT MESSAGE
// =====================================================

void removeCurrentMessage() {

  if (queueCount == 0) {

    return;
  }

  // -------------------------------------------------
  // Move queue head forward
  // -------------------------------------------------

  queueHead++;

  if (queueHead >= QUEUE_SIZE) {

    queueHead = 0;
  }

  queueCount--;

  // -------------------------------------------------
  // Clear active message
  // -------------------------------------------------

  activeMessageID = -1;

  activeMessage = "";

  retryCount = 0;
}


// =====================================================
// PROCESS RYLR RESPONSE
// =====================================================

void processLoRaResponse(String response) {

  response.trim();

  if (response.length() == 0) {

    return;
  }

  Serial.print("RYLR: ");
  Serial.println(response);

  if (response.startsWith("+RCV=")) {

    parseReceivedMessage(response);
  }
}


// =====================================================
// PARSE RECEIVED MESSAGE
// =====================================================

void parseReceivedMessage(String response) {

  String data =
    response.substring(5);

  int comma1 =
    data.indexOf(',');

  if (comma1 == -1) return;

  int comma2 =
    data.indexOf(',', comma1 + 1);

  if (comma2 == -1) return;

  int comma3 =
    data.indexOf(',', comma2 + 1);

  if (comma3 == -1) return;

  int comma4 =
    data.indexOf(',', comma3 + 1);

  if (comma4 == -1) return;

  String sender =
    data.substring(0, comma1);

  String length =
    data.substring(
      comma1 + 1,
      comma2
    );

  String message =
    data.substring(
      comma2 + 1,
      comma3
    );

  String rssi =
    data.substring(
      comma3 + 1,
      comma4
    );

  String snr =
    data.substring(
      comma4 + 1
    );

  // =================================================
  // ACK RECEIVED
  // =================================================

  if (message.startsWith("ACK|")) {

    int receivedID =
      message.substring(4).toInt();

    // ------------------------------------------------
    // Make sure ACK belongs to current message
    // ------------------------------------------------

    if (
      waitingForAck &&
      receivedID == activeMessageID
    ) {

      Serial.println();
      Serial.println("================================");
      Serial.println("          ✓ DELIVERED");
      Serial.println("================================");

      Serial.print("Message ID: ");
      Serial.println(receivedID);

      Serial.print("ACK from: ");
      Serial.println(sender);

      Serial.print("RSSI: ");
      Serial.print(rssi);
      Serial.println(" dBm");

      Serial.print("SNR: ");
      Serial.println(snr);

      Serial.print("Queue Before Remove: ");
      Serial.println(queueCount);

      Serial.println("================================");
      Serial.println();

      // ------------------------------------------------
      // Stop current transmission state
      // ------------------------------------------------

      waitingForAck = false;

      digitalWrite(RED_LED, LOW);

      // ------------------------------------------------
      // Yellow LED = delivered
      // ------------------------------------------------

      digitalWrite(YELLOW_LED, HIGH);

      yellowLedStart = millis();

      yellowLedActive = true;

      // ------------------------------------------------
      // Remove completed message
      // ------------------------------------------------

      removeCurrentMessage();

      // ------------------------------------------------
      // Start next queued message
      // ------------------------------------------------

      startNextMessage();
    }

    return;
  }

  // =================================================
  // NORMAL MESSAGE
  // =================================================

  if (message.startsWith("MSG|")) {

    String content =
      message.substring(4);

    int separator =
      content.indexOf('|');

    if (separator == -1) {

      Serial.println(
        "Invalid MSG packet."
      );

      return;
    }

    String receivedID =
      content.substring(
        0,
        separator
      );

    String actualMessage =
      content.substring(
        separator + 1
      );

    int receivedMessageID =
      receivedID.toInt();

    int senderAddress =
      sender.toInt();

    // =================================================
    // DUPLICATE CHECK
    // =================================================

    if (
      senderAddress == lastReceivedSender &&
      receivedMessageID == lastReceivedMessageID
    ) {

      Serial.println();
      Serial.println("================================");
      Serial.println("       DUPLICATE MESSAGE");
      Serial.println("================================");

      Serial.print("From: ");
      Serial.println(sender);

      Serial.print("Message ID: ");
      Serial.println(receivedMessageID);

      Serial.println(
        "Message will NOT be displayed again."
      );

      Serial.println(
        "Sending ACK again..."
      );

      Serial.println("================================");
      Serial.println();

      sendACK(
        sender,
        receivedID
      );

      return;
    }

    // =================================================
    // SAVE MESSAGE ID
    // =================================================

    lastReceivedSender =
      senderAddress;

    lastReceivedMessageID =
      receivedMessageID;

    // =================================================
    // DISPLAY MESSAGE
    // =================================================

    Serial.println();
    Serial.println("================================");
    Serial.println("        MESSAGE RECEIVED");
    Serial.println("================================");

    Serial.print("From: ");
    Serial.println(sender);

    Serial.print("Message ID: ");
    Serial.println(receivedID);

    Serial.print("Message: ");
    Serial.println(actualMessage);

    Serial.print("RSSI: ");
    Serial.print(rssi);
    Serial.println(" dBm");

    Serial.print("SNR: ");
    Serial.println(snr);

    Serial.println("================================");
    Serial.println();

    // =================================================
    // GREEN LED
    // =================================================

    digitalWrite(GREEN_LED, HIGH);

    greenLedStart = millis();

    greenLedActive = true;

    // =================================================
    // SEND ACK
    // =================================================

    sendACK(
      sender,
      receivedID
    );
  }
}


// =====================================================
// SEND ACK
// =====================================================

void sendACK(
  String destination,
  String id
) {

  String payload =
    "ACK|" + id;

  int length =
    payload.length();

  String command =
    "AT+SEND=" +
    destination +
    "," +
    String(length) +
    "," +
    payload;

  Serial.println();

  Serial.print("Sending ACK: ");
  Serial.println(command);

  LoRaSerial.print(command);
  LoRaSerial.print("\r\n");
}


// =====================================================
// BUTTON HANDLING
// =====================================================

void checkButtons() {

  bool button1 =
    digitalRead(BUTTON1);

  bool button2 =
    digitalRead(BUTTON2);

  bool button3 =
    digitalRead(BUTTON3);

  bool button4 =
    digitalRead(BUTTON4);


  // =================================================
  // BUTTON 1
  // =================================================

  if (
    button1 == LOW &&
    lastButton1State == HIGH &&
    millis() - lastButton1Time > DEBOUNCE_TIME
  ) {

    lastButton1Time = millis();

    Serial.println();
    Serial.println("BUTTON 1 PRESSED");

    queueMessage(
      "BUTTON 1 PRESSED"
    );
  }


  // =================================================
  // BUTTON 2
  // =================================================

  if (
    button2 == LOW &&
    lastButton2State == HIGH &&
    millis() - lastButton2Time > DEBOUNCE_TIME
  ) {

    lastButton2Time = millis();

    Serial.println();
    Serial.println("BUTTON 2 PRESSED");

    queueMessage(
      "BUTTON 2 PRESSED"
    );
  }


  // =================================================
  // BUTTON 3
  // =================================================

  if (
    button3 == LOW &&
    lastButton3State == HIGH &&
    millis() - lastButton3Time > DEBOUNCE_TIME
  ) {

    lastButton3Time = millis();

    Serial.println();
    Serial.println("BUTTON 3 PRESSED");

    queueMessage(
      "BUTTON 3 PRESSED"
    );
  }


  // =================================================
  // BUTTON 4
  // =================================================

  if (
    button4 == LOW &&
    lastButton4State == HIGH &&
    millis() - lastButton4Time > DEBOUNCE_TIME
  ) {

    lastButton4Time = millis();

    Serial.println();
    Serial.println("BUTTON 4 PRESSED");

    queueMessage(
      "BUTTON 4 PRESSED"
    );
  }


  // =================================================
  // SAVE BUTTON STATES
  // =================================================

  lastButton1State = button1;
  lastButton2State = button2;
  lastButton3State = button3;
  lastButton4State = button4;
}