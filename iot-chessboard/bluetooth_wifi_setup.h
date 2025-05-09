/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleWrite.cpp
    Ported to Arduino ESP32 by Evandro Copercini
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "wifi_setup.h"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

bool isWifiConnected = false;

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
      String ssid = value.substring(0, index);
      String password = value.substring(index+1);
      
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

    while(!isWifiConnected){//loop until we are connected
      delay(2); 
    }
}