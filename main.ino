#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

// Wi-Fi credentials
const char* ssid = "";
const char* password = "";

// RS232 
#define RX_PIN D9 // GPIO13
#define TX_PIN D10 // GPIO15

SoftwareSerial rs232Serial(RX_PIN, TX_PIN); // RX, TX

String serialData = "";
WiFiServer server(80);

// baud rates to test
const long baudRates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600 };
const int numBaudRates = sizeof(baudRates) / sizeof(baudRates[0]);

// variables to store testing results
long workingBaudRates[numBaudRates];
int workingCount = 0;

// threshold for considering data as legible (e.g., 70% printable)
const float PRINTABLE_THRESHOLD = 0.7;

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow time for Serial to initialize
  Serial.println("Starting RS232 Baud Rate Tester...");

  rs232Serial.begin(9600);
  delay(1000); 

  // test all baud rates
  testBaudRates();

  // after testing, connect to Wi-Fi
  connectToWiFi();
}

void loop() {
  // read incoming data from RS232
  while (rs232Serial.available()) {
    char incomingByte = rs232Serial.read();
    serialData += incomingByte;
    Serial.print(incomingByte); // Debug: print incoming data
  }

  // handle incoming client connections
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");

    // wait until the client sends some data
    while (client.connected() && !client.available()) {
      delay(1);
    }

    // read the HTTP request
    String request = "";
    while (client.available()) {
      request += char(client.read());
    }

    Serial.println("Request:");
    Serial.println(request);

    // prepare the response
    String response;
    if (request.indexOf("GET /data") != -1) {
      response = serialData;  // Send the data received from RS232
    } else if (request.indexOf("GET /baudrates") != -1) {
      // Send the list of working baud rates
      response = "Working Baud Rates:\n";
      for(int i = 0; i < workingCount; i++) {
        response += String(workingBaudRates[i]) + "\n";
      }
    } else {
      response = "Unknown request. Use /data to get RS232 data or /baudrates to get working baud rates.";
    }

    // send HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(response);

    // todo clear the serialData after sending if desired
    // serialData = "";

    // close the connection
    client.stop();
    Serial.println("Client disconnected");
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  // wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  server.begin();
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void testBaudRates() {
  Serial.println("Starting baud rate tests...");
  
  for(int i = 0; i < numBaudRates; i++) {
    long currentBaud = baudRates[i];
    Serial.print("Testing baud rate: ");
    Serial.println(currentBaud);
    
    // initialize SoftwareSerial with the current baud rate
    rs232Serial.begin(currentBaud);
    delay(1000); // Wait for RS232 to stabilize
    
    // clear any existing data
    while (rs232Serial.available()) {
      rs232Serial.read();
    }
    
    // read data for a specific duration
    unsigned long startTime = millis();
    unsigned long testDuration = 2000; // 2 seconds
    String testData = "";
    
    while (millis() - startTime < testDuration) {
      if (rs232Serial.available()) {
        char c = rs232Serial.read();
        testData += c;
      }
    }
    
    // evaluate the readability of the data
    int printableCount = 0;
    int totalCount = testData.length();
    
    for(int j = 0; j < totalCount; j++) {
      if(isPrintable(testData[j])) {
        printableCount++;
      }
    }
    
    float printableRatio = (totalCount > 0) ? ((float)printableCount / totalCount) : 0;
    
    Serial.print("Printable characters: ");
    Serial.print(printableCount);
    Serial.print("/");
    Serial.print(totalCount);
    Serial.print(" (");
    Serial.print(printableRatio * 100);
    Serial.println("%)");
    
    if(printableRatio >= PRINTABLE_THRESHOLD) {
      Serial.print("Baud rate ");
      Serial.print(currentBaud);
      Serial.println(" is likely correct.");
      workingBaudRates[workingCount++] = currentBaud;
    } else {
      Serial.print("Baud rate ");
      Serial.print(currentBaud);
      Serial.println(" is not suitable.");
    }
    
    // delay before next test
    delay(500);
  }
  
  if(workingCount > 0) {
    Serial.println("Baud rate testing completed.");
    Serial.print("Number of working baud rates: ");
    Serial.println(workingCount);
    
    // todo working - optionally, set the first working baud rate for normal operation
    rs232Serial.begin(workingBaudRates[0]);
    Serial.print("Set SoftwareSerial to baud rate: ");
    Serial.println(workingBaudRates[0]);
  } else {
    Serial.println("No suitable baud rate found.");
  }
}

// helper function to check if a character is printable
bool isPrintable(char c) {
  return (c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t';
}
