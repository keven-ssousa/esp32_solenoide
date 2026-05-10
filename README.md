# ESP32 Solenoid Valve Controller

Controle de válvula solenoide via relé pelo ESP32, acionado por uma interface web acessível na rede local.

## Funcionalidades

- Interface web responsiva servida diretamente pelo ESP32
- Dois botões independentes: **Abrir** e **Fechar** a válvula
- Estado sincronizado ao carregar a página (`/status`)
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
ESP32 GPIO 14  →  IN  do módulo relé
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

## Uso

Após gravado e conectado ao WiFi, acesse no navegador:

```
http://192.168.1.22
```

### Endpoints da API

| Método | Rota | Descrição |
|---|---|---|
| GET | `/` | Página de controle |
| GET | `/open` | Abre a válvula (relé LOW) |
| GET | `/close` | Fecha a válvula (relé HIGH) |
| GET | `/status` | Retorna estado atual em JSON |

Exemplo de resposta de `/status`:

```json
{ "open": false }
```

## Estrutura do projeto

```
esp32_solenoide/
├── include/
│   ├── wifi_secrets.h          # credenciais reais (não versionar)
│   └── wifi_secrets.example.h  # template de credenciais
├── src/
│   └── main.cpp                # firmware principal
├── platformio.ini
└── README.md
```

## Dependências

Gerenciadas automaticamente pelo PlatformIO:

- `espressif32` platform
- `WiFi.h` — biblioteca nativa ESP32
- `WebServer.h` — biblioteca nativa ESP32
