# ESP32 Solenoid Valve Controller

Controle de válvula solenoide via relé pelo ESP32, acionado por uma interface web acessível na rede local.

## Funcionalidades

- Interface web responsiva servida diretamente pelo ESP32
- Controle manual: botões **Abrir** e **Fechar** independentes
- **Agendamentos diários**: até 10 horários configurados pela interface, com duração individual por agendamento
- Fechamento automático após o tempo configurado (não bloqueia o servidor)
- Relógio em tempo real sincronizado via NTP (servidores NIC.br, fuso BRT UTC-3)
- Agendamentos persistidos em NVS — sobrevivem a reboot e queda de energia
- Estado da válvula sincronizado ao carregar a página
- IP estático configurável
- Credenciais WiFi isoladas em arquivo separado (fora do versionamento)

## Hardware

| Componente | Especificação |
|---|---|
| Microcontrolador | ESP32 Dev Kit V1 |
| Módulo relé | 1 canal, ativo em LOW, VCC 5V |
| Válvula solenoide | 12V DC (NC — normalmente fechada) |
| Alimentação válvula | Fonte externa 12V |

### Pinagem

```
ESP32 GPIO 21  →  IN  do módulo relé
ESP32 GND      →  GND do módulo relé
ESP32 VIN/5V   →  VCC do módulo relé

Relé COM       →  positivo (+) da fonte 12V
Relé NO        →  positivo (+) da válvula
Válvula GND    →  negativo (−) da fonte 12V
```

> O pino do relé é definido em [`src/main.cpp`](src/main.cpp) na constante `RELAY_PIN`.

## Configuração

### 1. Credenciais WiFi

Copie o arquivo de exemplo e preencha com sua rede:

```bash
cp include/wifi_secrets.example.h include/wifi_secrets.h
```

Edite `include/wifi_secrets.h`:

```cpp
#define WIFI_SECRET_SSID     "nome_da_rede"
#define WIFI_SECRET_PASSWORD "senha_da_rede"
```

> `wifi_secrets.h` não deve ser commitado. Adicione ao `.gitignore`.

### 2. IP estático

Ajuste em [`src/main.cpp`](src/main.cpp) conforme sua rede:

```cpp
static const IPAddress WIFI_IP(192, 168, 1, 22);
static const IPAddress WIFI_GW(192, 168, 1,  1);
static const IPAddress WIFI_SN(255, 255, 255, 0);
```

### 3. Gravar no ESP32

```bash
pio run --target upload
```

Monitorar Serial (115200 baud):

```bash
pio device monitor
```

Saída esperada no boot:

```
Relay pin: GPIO 21 — HIGH (closed)
Loaded 2 schedule(s) from NVS.
Connecting to WiFi....
Connected! Visit: http://192.168.1.22
NTP sync requested.
HTTP server started.
```

## Uso

Após gravado e conectado ao WiFi, acesse no navegador:

```
http://192.168.1.22
```

### Interface web

| Seção | Descrição |
|---|---|
| Relógio | Hora real sincronizada com NTP, atualizada a cada segundo |
| Controle Manual | Abre ou fecha a válvula imediatamente; exibe contador regressivo se houver fechamento automático agendado |
| Agendamentos Diários | Lista de horários cadastrados com estado (Ativo/Pausado), botões Pausar/Ativar e Excluir, e formulário para adicionar novos |

### Endpoints da API

| Rota | Descrição | Resposta |
|---|---|---|
| `GET /` | Página de controle | HTML |
| `GET /open` | Abre a válvula (relé LOW) | `{"open":true,"closeIn":0}` |
| `GET /close` | Fecha a válvula (relé HIGH) | `{"open":false,"closeIn":0}` |
| `GET /status` | Estado atual da válvula | `{"open":false,"closeIn":120}` |
| `GET /time` | Hora atual do ESP32 (NTP) | `{"synced":true,"time":"14:32:05"}` |
| `GET /schedules` | Lista todos os agendamentos | array JSON |
| `GET /schedule/add?h=&m=&d=` | Adiciona agendamento (`d` em segundos) | `{"ok":true}` |
| `GET /schedule/delete?id=` | Remove agendamento pelo índice | `{"ok":true}` |
| `GET /schedule/toggle?id=` | Ativa ou pausa agendamento | `{"ok":true,"enabled":false}` |

Exemplo de resposta de `/schedules`:

```json
[
  { "id": 0, "hour": 7, "minute": 0, "duration": 300, "enabled": true },
  { "id": 1, "hour": 18, "minute": 30, "duration": 120, "enabled": false }
]
```

## Estrutura do projeto

```
esp32_solenoide/
├── include/
│   ├── wifi_secrets.h           # credenciais reais (não versionar)
│   └── wifi_secrets.example.h   # template de credenciais
├── src/
│   └── main.cpp                 # firmware principal
├── platformio.ini
└── README.md
```

## Dependências

Gerenciadas automaticamente pelo PlatformIO:

- `espressif32` platform
- `WiFi.h` — biblioteca nativa ESP32
- `WebServer.h` — biblioteca nativa ESP32
- `Preferences.h` — biblioteca nativa ESP32 (NVS)
- `time.h` — biblioteca nativa ESP32 (NTP + timezone POSIX)
