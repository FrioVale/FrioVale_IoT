
# FRIOVALE | IoT Firmware

<p align="center">
  <strong>Smart Cold Chain Monitoring for Fruit Logistics in the São Francisco Valley</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-blue" />
  <img src="https://img.shields.io/badge/Communication-MQTT-orange" />
  <img src="https://img.shields.io/badge/Telemetry-ThingSpeak-blueviolet" />
  <img src="https://img.shields.io/badge/Project-Integrative%20Project-0b7c8a" />
</p>

<details open>
<summary><strong>🇺🇸 English version</strong></summary>

## 1. About the project

FrioVale is an IoT-based monitoring project designed to support cold-chain management in fruit storage and transportation in the São Francisco Valley, Brazil.

The system aims to monitor environmental conditions during the transportation and storage of products such as grapes and mangoes, helping identify temperature variations that may compromise fruit quality.

This repository contains the firmware responsible for collecting sensor readings, processing operational information and transmitting telemetry to the ThingSpeak platform.

## 2. Project objectives

- Monitor environmental temperature and humidity.
- Collect sensor data using an ESP32 microcontroller.
- Transmit telemetry through Wi-Fi and MQTT.
- Display collected information through ThingSpeak.
- Identify temperature conditions outside the configured operating range.
- Record operational events and communication failures.
- Provide a foundation for integration with the FrioVale backend and dashboard.

## 3. System architecture

```text
DHT11 Sensor
     |
     v
ESP32 Microcontroller
     |
     +----> Temperature and humidity readings
     |
     +----> Local operational logs (LittleFS)
     |
     v
Wi-Fi Connection
     |
     v
MQTT Broker (ThingSpeak)
     |
     v
ThingSpeak Channel
     |
     v
Telemetry Visualization
```

**Important:** the current firmware publishes telemetry to ThingSpeak. Direct transmission from the ESP32 to the FrioVale backend API is a required integration step and must be implemented and tested separately.

## 4. Hardware components

| Component | Function |
|---|---|
| ESP32 | Microcontroller and Wi-Fi connectivity |
| DHT11 | Temperature and humidity measurement |
| Red LED | Warning indication |
| Green LED | Normal operating indication |
| Power supply | Powers the device |
| Jumper wires | Electrical connections |
| Protective enclosure | Physical protection |

The current source code uses one DHT11 sensor. Temperature and humidity are two measurements from the same sensor, not two independent sensors.

A second physical sensor must be added to meet the professor's requirement of at least two sensors.

## 5. Pin configuration

| Component | ESP32 GPIO |
|---|---|
| DHT11 data | GPIO 4 |
| Red LED | GPIO 5 |
| Green LED | GPIO 19 |

Check the actual wiring before powering the circuit.

## 6. Software and libraries

### Requirements

- Arduino IDE
- ESP32 board support package
- ESP32 development board
- Wi-Fi network
- ThingSpeak account and configured channels

### Libraries

- WiFi.h (ESP32 core)
- PubSubClient
- DHT sensor library
- LittleFS (ESP32 core)
- esp_system.h (ESP32 core)

### Installation

1. Install Arduino IDE.
2. Add ESP32 board support.
3. Select the correct ESP32 board.
4. Install PubSubClient through the Arduino Library Manager.
5. Install the DHT sensor library.
6. Connect the ESP32 to the computer.
7. Select the correct serial port.

## 7. Configuration

The firmware requires the following configuration:

- Wi-Fi SSID and password.
- ThingSpeak MQTT broker address.
- MQTT authentication credentials.
- ThingSpeak channel identifiers.
- Device identification.
- Sensor and LED GPIO pins.

**Security requirement:** credentials must not be committed to the public repository.

Move sensitive configuration to a private local configuration file, exclude it through `.gitignore`, and provide only a sanitized example file such as:

```cpp
// credentials.example.h
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define MQTT_CLIENT_ID "YOUR_CLIENT_ID"
#define MQTT_USERNAME "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
```

The example file must never contain production credentials.

## 8. Firmware operation

### 8.1 Sensor acquisition

The DHT11 provides:

- Temperature in degrees Celsius (°C).
- Relative humidity in percentage (%).

The current configured acquisition interval is 15 seconds.

### 8.2 Temperature monitoring

The current firmware defines the following temperature range:

- Minimum: 7 °C.
- Maximum: 10 °C.

The LEDs indicate the operating condition:

- Green LED: temperature within the configured range.
- Red LED: temperature outside the configured range or invalid sensor reading.

These values are prototype configuration parameters and must be validated against the requirements of the transported fruit and the actual logistics operation.

### 8.3 MQTT communication

The firmware uses PubSubClient to communicate with the ThingSpeak MQTT broker.

Telemetry fields:

| ThingSpeak field | Information |
|---|---|
| field1 | Temperature |
| field2 | Relative humidity |

The current MQTT configuration uses port 1883. TLS-secured MQTT communication has not been established in the published firmware and must be implemented before claiming secure end-to-end transmission.

### 8.4 Wi-Fi and MQTT reconnection

The firmware periodically checks Wi-Fi and MQTT connectivity.

Current retry intervals:

- Wi-Fi: 10 seconds.
- MQTT: 5 seconds.

Operational events are registered when connections are lost or restored.

### 8.5 Local operational logs

LittleFS is used to store operational events in a local CSV file.

Examples of recorded events include:

- Device startup.
- Device restart.
- Wi-Fi disconnection.
- MQTT disconnection.
- Wi-Fi reconnection.
- MQTT reconnection.

The current local queue concerns operational logs. It does not yet provide a complete offline buffer for sensor telemetry.

## 9. Data flow

The current telemetry flow is:

1. The DHT11 measures temperature and humidity.
2. The ESP32 validates the readings.
3. The firmware evaluates the temperature range.
4. The ESP32 builds the MQTT payload.
5. The telemetry is published to ThingSpeak.
6. ThingSpeak stores and visualizes the received values.

## 10. Running the firmware

1. Clone the repository:

```bash
git clone https://github.com/FrioVale/FrioVale_IoT.git
```

2. Open the project in Arduino IDE.
3. Configure the ESP32 board and serial port.
4. Install the required libraries.
5. Configure Wi-Fi and MQTT credentials locally.
6. Verify the electrical connections.
7. Upload the firmware.
8. Open the Serial Monitor at 115200 baud.
9. Check Wi-Fi and MQTT connection messages.
10. Verify incoming data in the ThingSpeak channel.

## 11. Resilience requirements

The project follows the resilience requirements defined in the Integrative Project technical assessment.

| ID | Requirement | Current status |
|---|---|---|
| R1 | Automatic Wi-Fi and server reconnection | Partially implemented |
| R2 | Local telemetry buffer and retransmission | Pending |
| R3 | Device ID, sequence number and UTC timestamp | Pending |
| R4 | Task watchdog and automatic recovery | Pending |
| R5 | Invalid sensor reading detection | Implemented for NaN readings |
| R6 | Controlled degradation during service failures | Pending |
| R7 | Heartbeat and device availability monitoring | Partially implemented |
| R8 | Cloud service recovery and backup | Backend/Cloud responsibility |

Statuses must be updated only after implementation and testing.

## 12. Required failure tests

| Test | Description | Expected evidence |
|---|---|---|
| F1 | Disconnect Wi-Fi for 60 seconds | Automatic reconnection and no missing telemetry sequence |
| F2 | Interrupt device power | Automatic restart and recovery |
| F3 | Disconnect the sensor data wire | Invalid reading identification and visible warning |
| F4 | Stop the cloud API | Local retention and retransmission |
| F5 | Send a duplicated message | Only one database record |
| F6 | Make ThingSpeak unavailable | The cloud API flow continues operating |

F1 and F3 are required for the technical preview. F1 through F6 are required for the final delivery.

## 13. Planned improvements

- Add a second physical sensor.
- Implement telemetry buffering in LittleFS.
- Add device sequence numbers.
- Add UTC timestamps synchronized through NTP.
- Implement a hardware or task watchdog.
- Implement secure MQTT communication.
- Integrate the ESP32 directly with the FrioVale backend API.
- Add heartbeat reporting.
- Improve enclosure protection.
- Document sensor calibration.
- Perform and document failure tests.

## 14. Repository structure

Current source:

```text
FrioVale_IoT/
├── README.md
└── main.ino
```

Suggested structure as the implementation evolves:

```text
FrioVale_IoT/
├── README.md
├── main.ino
├── include/
│   └── credentials.example.h
├── diagrams/
│   ├── electrical-schematic.png
│   └── architecture.png
├── docs/
│   ├── components.md
│   ├── sensor-calibration.md
│   └── firmware-tests.md
├── images/
│   ├── prototype-internal.jpg
│   └── prototype-enclosure.jpg
└── .gitignore
```

The suggested folders must be created and populated before they are presented as implemented project artifacts.

## 15. Security

- Never publish Wi-Fi passwords.
- Never publish MQTT passwords or write API keys.
- Revoke credentials exposed in previous commits.
- Do not reuse compromised credentials.
- Use TLS for production communication.
- Store production secrets outside the source code.
- Keep sanitized configuration examples in the repository.

## 16. Academic context

This repository is part of the FrioVale Integrative Project, developed for the Internet of Things course.

The technical assessment includes hardware integration, firmware resilience, cloud communication, traceability, security and measurable failure testing.

</details>

<details>
<summary><strong>🇧🇷 Versão em português</strong></summary>

## 1. Sobre o projeto

O FrioVale é um projeto de monitoramento baseado em Internet das Coisas (IoT), desenvolvido para apoiar a gestão da cadeia do frio no armazenamento e transporte de frutas do Vale do São Francisco.

O sistema busca monitorar as condições ambientais durante o transporte e armazenamento de produtos como uvas e mangas, auxiliando na identificação de variações de temperatura que possam comprometer a qualidade das frutas.

Este repositório contém o firmware responsável pela coleta das leituras dos sensores, processamento das informações operacionais e transmissão da telemetria para a plataforma ThingSpeak.

## 2. Objetivos

- Monitorar temperatura e umidade do ambiente.
- Coletar dados por meio de um microcontrolador ESP32.
- Transmitir telemetria utilizando Wi-Fi e MQTT.
- Disponibilizar os dados coletados no ThingSpeak.
- Identificar condições de temperatura fora da faixa configurada.
- Registrar eventos operacionais e falhas de comunicação.
- Fornecer uma base para integração com o back-end e dashboard do FrioVale.

## 3. Arquitetura do sistema

```text
Sensor DHT11
     |
     v
Microcontrolador ESP32
     |
     +----> Leituras de temperatura e umidade
     |
     +----> Logs operacionais locais (LittleFS)
     |
     v
Conexão Wi-Fi
     |
     v
Broker MQTT (ThingSpeak)
     |
     v
Canal ThingSpeak
     |
     v
Visualização da telemetria
```

**Importante:** o firmware atual publica a telemetria no ThingSpeak. O envio direto do ESP32 para a API do FrioVale é uma etapa de integração obrigatória que ainda precisa ser implementada e testada separadamente.

## 4. Componentes de hardware

| Componente | Função |
|---|---|
| ESP32 | Microcontrolador e conexão Wi-Fi |
| DHT11 | Medição de temperatura e umidade |
| LED vermelho | Indicação de alerta |
| LED verde | Indicação de funcionamento normal |
| Fonte de alimentação | Alimentação do dispositivo |
| Jumpers | Conexões elétricas |
| Caixa de proteção | Proteção física |

O código atual utiliza um sensor DHT11. Temperatura e umidade são duas medições realizadas pelo mesmo sensor, não dois sensores independentes.

Um segundo sensor físico deverá ser adicionado para atender à exigência do professor de pelo menos dois sensores.

## 5. Configuração dos pinos

| Componente | GPIO do ESP32 |
|---|---|
| Dados do DHT11 | GPIO 4 |
| LED vermelho | GPIO 5 |
| LED verde | GPIO 19 |

Confira a ligação elétrica antes de alimentar o circuito.

## 6. Tecnologias e bibliotecas

### Requisitos

- Arduino IDE.
- Suporte à placa ESP32.
- Placa de desenvolvimento ESP32.
- Rede Wi-Fi.
- Conta ThingSpeak e canais configurados.

### Bibliotecas

- WiFi.h (núcleo ESP32).
- PubSubClient.
- Biblioteca de sensores DHT.
- LittleFS (núcleo ESP32).
- esp_system.h (núcleo ESP32).

### Instalação

1. Instale a Arduino IDE.
2. Adicione o suporte às placas ESP32.
3. Selecione a placa correta.
4. Instale PubSubClient pelo gerenciador de bibliotecas.
5. Instale a biblioteca DHT.
6. Conecte o ESP32 ao computador.
7. Selecione a porta serial correta.

## 7. Configuração

O firmware necessita das seguintes configurações:

- SSID e senha do Wi-Fi.
- Endereço do broker MQTT do ThingSpeak.
- Credenciais de autenticação MQTT.
- Identificadores dos canais ThingSpeak.
- Identificação do dispositivo.
- Pinos GPIO dos sensores e LEDs.

**Requisito de segurança:** credenciais não devem ser publicadas no repositório.

As configurações sensíveis devem ser transferidas para um arquivo local privado, excluído pelo `.gitignore`. No GitHub, deve permanecer apenas um exemplo sem credenciais reais.

## 8. Funcionamento do firmware

### 8.1. Coleta dos sensores

O DHT11 fornece:

- Temperatura em graus Celsius (°C).
- Umidade relativa em porcentagem (%).

O intervalo de coleta configurado atualmente é de 15 segundos.

### 8.2. Monitoramento de temperatura

O firmware utiliza a seguinte faixa de temperatura:

- Mínima: 7 °C.
- Máxima: 10 °C.

Os LEDs indicam a condição operacional:

- LED verde: temperatura dentro da faixa configurada.
- LED vermelho: temperatura fora da faixa ou leitura inválida.

Esses valores são parâmetros do protótipo e precisam ser validados de acordo com as exigências das frutas transportadas e da operação logística.

### 8.3. Comunicação MQTT

O firmware utiliza PubSubClient para comunicação com o broker MQTT do ThingSpeak.

Campos de telemetria:

| Campo ThingSpeak | Informação |
|---|---|
| field1 | Temperatura |
| field2 | Umidade relativa |

A configuração MQTT atual utiliza a porta 1883. A comunicação MQTT com TLS ainda não está estabelecida no firmware publicado e precisa ser implementada antes de afirmar que a transmissão ponta a ponta é segura.

### 8.4. Reconexão Wi-Fi e MQTT

O firmware verifica periodicamente a conexão Wi-Fi e MQTT.

Intervalos atuais de tentativa:

- Wi-Fi: 10 segundos.
- MQTT: 5 segundos.

Os eventos operacionais são registrados quando as conexões são perdidas ou recuperadas.

### 8.5. Logs operacionais locais

O LittleFS é utilizado para armazenar eventos operacionais em um arquivo CSV local.

Exemplos de eventos:

- Inicialização do dispositivo.
- Reinicialização.
- Desconexão do Wi-Fi.
- Desconexão do MQTT.
- Reconexão do Wi-Fi.
- Reconexão do MQTT.

A fila local atual é destinada aos logs operacionais. Ela ainda não oferece um buffer completo das leituras dos sensores durante a ausência de internet.

## 9. Fluxo dos dados

O fluxo atual da telemetria é:

1. O DHT11 mede temperatura e umidade.
2. O ESP32 valida as leituras.
3. O firmware avalia a faixa de temperatura.
4. O ESP32 constrói a mensagem MQTT.
5. A telemetria é publicada no ThingSpeak.
6. O ThingSpeak armazena e apresenta os valores.

## 10. Como executar o firmware

1. Clone o repositório:

```bash
git clone https://github.com/FrioVale/FrioVale_IoT.git
```

2. Abra o projeto na Arduino IDE.
3. Configure a placa ESP32 e a porta serial.
4. Instale as bibliotecas necessárias.
5. Configure localmente as credenciais Wi-Fi e MQTT.
6. Confira as conexões elétricas.
7. Faça o upload do firmware.
8. Abra o Monitor Serial em 115200 baud.
9. Confira as mensagens de conexão Wi-Fi e MQTT.
10. Verifique a chegada dos dados no canal ThingSpeak.

## 11. Requisitos de resiliência

O projeto segue os requisitos de resiliência definidos na avaliação técnica do Projeto Integrador.

| ID | Requisito | Situação atual |
|---|---|---|
| R1 | Reconexão automática Wi-Fi e servidor | Parcialmente implementado |
| R2 | Buffer local de telemetria e reenvio | Pendente |
| R3 | ID do dispositivo, sequência e horário UTC | Pendente |
| R4 | Watchdog e recuperação automática | Pendente |
| R5 | Identificação de leituras inválidas | Implementado para leituras NaN |
| R6 | Degradação controlada em falhas de serviços | Pendente |
| R7 | Heartbeat e monitoramento de disponibilidade | Parcialmente implementado |
| R8 | Recuperação de serviços e backup na nuvem | Responsabilidade do Back-end/Cloud |

Os status devem ser atualizados somente após implementação e testes.

## 12. Testes de falha obrigatórios

| Teste | Descrição | Evidência esperada |
|---|---|---|
| F1 | Desconectar o Wi-Fi por 60 segundos | Reconexão automática e sequência de telemetria sem lacunas |
| F2 | Interromper a alimentação do dispositivo | Reinicialização e recuperação automáticas |
| F3 | Desconectar o fio de dados do sensor | Identificação de leitura inválida e alerta visível |
| F4 | Interromper a API na nuvem | Retenção local e reenvio |
| F5 | Enviar uma mensagem duplicada | Apenas um registro no banco |
| F6 | Indisponibilizar o ThingSpeak | Continuidade do fluxo pela API em nuvem |

F1 e F3 são obrigatórios na prévia técnica. F1 a F6 são obrigatórios na entrega final.

## 13. Melhorias planejadas

- Adicionar um segundo sensor físico.
- Implementar buffer de telemetria no LittleFS.
- Adicionar numeração sequencial das mensagens.
- Adicionar horário UTC sincronizado por NTP.
- Implementar watchdog.
- Implementar comunicação MQTT segura.
- Integrar diretamente o ESP32 à API do FrioVale.
- Adicionar envio de heartbeat.
- Melhorar a proteção da caixa.
- Documentar a calibração dos sensores.
- Executar e documentar os testes de falha.

## 14. Estrutura do repositório

Estrutura atual:

```text
FrioVale_IoT/
├── README.md
└── main.ino
```

Estrutura sugerida para evolução:

```text
FrioVale_IoT/
├── README.md
├── main.ino
├── include/
│   └── credentials.example.h
├── diagrams/
│   ├── esquema-eletrico.png
│   └── arquitetura.png
├── docs/
│   ├── componentes.md
│   ├── calibracao-sensores.md
│   └── testes-firmware.md
├── images/
│   ├── prototipo-interno.jpg
│   └── prototipo-caixa.jpg
└── .gitignore
```

As pastas sugeridas deverão ser criadas e preenchidas antes de serem apresentadas como artefatos implementados.

## 15. Segurança

- Nunca publique senhas de Wi-Fi.
- Nunca publique senhas MQTT ou chaves de escrita.
- Revogue credenciais expostas em commits anteriores.
- Não reutilize credenciais comprometidas.
- Utilize TLS na comunicação de produção.
- Armazene segredos fora do código-fonte.
- Mantenha exemplos de configuração sem credenciais reais.

## 16. Contexto acadêmico

Este repositório faz parte do Projeto Integrador FrioVale, desenvolvido para a disciplina de Internet das Coisas.

A avaliação técnica envolve integração de hardware, resiliência do firmware, comunicação com a nuvem, rastreabilidade, segurança e testes de falhas mensuráveis.

</details>
