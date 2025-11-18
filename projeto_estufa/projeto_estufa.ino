// =================================================================
// INCLUSÃO DE BIBLIOTECAS
// =================================================================
#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// =================================================================
// CONFIGURAÇÃO DE WIFI E FIREBASE
// =================================================================
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define FIREBASE_HOST "" // sem https:// e sem / no final

// =================================================================
// DEFINIÇÃO DE PINOS
// =================================================================
#define DHTPIN 27
#define DHTTYPE DHT22
#define SOILPIN 34
#define LDR_DIGITAL_PIN 26
#define LDR_ANALOG_PIN 32

// =================================================================
// CALIBRAÇÃO E INTERVALOS
// =================================================================
int valorSoloSeco = 4095;
int valorSoloMolhado = 1700;
#define INTERVALO 5000 // 5 segundos entre leituras

// =================================================================
// OBJETOS GLOBAIS
// =================================================================
DHT dht(DHTPIN, DHTTYPE);
unsigned long delayIntervalo = 0;

// =================================================================
// SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Iniciando Estação de Monitoramento Local ---");

  // --- Conexão Wi-Fi ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nConectado com sucesso!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  // --- Inicialização de sensores ---
  pinMode(LDR_DIGITAL_PIN, INPUT);
  dht.begin();
  delayIntervalo = millis() - INTERVALO;
}

// =================================================================
// LOOP PRINCIPAL
// =================================================================
void loop() {
  if ((millis() - delayIntervalo) >= INTERVALO) {
    delayIntervalo = millis();

    // --- 1. LEITURA DOS SENSORES ---
    float umidadeAr = dht.readHumidity();
    float temperatura = dht.readTemperature();
    int valorSolo = analogRead(SOILPIN);
    int valorLuzAnalogico = analogRead(LDR_ANALOG_PIN);
    int estadoLuzDigital = digitalRead(LDR_DIGITAL_PIN);

    if (isnan(umidadeAr) || isnan(temperatura)) {
      Serial.println("!!! Falha ao ler o sensor DHT22!");
      return;
    }

    // --- 2. PROCESSAMENTO DOS DADOS ---
    int umidadeSoloPorcentagem = map(valorSolo, valorSoloSeco, valorSoloMolhado, 0, 100);
    umidadeSoloPorcentagem = constrain(umidadeSoloPorcentagem, 0, 100);

    // --- 3. ENVIO PARA O FIREBASE ---
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String("https://") + FIREBASE_HOST + "/estufa.json"; // nó "estufa"
      http.begin(url);
      http.addHeader("Content-Type", "application/json");

      // Montar JSON com todos os sensores
      String payload = "{";
      payload += "\"temperatura\":" + String(temperatura, 1);
      payload += ",\"umidadeAr\":" + String(umidadeAr, 1);
      payload += ",\"umidadeSolo\":" + String(umidadeSoloPorcentagem);
      payload += ",\"luzAnalogica\":" + String(valorLuzAnalogico);
      payload += "}";

      int httpCode = http.POST(payload);

      if (httpCode > 0) {
        String resp = http.getString();
        Serial.printf("POST %d -> %s\n", httpCode, resp.c_str());
      } else {
        Serial.printf("Erro no POST: %d\n", httpCode);
      }

      http.end();
    } else {
      Serial.println("WiFi desconectado.");
    }

    // --- 4. EXIBIÇÃO LOCAL ---
    Serial.println("---------------------------------");
    Serial.printf("Temperatura: %.1f °C | Umidade Ar: %.1f%%\n", temperatura, umidadeAr);
    Serial.printf("Umidade Solo: %d%% | Luz: %d (analog)\n", umidadeSoloPorcentagem, valorLuzAnalogico);
    Serial.println("---------------------------------");
  }
}
