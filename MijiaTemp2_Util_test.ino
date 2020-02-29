//https://github.com/karolkalinski/esp32-snippets
//https://randomnerdtutorials.com/esp32-bluetooth-low-energy-ble-arduino-ide/
#include <BLEDevice.h>
#include <SimpleTimer.h>

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
static BLEUUID charUUID_SetIntervalComm("ebe0ccd8-7a0a-4b0c-8a1a-6ff2997da3a6");
static BLEUUID charUUID_03("00002902-0000-1000-8000-00805f9b34fb");
bool Nueva_Notificacion=false;
float temp;
float humi;
float bat;
/*Log´s of App nRF 
Disabling notifications for ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6
gatt.setCharacteristicNotification(ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6, false)
gatt.writeDescriptor(00002902-0000-1000-8000-00805f9b34fb, value=0x0000)
Data written to descr. 00002902-0000-1000-8000-00805f9b34fb, value: (0x) 00-00

Enabling notifications for ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6
gatt.setCharacteristicNotification(ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6, true)
gatt.writeDescriptor(00002902-0000-1000-8000-00805f9b34fb, value=0x0100)
Data written to descr. 00002902-0000-1000-8000-00805f9b34fb, value: (0x) 01-00
*/
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
      Serial.println("Connected");
    }
    void onDisconnect(BLEClient* pclient) {
      Serial.println("Disconnected");
      if (!connectionSuccessful) {
        Serial.println("RESTART");
        ESP.restart();
      }
    }
};

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
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
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
  }
  Serial.println(" - Found our service");

  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
  }
  Serial.println(" - Found our characteristic");
  pRemoteCharacteristic->registerForNotify(notifyCallback);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting MJ client...");
  //delay(500);

  BLEDevice::init("ESP32");
  createBleClientWithCallbacks();
  //delay(500);
  connectSensor();
  registerNotification();

  const uint8_t SetCommInterval[] = {0xf4, 0x01};  //Intervalo de comunicacion= 0x01F4 = 500ms. Es el maximo en formato endian littel.
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID_SetIntervalComm);
  // Write the value of the characteristic.
  //pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)SetCommInterval, 2, true); 
  pRemoteCharacteristic->writeValue((uint8_t*)SetCommInterval, 2, true);   
}

unsigned long currentMillis;
unsigned long previousMillis;
unsigned long interval = 300000; //Lee T/H/Bat cada 5 minutos por notificaciones

const uint8_t notificationOff[] = {0x0, 0x0};
const uint8_t notificationOn[] = {0x1, 0x0};
void loop() 
{
  currentMillis = millis();
  
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  // Obtain a reference to the characteristic in the service of the remote BLE server.
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  
  if (currentMillis - previousMillis > interval) 
  {
    previousMillis = currentMillis;
    Serial.println("Notifications turned on");
    pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
  }
  if(Nueva_Notificacion==true)
  {
    Serial.printf("temp= %.2f ºC : humidity= %.2f \% : bat= %.3f v : TempRocio= %.2f ºC \n", temp, humi, bat, dewPointC(temp, humi));
    pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOff, 2, true);
    Serial.println("Notifications turned off \n"); Nueva_Notificacion=false;  
  }  
}

void createBleClientWithCallbacks() {
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
}

void connectSensor() {
  pClient->connect(htSensorAddress);
  connectionSuccessful = true;
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
