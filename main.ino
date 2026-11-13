#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <time.h>


// ==================================================
// 1. CONFIGURAÇÕES DE REDE
// ==================================================
const char* ssid = "USER";
const char* password = "SENHA";


// ==================================================
// 2. CREDENCIAIS MQTT DO THINGSPEAK
// ==================================================
// Use as credenciais NOVAS geradas em Devices > MQTT.
// Não publique este arquivo com as credenciais preenchidas.
const char* mqtt_server = "mqtt3.thingspeak.com";
const int mqtt_port = 1883;

const char* mqttClientID = "HxsXBxMVDQEbDhYhOgU4Gwo";
const char* mqttUser     = "TESTE";
const char* mqttPass     = "TESTE";

// Canal de telemetria
const unsigned long channelID = 3513340;

// Canal de logs operacionais
// (via MQTT a autorização vem do dispositivo MQTT; não é preciso API key)
const unsigned long logChannelID = 3551998;

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
#define LED_VERDE 6

#define TEMP_MIN 7.0
#define TEMP_MAX 10.0

DHT dht(DHT_PIN, DHT_TYPE);


// ==================================================
// 4. FILA LOCAL DE LOGS
// ==================================================
// Formato de cada linha do CSV:
// codigo,serie,idPlaca,estado,causaReset,epochUTC,uptimeSeg
const char* arquivoLogs = "/logs.csv";
const char* arquivoTemporario = "/logs_tmp.csv";

const size_t TAMANHO_MAX_FILA = 8000;  // bytes

bool armazenamentoOK = false;
bool temLogsPendentes = true;  // true no boot: pode haver sobra de antes do reset


// ==================================================
// 5. INTERVALOS
// ==================================================
const unsigned long intervaloLeitura = 15000;
const unsigned long intervaloLog = 16000;  // ThingSpeak grátis: mín. 15 s por canal
const unsigned long intervaloReconexao = 5000;
const unsigned long intervaloWiFi = 10000;

unsigned long ultimoEnvioTelemetria = 0;
unsigned long ultimoEnvioLog = 0;
unsigned long ultimaTentativaMQTT = 0;
unsigned long ultimaTentativaWiFi = 0;

bool wifiEstavaConectado = false;
bool mqttEstavaConectado = false;

// Estados para eventos disparados só na transição
bool sensorEmFalha = false;
bool tempForaFaixa = false;


// ==================================================
// CÓDIGOS DOS EVENTOS
// ==================================================
// 100 = inicialização
// 200 = reinicialização
// 300 = perda de Wi-Fi
// 310 = perda de MQTT
// 320 = Wi-Fi recuperado
// 330 = MQTT conectado/recuperado
// 400 = temperatura fora da faixa
// 410 = temperatura normalizada
// 500 = falha no sensor DHT
// 510 = sensor DHT recuperado
//
// Estado:
// 0 = problema / indisponível
// 1 = normal / disponível
// 2 = inicialização/reinicialização
//
// Causa do reset (esp_reset_reason):
// 1 = energia (power on)   3 = software    4 = panic/exceção
// 5 = int. watchdog        6 = task WDT    7 = outro WDT
// 9 = brownout (queda de tensão)
//
// Fields do canal de logs:
// Field 1 = código do evento
// Field 2 = ID numérico da placa
// Field 3 = estado operacional
// Field 4 = causa do reset
//
// No status vão: série, evento, TS (epoch UTC real do evento),
// UP (segundos desde o boot) e a descrição do evento.


// ==================================================
// DESCRIÇÃO TEXTUAL DOS EVENTOS (sem espaços)
// ==================================================
const char* descricaoEvento(int codigo) {
  switch (codigo) {
    case 100: return "Placa_iniciada";
    case 200: return "Placa_reiniciada";
    case 300: return "WiFi_perdido";
    case 310: return "MQTT_perdido";
    case 320: return "WiFi_recuperado";
    case 330: return "MQTT_conectado";
    case 400: return "Temp_fora_da_faixa";
    case 410: return "Temp_normalizada";
    case 500: return "Falha_sensor_DHT";
    case 510: return "Sensor_DHT_recuperado";
    default:  return "Evento_desconhecido";
  }
}


// ==================================================
// UTILITÁRIO: HORA ATUAL (epoch UTC) OU 0 SE NÃO SINCRONIZOU
// ==================================================
unsigned long epochAtual() {
  time_t agora = time(nullptr);

  if (agora < 1700000000) {
    return 0;  // NTP ainda não sincronizou
  }

  return (unsigned long)agora;
}


// ==================================================
// 6. REMOVER O PRIMEIRO REGISTRO DA FILA
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
// 7. REGISTRAR EVENTO NA FILA LOCAL
// ==================================================
void registrarLog(int codigo, int estado, int causaReset) {
  unsigned long epoch = epochAtual();
  unsigned long uptime = millis() / 1000;

  Serial.print("[LOG] Evento: ");
  Serial.print(codigo);
  Serial.print(" (");
  Serial.print(descricaoEvento(codigo));
  Serial.print(") | Série: ");
  Serial.print(numeroSerie);
  Serial.print(" | Estado: ");
  Serial.print(estado);
  Serial.print(" | Causa do reset: ");
  Serial.print(causaReset);
  Serial.print(" | TS: ");
  Serial.print(epoch);
  Serial.print(" | UP: ");
  Serial.println(uptime);

  if (!armazenamentoOK) {
    Serial.println("[LOG] ERRO: LittleFS indisponível; evento não foi salvo.");
    return;
  }

  // Fila cheia: descarta o registro mais antigo para abrir espaço.
  File teste = LittleFS.open(arquivoLogs, FILE_READ);

  if (teste) {
    size_t tamanho = teste.size();
    teste.close();

    if (tamanho > TAMANHO_MAX_FILA) {
      Serial.println("[LOG] Fila cheia; descartando o registro mais antigo.");
      removerPrimeiroLog();
    }
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
  arquivo.print(causaReset);
  arquivo.print(",");
  arquivo.print(epoch);
  arquivo.print(",");
  arquivo.println(uptime);

  arquivo.close();

  temLogsPendentes = true;
}


// ==================================================
// 8. ENVIAR LOG PENDENTE AO THINGSPEAK
// ==================================================
void enviarProximoLog() {
  if (!temLogsPendentes || !armazenamentoOK || !client.connected()) {
    return;
  }

  unsigned long agora = millis();

  if (agora - ultimoEnvioLog < intervaloLog) {
    return;
  }

  File arquivo = LittleFS.open(arquivoLogs, FILE_READ);

  if (!arquivo) {
    temLogsPendentes = false;
    return;
  }

  String linha = arquivo.readStringUntil('\n');
  arquivo.close();
  linha.trim();

  if (linha.length() == 0) {
    temLogsPendentes = false;  // fila vazia
    return;
  }

  // Separa os 7 campos: codigo,serie,idPlaca,estado,causa,epoch,uptime
  String campos[7];
  int inicio = 0;
  bool valido = true;

  for (int i = 0; i < 7; i++) {
    int virgula = linha.indexOf(',', inicio);

    if (i < 6) {
      if (virgula < 0) {
        valido = false;
        break;
      }

      campos[i] = linha.substring(inicio, virgula);
      inicio = virgula + 1;
    } else {
      campos[i] = linha.substring(inicio);
    }
  }

  if (!valido) {
    Serial.println("[LOG] Registro inválido; removendo da fila.");
    removerPrimeiroLog();
    return;
  }

  String codigo = campos[0];
  String serie  = campos[1];
  String idPlaca = campos[2];
  String estado = campos[3];
  String causa  = campos[4];
  String epoch  = campos[5];
  String uptime = campos[6];

  // %3D = "=" e %3B = ";" para não quebrar o payload MQTT
  String status =
      "Serie%3D" + serie +
      "%3BEvento%3D" + codigo +
      "%3BTS%3D" + epoch +
      "%3BUP%3D" + uptime +
      "%3B" + String(descricaoEvento(codigo.toInt()));

  String payload =
      "field1=" + codigo +
      "&field2=" + idPlaca +
      "&field3=" + estado +
      "&field4=" + causa +
      "&status=" + status;

  String topico = "channels/" + String(logChannelID) + "/publish";

  ultimoEnvioLog = agora;

  if (client.publish(topico.c_str(), payload.c_str())) {
    Serial.println("[LOG] Mensagem enviada via MQTT; verifique o canal.");

    // O retorno positivo do publish não garante que
    // o ThingSpeak gravou o registro.
    removerPrimeiroLog();
  } else {
    Serial.println("[LOG] Falha no envio. Registro mantido na fila.");
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

  if (!conectado && millis() - ultimaTentativaWiFi >= intervaloWiFi) {
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    ultimaTentativaWiFi = millis();
  }
}


// ==================================================
// 10. CONECTAR E MONITORAR O MQTT
// ==================================================
void verificarMQTT() {
  // Se o Wi-Fi caiu, o MQTT também caiu: registra a perda uma única vez.
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttEstavaConectado && !client.connected()) {
      mqttEstavaConectado = false;
      registrarLog(310, 0, 0);
      Serial.println("Conexão MQTT perdida (sem Wi-Fi).");
    }

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

  if (millis() - ultimaTentativaMQTT < intervaloReconexao) {
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
// 11. LER SENSOR, GERAR EVENTOS E ENVIAR TELEMETRIA
// ==================================================
// O sensor é lido mesmo sem MQTT, para que falhas e
// temperaturas fora da faixa sejam registradas offline.
void processarTelemetria() {
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  // ---- Falha no sensor ----
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("ERRO AO LER DHT11!");

    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);

    if (!sensorEmFalha) {
      sensorEmFalha = true;
      registrarLog(500, 0, 0);
    }

    return;
  }

  if (sensorEmFalha) {
    sensorEmFalha = false;
    registrarLog(510, 1, 0);
  }

  // ---- Faixa de temperatura ----
  bool foraFaixa = (temperatura < TEMP_MIN || temperatura > TEMP_MAX);

  if (foraFaixa && !tempForaFaixa) {
    tempForaFaixa = true;
    registrarLog(400, 0, 0);
  }

  if (!foraFaixa && tempForaFaixa) {
    tempForaFaixa = false;
    registrarLog(410, 1, 0);
  }

  Serial.println("----------------------------");

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.println(" °C");

  Serial.print("Umidade: ");
  Serial.print(umidade, 1);
  Serial.println(" %");

  if (!foraFaixa) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);

    Serial.println("Status: NORMAL");
  } else {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);

    Serial.println("Status: ALERTA");
  }

  // ---- Envio ao canal de telemetria ----
  if (client.connected()) {
    String payload =
        "field1=" + String(temperatura, 1) +
        "&field2=" + String(umidade, 1);

    String topico = "channels/" + String(channelID) + "/publish";

    if (client.publish(topico.c_str(), payload.c_str())) {
      Serial.println("Telemetria enviada ao ThingSpeak.");
    } else {
      Serial.println("Falha ao enviar telemetria via MQTT.");
    }
  } else {
    Serial.println("MQTT offline: telemetria não enviada.");
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

  // NTP: o epoch é sempre UTC. Para Recife, converta com UTC-3
  // ao ler (ou use configTime(-3 * 3600, 0, ...) se preferir).
  // A sincronização acontece sozinha quando o Wi-Fi conectar.
  configTime(0, 0, "pool.ntp.org", "time.google.com");

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

  if (agora - ultimoEnvioTelemetria >= intervaloLeitura) {
    ultimoEnvioTelemetria = agora;
    processarTelemetria();
  }

  enviarProximoLog();
}
