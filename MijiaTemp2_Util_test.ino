//https://github.com/karolkalinski/esp32-snippets
//https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
//https://medium.com/home-wireless/how-to-program-an-esp32-in-arduino-while-using-esp-idf-functions-90033d860f75
#define BTU_TASK_STACK_SIZE (6144 + BT_TASK_EXTRA_STACK_SIZE)
#include <BLEDevice.h>
#include <Esp.h>

//#define LYWSD03MMC_ADDR "a4:c1:38:06:78:d8"
#define LYWSD03MMC_ADDR "a4:c1:38:89:78:1c"

BLEClient* pClient;
static BLEAddress htSensorAddress(LYWSD03MMC_ADDR);

bool connectionSuccessful = false;

// The remote service we wish to connect to.
static BLEUUID serviceUUID("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");
// La caracteristica para cambiar el tiempo de intervalo de comunicaciones.
static BLEUUID charUUID_SetCommInterval("ebe0ccd8-7a0a-4b0c-8a1a-6ff2997da3a6");
static BLEUUID charUUID_SetNotify("00002902-0000-1000-8000-00805f9b34fb");

// Obtain a reference to the service we are after in the remote BLE server.
BLERemoteService* pRemoteService;
// Obtain a reference to the characteristic in the service of the remote BLE server.
BLERemoteCharacteristic* pRemoteCharacteristic_THB;
  
const uint8_t notificationOff[] = {0x0, 0x0};
const uint8_t notificationOn[] = {0x1, 0x0};
  
bool Nueva_Notificacion=false;
float temp;
float humi;
float bat;

/*Log App nRF 
Disabling notifications for ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6
gatt.setCharacteristicNotification(ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6, false)
gatt.writeDescriptor(00002902-0000-1000-8000-00805f9b34fb, value=0x0000)
Data written to descr. 00002902-0000-1000-8000-00805f9b34fb, value: (0x) 00-00

Enabling notifications for ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6
gatt.setCharacteristicNotification(ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6, true)
gatt.writeDescriptor(00002902-0000-1000-8000-00805f9b34fb, value=0x0100)
Data written to descr. 00002902-0000-1000-8000-00805f9b34fb, value: (0x) 01-00
*/

class MyClientCallback : public BLEClientCallbacks 
{
    void onConnect(BLEClient* pclient) {Serial.println("Connected");}
    void onDisconnect(BLEClient* pclient)
    { //Siempre se activa este evento cuando no conecta en la conexion de setup
      Serial.println("Disconnected");connectionSuccessful=false;
      //if (!connectionSuccessful) {Serial.println("RESTART");delay(5000);ESP.restart();}
    }
};

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic_THB,
  uint8_t* pData,
  size_t length,
  bool isNotify) 
  {
  //Serial.print("Notify callback for characteristic ");
  //Serial.println(pBLERemoteCharacteristic->getUUID().toString().c_str());
  Serial.printf("T= %#02x %#02x : H= %#02x : BatmV= %#02x %#02x \n", pData[0], pData[1], pData[2], pData[3], pData[4] );
  temp = (pData[0] | (pData[1] << 8)) * 0.01; //little endian
  humi = pData[2];
  bat = (pData[3] | (pData[4] << 8)) * 0.001; //little endian. Dato en mV y se pasa a voltios
  //Serial.printf("temp= %.2f ºC : humidity= %.2f \% : bat= %.3f v : TempRocio= %.2f ºC \n\n", temp, humi, bat, dewPointC(temp, humi));
  //pClient->disconnect();
 
  Nueva_Notificacion=true;
  }

void registerNotification() {
  // Obtain a reference to the service we are after in the remote BLE server.
  //BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  pRemoteService = pClient->getService(serviceUUID);  
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  //BLERemoteCharacteristic* pRemoteCharacteristic_THB = pRemoteService->getCharacteristic(charUUID);
  pRemoteCharacteristic_THB = pRemoteService->getCharacteristic(charUUID);  
  if (pRemoteCharacteristic_THB == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
  }
  Serial.println(" - Found our characteristic");
  pRemoteCharacteristic_THB->registerForNotify(notifyCallback);
}
unsigned long long currentMillis;
unsigned long long previousMillis;
unsigned long long interval = 60000000; //Lee T/H/Bat cada 1 minuto por notificaciones
unsigned long long looptime;
unsigned long long looptimeMax;
unsigned long long looptimeMin=1000000; //Valor de referencia max.
void setup() {
  Serial.begin(115200); 
  //Serial.begin(921600); 
  ESP32_info();
  
  Serial.println("Starting Mijia client...");
  BLEDevice::init("ESP32");
  createBleClientWithCallbacks();

  connectionSuccessful = false;
  previousMillis = micros();
}


void loop() 
{
  //currentMillis = millis();
  currentMillis = micros();  
  looptime = currentMillis;

  if (!connectionSuccessful) 
  {Serial.println("Reconectar....");
  connectSensor(); Serial.println("Reconectado y registrado"); }

  if(Nueva_Notificacion==true)
  {
    Nueva_Notificacion=false; 
    pRemoteCharacteristic_THB->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOff, 2, true);    
    Serial.printf("temp= %.2f ºC : humidity= %.2f \% : bat= %.3f v : TempRocio= %.2f ºC \n", temp, humi, bat, dewPointC(temp, humi));
    Serial.print("Notifications turned off ->"); Serial.printf("esp_get_free_heap_size: %d \n\n", esp_get_free_heap_size());
  } 
     
  if ( (currentMillis - previousMillis) > interval ) 
  {
    Serial.printf("Notifications turned on ->esp_get_free_heap_size: %d \n", esp_get_free_heap_size()); 
    pRemoteCharacteristic_THB->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
    previousMillis = currentMillis;
    Serial.println( micros());    
  }
  
  //looptime = millis() - looptime;
  looptime = micros() - looptime;   
  if (looptime > looptimeMax){ looptimeMax= looptime;Serial.printf("LooptimeMax: %d \n", looptimeMax);}
  if (looptime < looptimeMin){ looptimeMin= looptime;Serial.printf("LooptimeMin: %d \n", looptimeMin);}
  //if (looptime >= 1000){ Serial.printf("Looptime > 1s: %d \n", looptime);}  //Indica si el "loop" dura mas de 1 s.
  if (looptime >= 1000000){ Serial.printf("Looptime > 1s: %d \n", looptime);}  //Indica si el "loop" dura mas de 1 s.  
}

void createBleClientWithCallbacks() 
{
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
}

void connectSensor() 
{
  pClient->connect(htSensorAddress);
  //BLEAddress pClient->getPeerAddress();
  String BLE_address= pClient->getPeerAddress().toString().c_str();
  if(pClient->isConnected()==true)
  {
    connectionSuccessful = true;
    Serial.print("BLE_address= ");Serial.print(BLE_address); Serial.printf(" ->Rssi= %d \n", pClient->getRssi()); //advertisedDevice.getRSSI()
    const uint8_t SetCommInterval[] = {0xf4, 0x01};  //Intervalo de comunicacion= 0x01F4 = 500ms. Es el maximo en formato endian littel.
    // Obtain a reference to the service we are after in the remote BLE server.
    //BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    pRemoteService = pClient->getService(serviceUUID);
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_SetCommInterval);
    // Write the value of the characteristic.
    //pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)SetCommInterval, 2, true); 
    pRemoteCharacteristic->writeValue((uint8_t*)SetCommInterval, 2, true); 
 
    // Obtain a reference to the service we are after in the remote BLE server.
    pRemoteService = pClient->getService(serviceUUID);  
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic_THB = pRemoteService->getCharacteristic(charUUID);
    
    registerNotification();      
  }
}
//****************************************************************************//
//  Dew point Section
//****************************************************************************//
//Funcion copiada de la lib: https://github.com/sparkfun/SparkFun_BME280_Arduino_Library/blob/master/src/SparkFunBME280.cpp
// Returns Dew point in DegC
//double BME280::dewPointC(void)
double dewPointC(double celsius, double humidity)
{
  //double celsius = readTempC(); 
  //double humidity = readFloatHumidity();
  
  // (1) Saturation Vapor Pressure = ESGG(T)
  double RATIO = 373.15 / (273.15 + celsius);
  double RHS = -7.90298 * (RATIO - 1);
  RHS += 5.02808 * log10(RATIO);
  RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
  RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  RHS += log10(1013.246);
  double VP = pow(10, RHS - 3) * humidity;  // factor -3 is to adjust units - Vapor Pressure SVP * humidity
         // (2) DEWPOINT = F(Vapor Pressure)
  double T = log(VP/0.61078);   // temp var
  return (241.88 * T) / (17.558 - T);
}

//ESP32 info
//https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/Esp.h#L63
void ESP32_info()
{
  Serial.print("C++ version: "); Serial.println(__cplusplus); 
  Serial.printf("SDK_version: %s \n", ESP.getSdkVersion()); 
  Serial.printf("ESP32_ChipRevision: %d \n", ESP.getChipRevision());
  
uint64_t chipid3 = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chipid3 >> 32);
  Serial.printf("chip_serial: %04X%08X\n", chip, (uint32_t)chipid3);
  
String  stringChipId = getMacAddress();Serial.println(stringChipId);

  Serial.printf("ESP32_GetCpuFreqMHz(): %d \n", ESP.getCpuFreqMHz());
    
  //Internal RAM
  Serial.printf("ESP32_Total heap size: %d \n", ESP.getHeapSize());  //total heap size
  Serial.printf("ESP32_Available heap: %d \n", ESP.getFreeHeap());   //available heap
  Serial.printf("ESP32_Lowest level of free heap since boot: %d \n", ESP.getMinFreeHeap()); //lowest level of free heap since boot
  Serial.printf("ESP32_Largest block of heap that can be allocated at once: %d \n", ESP.getMaxAllocHeap()); //largest block of heap that can be allocated at once

  //Internal FLASH
  Serial.printf("ESP32_FlashChipSize: %d \n", ESP.getFlashChipSize());
  Serial.printf("ESP32_FlashChipSpeed: %d \n", ESP.getFlashChipSpeed());
  //Serial.printf("ESP32_FlashChipMode: %d \n", ESP.getFlashChipMode());   //FlashMode_t
/*
  Serial.printf("ESP32_magicFlashChipSize: %d \n", ESP.magicFlashChipSize());
  Serial.printf("ESP32_magicFlashChipSpeed: %d \n", ESP.magicFlashChipSpeed());
  Serial.printf("ESP32_magicFlashChipMode: %s \n", ESP.magicFlashChipMode());   //FlashMode_t
*/
  Serial.printf("ESP32_SketchSize: %d \n", ESP.getSketchSize());
  Serial.printf("ESP32_SketchMD5: %d \n", ESP.getSketchMD5());    
  Serial.printf("ESP32_FreeSketchSpace: %d \n", ESP.getFreeSketchSpace());

} 
 
//Call function to get custom Mac address
String getMacAddress() 
{
    uint8_t baseMac[6];
    // Get MAC address for WiFi station
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char baseMacChr[18] = {0};
    sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    return String(baseMacChr);
}
