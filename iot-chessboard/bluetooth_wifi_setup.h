#ifndef bluetooth_wifi_setup
#define bluetooth_wifi_setup

  /*
      Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleWrite.cpp
      Ported to Arduino ESP32 by Evandro Copercini
  */

  #include <BLEDevice.h>
  #include <BLEUtils.h>
  #include <BLEServer.h>
  #include "wifi_setup.h"

  #include "oled_functions.h"
  #include "global_preferences.h"

  // See the following for generating UUIDs:
  // https://www.uuidgenerator.net/
  #define SERVICE_UUID        "d9a2ce94-f87b-4bac-9f17-7fa79d88e5c6"
  #define CHARACTERISTIC_UUID "3cb62c6b-f175-448c-ace6-7b6d30849d9b"

  bool isWifiConnected = false;
  String ssid = "";
  String password = "";

  class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();

      //print recieved value
      Serial.print("Value Recieved Over Bluetooth: ");
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }
      Serial.println();

      //separate into parts for wifi setup
      int index = value.indexOf(':');
      //ensure colon is found if not continue to wait for network credentials
      if(index != -1){
        ssid = value.substring(0, index);
        password = value.substring(index+1);
        
        Serial.println("SSID: " + ssid);
        Serial.println("Password: " + password);

        //should return true if successful (I haven't setup timeout so invalid data will just stall here)
        isWifiConnected = connectWifi(ssid, password);
      }
      
      //after connected to wifi stop BLE communication if connection unsuccessful send message to indicate that over bluetooth and wait for data again
      if(isWifiConnected){
        BLEDevice::getAdvertising()->stop();
        BLEDevice::deinit(true); // 'true' clears the BLE device resources
      } else {
        pCharacteristic->setValue("Connection Unsuccessful\nSend Your Wifi Network Information in the following format <network_name>:<network_password>");
      }
    }
  };

  extern void runBluetoothNetworkSetup(){
      BLEDevice::init("IoT_ChessBoard");
      BLEServer *pServer = BLEDevice::createServer();

      BLEService *pService = pServer->createService(SERVICE_UUID);

      BLECharacteristic *pCharacteristic =
        pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

      pCharacteristic->setCallbacks(new MyCallbacks());

      pCharacteristic->setValue("Send Your Wifi Network Information in the following format <network_name>:<network_password>");
      pService->start();

      BLEAdvertising *pAdvertising = pServer->getAdvertising();
      pAdvertising->start();

      displayString(0, 0, "Connect to BLE Device:");
      displayString(0, 10, "IoT_ChessBoard");
      displayString(0, 20, "Send Your Wifi Network Info");
      displayString(0, 30, "in Format:");
      displayString(0, 40, "<newtork_name>:<network_password>");

      while(!isWifiConnected){//loop until we are connected
        delay(2);
      }

      prefs.putString("network_ssid", ssid);
      prefs.putString("network_pw", password);
      Serial.println("After prefs.putting ssid is: " + prefs.getString("network_ssid"));
      Serial.println("After prefs.putting password is: " + prefs.getString("network_password"));

      display.clear();
      VextOFF();
  }

#endif