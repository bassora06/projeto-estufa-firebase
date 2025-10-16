// =================================================================
// INCLUSÃO DE BIBLIOTECAS
// =================================================================
#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "time.h" // <<< ADICIONADA BIBLIOTECA PARA CONTROLE DE TEMPO

// =================================================================
// SUAS CREDENCIAIS
// =================================================================
#define WIFI_SSID "LIA"
#define WIFI_PASSWORD "Lucas1974"
#define API_KEY "AIzaSyC-d7s582MNZtJMRIra23w2UFsi7WHavKQ"
#define DATABASE_URL "https://estufa-b2a52-default-rtdb.firebaseio.com"

// =================================================================
// DEFINIÇÃO DE PINOS E CONFIGURAÇÕES
// =================================================================
#define DHTPIN 27
#define DHTTYPE DHT22
#define SOILPIN 34
#define LDR_DIGITAL_PIN 26
#define LDR_ANALOG_PIN  25

// =================================================================
// CONFIGURAÇÕES DE TEMPO (NTP)
// =================================================================
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800; // Offset para GMT-3 (Brasília)
const int   daylightOffset_sec = 0;   // Sem horário de verão

// =================================================================
// CALIBRAÇÃO E INTERVALOS
// =================================================================
int valorSoloSeco = 4095;
int valorSoloMolhado = 1700;
#define INTERVALO 5000 

// =================================================================
// OBJETOS GLOBAIS
// =================================================================
DHT dht(DHTPIN, DHTTYPE);
unsigned long delayIntervalo;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(115200);
  Serial.println("--- Iniciando Estação de Monitoramento com Firebase ---");

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

  // --- [NOVO] SINCRONIZAÇÃO DE TEMPO VIA NTP ---
  Serial.println("Sincronizando o tempo...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("!!! Falha ao obter a hora do servidor NTP.");
    // Opcional: você pode decidir parar aqui se o tempo for crítico
  } else {
    Serial.println("Tempo sincronizado com sucesso!");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S"); // Imprime a data e hora
  }

  // --- Configuração do Firebase ---
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // --- Configuração dos Sensores ---
  pinMode(LDR_DIGITAL_PIN, INPUT);
  dht.begin();
  delayIntervalo = millis() - INTERVALO;
}

void loop() {
  // O código do loop continua exatamente o mesmo
  if ((millis() - delayIntervalo) >= INTERVALO) {
    delayIntervalo = millis();

    // ... (resto do seu código do loop que já está correto) ...
    // --- 1. LEITURA DE TODOS OS SENSORES ---
    float umidadeAr = dht.readHumidity();
    float temperatura = dht.readTemperature();
    int valorUmidadeSolo = analogRead(SOILPIN);
    int valorLuzAnalogico = analogRead(LDR_ANALOG_PIN);

    if (isnan(umidadeAr) || isnan(temperatura)) {
      Serial.println("!!! Falha ao ler o sensor DHT22!");
      return;
    }

    // --- 2. PROCESSAMENTO DOS DADOS ---
    int umidadeSoloPorcentagem = map(valorUmidadeSolo, valorSoloSeco, valorSoloMolhado, 0, 100);
    umidadeSoloPorcentagem = constrain(umidadeSoloPorcentagem, 0, 100);

    // --- 3. EXIBIÇÃO LOCAL (NO MONITOR SERIAL) ---
    Serial.println("---------------------------------");
    Serial.print("Umidade do Solo: ");
    Serial.print(umidadeSoloPorcentagem);
    Serial.println("%");
    Serial.print("Umidade do Ar: ");
    Serial.print(umidadeAr, 1);
    Serial.print("% / Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.println(" *C");
    Serial.print("Luminosidade: ");
    Serial.println(valorLuzAnalogico);
    
    // --- 4. ENVIO DOS DADOS PARA O FIREBASE (COM VERIFICAÇÃO SEPARADA) ---
    if (WiFi.status() == WL_CONNECTED) {
      if (Firebase.ready()) {
        FirebaseJson json;
        json.set("temperatura", String(temperatura, 1));
        json.set("umidadeAr", String(umidadeAr, 1));
        json.set("umidadeSolo", umidadeSoloPorcentagem);
        json.set("luminosidade", valorLuzAnalogico);
        json.set("timestamp/.sv", "timestamp");

        String path = "/leiturasAtuais";
        
        Serial.printf("Enviando dados para o Firebase em: %s\n", path.c_str());
        
        if (Firebase.RTDB.setJSON(&fbdo, path, &json)) {
          Serial.println(">> Dados enviados com sucesso!");
        } else {
          Serial.println(">> ERRO ao enviar dados: " + fbdo.errorReason());
        }

      } else {
        Serial.println("!!! Conectado ao Wi-Fi, mas o Firebase não está pronto.");
        Serial.println("!!! Causa provável: " + fbdo.errorReason()); // Adiciona mais detalhes do erro
      }

    } else {
      Serial.println("!!! Falha na conexão Wi-Fi. Sem comunicação com a internet.");
    }
    Serial.println("---------------------------------");
  }
}