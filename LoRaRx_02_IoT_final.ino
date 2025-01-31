#include <LoRaWAN_ESP32.h>

#include "Arduino.h"
#include "LoRaWan_APP.h"
#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid     = "CNT_starlink";
const char* password = "robert_2024";

//Remplazar por los datos correspondientes
const char *mqtt_server = "***********";
const int mqtt_port = *******;
const char *mqtt_user = "**********";
const char *mqtt_pass = "**********";


#define TopicSend "Apicultura/Colmena"

WiFiClient espClient;
PubSubClient client(espClient);


#define Led_Recepcion 13
#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             14        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
//  1: 250 kHz,
//  2: 500 kHz,
//  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
//  2: 4/6,
//  3: 4/7,
//  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 90 // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
static RadioEvents_t RadioEvents;
int16_t txNumber;
int16_t rssi, rxSize;
bool lora_idle = true;

String StrDataLoraIn, StrIndx, StrFechaHora;
float ValDs18B20, ValHx711, ValTempAm2301, ValHmdAm2301;
bool BandLoraIn;


void setup_wifi();
void reconnect();
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
String getValue(String data, char separator, int index);

void setup() {
  Serial.begin(115200);
  pinMode(Led_Recepcion, OUTPUT);
  digitalWrite(Led_Recepcion, LOW);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Mcu.begin();

  txNumber = 0;
  rssi = 0;

  RadioEvents.RxDone = OnRxDone;
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                     LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                     0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

}


void loop()
{

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  if (lora_idle)
  {
    lora_idle = false;
    Serial.println("into RX mode");
    Radio.Rx(0);
  }
  Radio.IrqProcess( );

}

void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
  rssi = rssi;
  rxSize = size;

  Serial.println();
  Serial.println();
  Serial.println();

  memcpy(rxpacket, payload, size );
  rxpacket[size] = '\0';
  Radio.Sleep( );

  Serial.println("Ingreso data lora");

  Serial.printf("\r\nPaquete Recibido \"%s\" con RSSI %d , LONGITUD %d\r\n", rxpacket, rssi, rxSize);

  StrDataLoraIn = String(rxpacket);


  StrIndx = getValue(StrDataLoraIn, '&', 0);
  ValDs18B20 = (getValue(StrDataLoraIn, '&', 1)).toFloat();
  ValHx711 = (getValue(StrDataLoraIn, '&', 2)).toFloat();
  ValTempAm2301 = (getValue(StrDataLoraIn, '&', 3)).toFloat();
  ValHmdAm2301 = (getValue(StrDataLoraIn, '&', 4)).toFloat();
  StrFechaHora = getValue(StrDataLoraIn, '&', 5);

  Serial.print(" Index: "); Serial.println(StrIndx);
  Serial.print(" ValDs18B20: "); Serial.println(ValDs18B20);
  Serial.print(" ValHx711: "); Serial.println(ValHx711);
  Serial.print(" ValTempAm2301: "); Serial.println(ValTempAm2301);
  Serial.print(" ValHmdAm2301: "); Serial.println(ValHmdAm2301);
  Serial.print(" StrFechaHora: "); Serial.println(StrFechaHora);


  int str_len = StrDataLoraIn.length() + 1;
  char char_Msg[str_len];
  StrDataLoraIn.toCharArray(char_Msg, str_len);
  client.publish(TopicSend, char_Msg);

  lora_idle = true;
}


String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setup_wifi()
{
  delay(10);

  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Conectado a red WiFi!");
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}


void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Intentando conexión Mqtt...");
    // Creamos un cliente ID
    String clientId = "esp32_";
    clientId += String(random(0xffff), HEX);
    // Intentamos conectar
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass))
    {
      Serial.println("Conectado!");

      // client.publish(TopicSend , "Iniciando");

    } else {

      Serial.print("falló :( con error -> ");
      Serial.print(client.state());
      Serial.println(" Intentamos de nuevo en 5 segundos");
      delay(5000);
    }
  }
}
