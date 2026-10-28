#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LittleFS.h>
#include <esp_system.h>


// ==================================================
// 1. CONFIGURAÇÕES DE REDE
// ==================================================
const char* ssid = "user";
const char* password = "senha";


// ==================================================
// 2. CREDENCIAIS MQTT DO THINGSPEAK
// ==================================================
const char* mqtt_server = "mqtt3.thingspeak.com";
const int mqtt_port = 1883;


const char* mqttClientID = "HxsXBxMVDQEbDhYhOgU4Gwo";
const char* mqttUser = "HxsXBxMVDQEbDhYhOgU4Gwo";
const char* mqttPass = "t+ztAq6H1lIhG7KPP9eh5+VL";


// Canal de telemetria
const unsigned long channelID = 3513340;


// Canal de logs operacionais
const unsigned long logChannelID = 3551998;
const char* logWriteAPIKey =
    "HB0G19ZZ7ACIW0K7";


// Identificação da placa
const char* numeroSerie = "FV001";
const int idNumericoPlaca = 1;


// ==================================================
// 3. MQTT, SENSOR E PINOS
// ==================================================
WiFiClient espClient;
PubSubClient client(espClient);


#define DHT_PIN 4
#define DHT_TYPE DHT11


#define LED_VERMELHO 5
#define LED_VERDE 19


#define TEMP_MIN 7.0
#define TEMP_MAX 10.0


DHT dht(DHT_PIN, DHT_TYPE);


// ==================================================
// 4. FILA LOCAL DE LOGS
// ==================================================
const char* arquivoLogs = "/logs.csv";
const char* arquivoTemporario = "/logs_tmp.csv";


bool armazenamentoOK = false;


// ==================================================
// 5. INTERVALOS
// ==================================================
const unsigned long intervaloLeitura = 15000;
const unsigned long intervaloLog = 16000;
const unsigned long intervaloReconexao = 5000;
const unsigned long intervaloWiFi = 10000;


unsigned long ultimoEnvioTelemetria = 0;
unsigned long ultimoEnvioLog = 0;
unsigned long ultimaTentativaMQTT = 0;
unsigned long ultimaTentativaWiFi = 0;


bool wifiEstavaConectado = false;
bool mqttEstavaConectado = false;


// ==================================================
// CÓDIGOS DOS EVENTOS
// ==================================================
// 100 = inicialização
// 200 = reinicialização
// 300 = perda de Wi-Fi
// 310 = perda de MQTT
// 320 = Wi-Fi recuperado
// 330 = MQTT conectado/recuperado
//
// Estado:
// 0 = comunicação indisponível
// 1 = comunicação disponível
// 2 = inicialização/reinicialização
//
// Fields do canal de logs:
// Field 1 = código do evento
// Field 2 = ID numérico da placa
// Field 3 = estado operacional
// Field 4 = causa do reset
//
// O número de série textual será enviado no status.


// ==================================================
// 6. REGISTRAR EVENTO NA FILA LOCAL
// ==================================================
void registrarLog(int codigo, int estado, int causaReset) {
  Serial.print("[LOG] Evento: ");
  Serial.print(codigo);


  Serial.print(" | Série: ");
  Serial.print(numeroSerie);


  Serial.print(" | Estado: ");
  Serial.print(estado);


  Serial.print(" | Causa do reset: ");
  Serial.println(causaReset);


  if (!armazenamentoOK) {
    Serial.println(
        "[LOG] ERRO: LittleFS indisponível; evento não foi salvo."
    );
    return;
  }


  File arquivo = LittleFS.open(arquivoLogs, FILE_APPEND);


  if (!arquivo) {
    Serial.println("[LOG] ERRO ao abrir arquivo.");
    return;
  }


  arquivo.print(codigo);
  arquivo.print(",");
  arquivo.print(numeroSerie);
  arquivo.print(",");
  arquivo.print(idNumericoPlaca);
  arquivo.print(",");
  arquivo.print(estado);
  arquivo.print(",");
  arquivo.println(causaReset);


  arquivo.close();
}


// ==================================================
// 7. REMOVER O PRIMEIRO REGISTRO DA FILA
// ==================================================
void removerPrimeiroLog() {
  File origem = LittleFS.open(arquivoLogs, FILE_READ);


  if (!origem) {
    return;
  }


  // Descarta a primeira linha.
  origem.readStringUntil('\n');


  File destino = LittleFS.open(arquivoTemporario, FILE_WRITE);


  if (!destino) {
    origem.close();
    return;
  }


  while (origem.available()) {
    String linha = origem.readStringUntil('\n');
    linha.trim();


    if (linha.length() > 0) {
      destino.println(linha);
    }
  }


  origem.close();
  destino.close();


  LittleFS.remove(arquivoLogs);
  LittleFS.rename(arquivoTemporario, arquivoLogs);
}


// ==================================================
// 8. ENVIAR LOG PENDENTE AO THINGSPEAK
// ==================================================
void enviarProximoLog() {
  if (!armazenamentoOK || !client.connected()) {
    return;
  }


  unsigned long agora = millis();


  if (agora - ultimoEnvioLog < intervaloLog) {
    return;
  }


  File arquivo = LittleFS.open(arquivoLogs, FILE_READ);


  if (!arquivo) {
    return;
  }


  String linha = arquivo.readStringUntil('\n');
  arquivo.close();
  linha.trim();


  if (linha.length() == 0) {
    return;
  }


  int p1 = linha.indexOf(',');
  int p2 = linha.indexOf(',', p1 + 1);
  int p3 = linha.indexOf(',', p2 + 1);
  int p4 = linha.indexOf(',', p3 + 1);


  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) {
    Serial.println(
        "[LOG] Registro inválido; removendo da fila."
    );


    removerPrimeiroLog();
    return;
  }


  String codigo = linha.substring(0, p1);
  String serie = linha.substring(p1 + 1, p2);
  String idPlaca = linha.substring(p2 + 1, p3);
  String estado = linha.substring(p3 + 1, p4);
  String causa = linha.substring(p4 + 1);


  causa.trim();


  // Codificação dos caracteres especiais de status.
  String status =
      "Serie%3D" + serie + "%3BEvento%3D" + codigo;


  String payload =
      "api_key=" + String(logWriteAPIKey) +
      "&field1=" + codigo +
      "&field2=" + idPlaca +
      "&field3=" + estado +
      "&field4=" + causa +
      "&status=" + status;


  String topico =
      "channels/" + String(logChannelID) + "/publish";


  ultimoEnvioLog = agora;


  if (client.publish(topico.c_str(), payload.c_str())) {
    Serial.println(
        "[LOG] Mensagem enviada via MQTT; verifique o canal."
    );


    // O retorno positivo do publish não garante que
    // o ThingSpeak gravou o registro.
    removerPrimeiroLog();
  } else {
    Serial.println(
        "[LOG] Falha no envio. Registro mantido na fila."
    );
  }
}


// ==================================================
// 9. INICIAR E MONITORAR O WI-FI
// ==================================================
void iniciarWiFi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  ultimaTentativaWiFi = millis();
}


void verificarWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);


  if (conectado && !wifiEstavaConectado) {
    wifiEstavaConectado = true;


    Serial.print("Wi-Fi conectado. IP: ");
    Serial.println(WiFi.localIP());


    registrarLog(320, 1, 0);
  }


  if (!conectado && wifiEstavaConectado) {
    wifiEstavaConectado = false;


    registrarLog(300, 0, 0);


    Serial.println("Falha/perda de Wi-Fi.");
  }


  if (
      !conectado &&
      millis() - ultimaTentativaWiFi >= intervaloWiFi
  ) {
    WiFi.disconnect();
    WiFi.begin(ssid, password);


    ultimaTentativaWiFi = millis();
  }
}


// ==================================================
// 10. CONECTAR E MONITORAR O MQTT
// ==================================================
void verificarMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }


  if (client.connected()) {
    if (!mqttEstavaConectado) {
      mqttEstavaConectado = true;


      registrarLog(330, 1, 0);


      Serial.println("MQTT conectado ao ThingSpeak.");
    }


    return;
  }


  if (mqttEstavaConectado) {
    mqttEstavaConectado = false;


    registrarLog(310, 0, 0);


    Serial.println("Conexão MQTT perdida.");
  }


  if (
      millis() - ultimaTentativaMQTT <
      intervaloReconexao
  ) {
    return;
  }


  ultimaTentativaMQTT = millis();


  Serial.println("Tentando conectar ao MQTT...");


  if (client.connect(mqttClientID, mqttUser, mqttPass)) {
    mqttEstavaConectado = true;


    registrarLog(330, 1, 0);


    Serial.println("MQTT conectado.");
  } else {
    Serial.print("Falha MQTT. Código: ");
    Serial.println(client.state());
  }
}


// ==================================================
// 11. ENVIAR TEMPERATURA E UMIDADE
// ==================================================
void enviarTelemetria() {
  if (!client.connected()) {
    return;
  }


  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();


  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("ERRO AO LER DHT11!");


    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);


    return;
  }


  Serial.println("----------------------------");


  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.println(" °C");


  Serial.print("Umidade: ");
  Serial.print(umidade, 1);
  Serial.println(" %");


  if (
      temperatura >= TEMP_MIN &&
      temperatura <= TEMP_MAX
  ) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);


    Serial.println("Status: NORMAL");
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);


    Serial.println("Status: ALERTA");
  }


  String payload =
      "field1=" + String(temperatura, 1) +
      "&field2=" + String(umidade, 1);


  String topico =
      "channels/" + String(channelID) + "/publish";


  if (client.publish(topico.c_str(), payload.c_str())) {
    Serial.println("Telemetria enviada ao ThingSpeak.");
  } else {
    Serial.println("Falha ao enviar telemetria via MQTT.");
  }


  Serial.println("----------------------------");
}


// ==================================================
// 12. CONFIGURAÇÃO INICIAL
// ==================================================
void setup() {
  Serial.begin(115200);


  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);


  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);


  dht.begin();


  // Atenção: true pode formatar LittleFS se a montagem falhar,
  // apagando logs locais que ainda existam.
  armazenamentoOK = LittleFS.begin(true);


  if (!armazenamentoOK) {
    Serial.println("ERRO: não foi possível iniciar LittleFS.");
  } else {
    Serial.println("LittleFS pronto.");
  }


  Serial.println("================================");
  Serial.println("       FRIOVALE - MQTT");
  Serial.println("    Telemetria + Logs");
  Serial.print("Número de série: ");
  Serial.println(numeroSerie);
  Serial.println("================================");


  int causaReset = (int)esp_reset_reason();


  registrarLog(100, 2, causaReset);


  if (esp_reset_reason() != ESP_RST_POWERON) {
    registrarLog(200, 2, causaReset);
  }


  client.setServer(mqtt_server, mqtt_port);
  client.setKeepAlive(30);


  iniciarWiFi();
}


// ==================================================
// 13. LOOP PRINCIPAL
// ==================================================
void loop() {
  verificarWiFi();
  verificarMQTT();


  if (client.connected()) {
    client.loop();
  }


  unsigned long agora = millis();


  if (
      agora - ultimoEnvioTelemetria >= intervaloLeitura
  ) {
    ultimoEnvioTelemetria = agora;
    enviarTelemetria();
  }


  enviarProximoLog();
}

