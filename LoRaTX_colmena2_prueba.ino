// Incluir bibliotecas necesarias para el manejo de sensores, comunicación LoRa y funciones del tiempo
#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "HX711.h"
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Ticker.h>
#include <Wire.h>
#include "RTClib.h"

// Definir configuraciones y parámetros de LoRa
#define IdLora "Id_02"   // Identificador único para la colmena 1 (cambiar para colmena 2)
#define RF_FREQUENCY 915000000 // Frecuencia de LoRa en Hz, común en América del Norte
#define TX_OUTPUT_POWER 5        // Potencia de transmisión LoRa en dBm
#define LORA_BANDWIDTH 0         // Configuración de ancho de banda LoRa a 125 kHz
#define LORA_SPREADING_FACTOR 7  // Factor de esparcimiento SF7, equilibrio entre alcance y velocidad
#define LORA_CODINGRATE 1        // Tasa de codificación 4/5, eficiencia en la codificación de datos
#define LORA_PREAMBLE_LENGTH 8   // Longitud de preámbulo LoRa para la sincronización
#define LORA_SYMBOL_TIMEOUT 0    // Timeout de símbolos desactivado
#define LORA_FIX_LENGTH_PAYLOAD_ON false  // Uso de payload de longitud variable
#define LORA_IQ_INVERSION_ON false        // Desactivación de inversión IQ

// Definir valores de timeout y tamaño de buffer para transmisión de datos
#define RX_TIMEOUT_VALUE 1000    // Timeout de recepción en milisegundos
#define BUFFER_SIZE 30           // Tamaño del buffer para almacenar datos a transmitir
#define uS_TO_S_FACTOR 1000000   // Factor de conversión de microsegundos a segundos
#define TIME_TO_SLEEP 750        // Tiempo de sueño en segundos (12.5 minutos)

// Configurar almacenamiento de estado entre reinicios usando RTC memory
RTC_DATA_ATTR int bootCount = 0;

// Configurar pines para sensores
const int PinDs18B20 = 23; 
const int LOADCELL_DOUT_PIN = 13;
const int LOADCELL_SCK_PIN = 12;
#define DHTPIN    17

// Variables para almacenar datos de sensores
float ValHmdAm2301,ValHx711, ValTempAm2301,ValDs18B20;   
OneWire oneWire(PinDs18B20);
DallasTemperature SenDs18B20(&oneWire);
HX711 scale;
#define DHTTYPE DHT21       // Tipo de sensor DHT configurado
DHT dht(DHTPIN, DHTTYPE);

// Configurar RTC y variables de tiempo
RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
String StrFechaHora;

// Buffers para paquetes de datos
char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

// Variables de control de estado y transmisión
double txNumber;
bool lora_idle=true;
boolean BandEnviarTrama = false;

// Declarar funciones de eventos LoRa y manejo de sueño
void print_wakeup_reason();
static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

// Función de configuración inicial del dispositivo
void setup() {
    Serial.begin(115200); // Iniciar comunicación serial para depuración
    Mcu.begin();          // Iniciar configuraciones específicas del microcontrolador
    
    txNumber=0;           // Inicializar contador de transmisiones

    // Configurar eventos de radio LoRa
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    
    // Iniciar y configurar radio LoRa
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, 3000); 

    // Iniciar sensores
    SenDs18B20.begin();
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(26.488); // Calibrar sensor de peso
    dht.begin();             // Iniciar sensor DHT

    // Iniciar y configurar RTC
    if (!rtc.begin()) {
        Serial.println("No se puede encontrar al modulo RTC");
        while (1);  // Bloquear si no se encuentra el RTC
    }

    if (!rtc.isrunning()) {
        Serial.println("RTC no esta TRABAJANDO!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Ajustar RTC a la hora de compilación
    }

    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));

    print_wakeup_reason(); // Imprimir razón del despertar

    // Configurar despertar automático
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

    BandEnviarTrama = true; // Habilitar bandera de envío de datos
}

// Bucle principal de ejecución
void loop() {
    if (BandEnviarTrama) {
        BandEnviarTrama = false; // Reiniciar bandera de envío

        // Leer y procesar datos de los sensores
        SenDs18B20.requestTemperatures(); 
        ValDs18B20 = SenDs18B20.getTempCByIndex(0);
  
        if (scale.is_ready()) {
            int valorSensor = scale.get_units(10); // Leer promedio de 10 mediciones
            Serial.print("valorSensor: "); Serial.print(valorSensor); 
            ValHx711 = valorSensor;
        } else {
            Serial.println("HX711 no encontrado.");
            ValHx711 = 0;
        }
                   
        ValHmdAm2301 = dht.readHumidity();
        ValTempAm2301 = dht.readTemperature();
            
        if (isnan(ValHmdAm2301) || isnan(ValTempAm2301)) {
            ValHmdAm2301 = 0;
            ValTempAm2301 = 0;
            Serial.println("Error de lectura del sensor DHT22!");
        }

        // Obtener y formatear la fecha y hora actuales
        DateTime now = rtc.now();
        int Anio = now.year();
        byte Mes = now.month();
        byte Dia = now.day();
        byte Hora = now.hour();
        byte Minuto = now.minute();
        byte Segundo = now.second();

        // Construir cadena de fecha y hora
        StrFechaHora = String(Anio, DEC);
        if (Mes > 9) StrFechaHora += "-" + String(Mes, DEC); else StrFechaHora += "-0" + String(Mes, DEC);
        if (Dia > 9) StrFechaHora += "-" + String(Dia, DEC); else StrFechaHora += "-0" + String(Dia, DEC);
        StrFechaHora += " ";
        if (Hora > 9) StrFechaHora += String(Hora, DEC); else StrFechaHora += "0" + String(Hora, DEC);
        if (Minuto > 9) StrFechaHora += ":" + String(Minuto, DEC); else StrFechaHora += ":0" + String(Minuto, DEC);
        if (Segundo > 9) StrFechaHora += ":" + String(Segundo, DEC); else StrFechaHora += ":0" + String(Segundo, DEC);

        // Construir y enviar el mensaje
        String StrMsg = String(IdLora) + "&" + String(ValDs18B20, 2) + "&" + String(ValHx711, 2) + "&" + String(ValTempAm2301, 2) + "&" + String(ValHmdAm2301, 2) + "&" + StrFechaHora;
        Serial.print(" ValDs18B20: "); Serial.println(ValDs18B20);
        Serial.print(" ValHx711: "); Serial.println(ValHx711);
        Serial.print(" ValTempAm2301: "); Serial.println(ValTempAm2301);
        Serial.print(" ValHmdAm2301: "); Serial.println(ValHmdAm2301);
        Serial.print(" StrFechaHora: "); Serial.println(StrFechaHora);
        int str_len = StrMsg.length() + 1;
        char char_Msg[str_len];
        StrMsg.toCharArray(char_Msg, str_len);
        sprintf(txpacket, char_Msg); // Preparar paquete de datos
        Serial.printf("\r\nEnviando Paquete \"%s\" , longitud %d\r\n", txpacket, strlen(txpacket));
        Radio.Send((uint8_t *)txpacket, strlen(txpacket)); // Enviar paquete por LoRa
        lora_idle = false;
    }

    Radio.IrqProcess(); // Procesar interrupciones de radio
}

// Funciones adicionales para eventos de LoRa y manejo de errores
void OnTxDone(void) {
    Serial.println("TX done......");
    lora_idle = true;
    esp_deep_sleep_start(); // Entrar en modo de sueño profundo después de enviar datos
    Serial.println("This will never be printed");
}

void OnTxTimeout(void) {
    Radio.Sleep();
    Serial.println("TX Timeout......");
    lora_idle = true;
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER: Serial.println("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
        default: Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}
