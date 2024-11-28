#include <WiFi.h>
#include <esp_now.h>
#include <ESPAsyncWebServer.h>
#include <mbedtls/aes.h>  // Import AES library from mbedtls (pre-installed in ESP32)

AsyncWebServer server(80);

// AES key (128-bit key for AES encryption)
uint8_t aesKey[16] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                       0xab, 0xf7, 0x97, 0x75, 0x46, 0x8f, 0x2e, 0x64 };

uint8_t iv[16] = { 0 }; // Initialization Vector (IV) for AES

uint8_t encryptedData[32];
uint8_t decryptedData[32];

// WiFi AP credentials
const char *ssid = ""; // Your AP SSID
const char *password = "";   // Password for your WiFi

uint8_t peerMAC[] = { }; // Change to your peer's MAC address

void setup() {
  Serial.begin(115200);

  // Set ESP32 as an Access Point (AP) with hidden SSID
  WiFi.softAP(ssid, password, 6, false, 1); // Channel 6, SSID hidden (false)
  
  Serial.println("ESP32 is now in AP mode.");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Add peer for ESP-NOW communication
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, peerMAC, sizeof(peerMAC));
  peerInfo.channel = 0;
  peerInfo.encrypt = false;  // No encryption for communication (we handle encryption manually)
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Set up ESP-NOW receive callback with updated signature
  esp_now_register_recv_cb(onReceive);

  // Start web server to send encrypted data
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String message = "Enter your message to send: <form action='/sendData' method='get'><input type='text' name='message'><input type='submit' value='Send Message'></form>";
    request->send(200, "text/html", message);
  });

  server.on("/sendData", HTTP_GET, [](AsyncWebServerRequest *request){
    String message = request->getParam("message")->value();
    sendEncryptedData(message.c_str());  // Send the message as encrypted data
    request->send(200, "text/plain", "Encrypted message sent!");
  });

  server.begin();
}

void loop() {
  // Nothing to do here, asynchronous web server and ESP-NOW callback handle everything
  delay(10); // Prevent blocking
}

// Updated callback function with the correct signature
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  Serial.print("Data received (encrypted): ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)data[i]);
  }
  Serial.println();

  // Decrypt the data received via ESP-NOW
  aesDecrypt(data, decryptedData);

  // Print decrypted data
  Serial.print("Decrypted Data: ");
  Serial.println((char*)decryptedData);
}

// Function to send encrypted data via ESP-NOW
void sendEncryptedData(const char* plaintext) {
  size_t len = strlen(plaintext) + 1;  // +1 for null terminator

  // Encrypt the data
  aesEncrypt((uint8_t*)plaintext, encryptedData);

  // Send encrypted data
  esp_now_send(peerMAC, encryptedData, len);
  Serial.println("Encrypted Data Sent:");
  for (int i = 0; i < len; i++) {
    Serial.print((char)encryptedData[i]);
  }
  Serial.println();
}

// AES encryption using AES-ECB mode (no IV required)
void aesEncrypt(uint8_t *input, uint8_t *output) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  // Set the AES encryption key
  mbedtls_aes_setkey_enc(&aes, aesKey, 128); // 128-bit key

  // Encrypt the input data
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output);
  
  mbedtls_aes_free(&aes);
}

// AES decryption using AES-ECB mode (no IV required)
void aesDecrypt(const uint8_t *input, uint8_t *output) {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  // Set the AES decryption key
  mbedtls_aes_setkey_dec(&aes, aesKey, 128); // 128-bit key

  // Decrypt the input data
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input, output);

  mbedtls_aes_free(&aes);
}
