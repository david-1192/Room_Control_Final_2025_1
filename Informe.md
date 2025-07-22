# German Santiago Bernal Hoyos
# Cristian David Chalaca Salas

---

**🚀Estructuras Computacionales S1-2025 – Room Control**

---
### **1. Arquitectura de Hardware**


graph TD
    MCU[STM32L476RG]
    OLED[OLED SSD1306]
    Keypad[Teclado Matricial 4x4]
    NTC[Sensor NTC]
    Fan[Ventilador PWM]
    DOOR[DOOR_STATUS]
    ESP01[WiFi ESP-01]
    LD2[LED Heartbeat]

    MCU -- I2C --> OLED
    MCU -- GPIO --> Keypad
    MCU -- GPIO PA4 --> DOOR
    MCU -- GPIO PA5 --> LD2
    MCU -- ADC PA0 --> NTC
    MCU -- PWM PA6 --> Fan
    MCU -- UART3 PA2,PA3 --> ESP01

## Interfaces:

- **I2C:** pantalla OLED

- **GPIO:** teclado, Heartbeat, control de puerta

- **ADC:** sensor de temperatura NTC

- **PWM:** ventilador

- **UART3:** módulo WiFi ESP-01

### **2. Arquitectura de Firmware**

- **Super Loop:**
    El firmware principal ejecuta un bucle infinito donde se actualizan sensores, se procesan eventos y se actualiza la lógica del sistema.

- **State Machine:**
    Implementa una máquina de estados para manejar el acceso a la habitación y las transiciones entre estados. LOCKED, INPUT_PASSWORD, UNLOCKED, ACCESS_DENIED.

Cada estado define el comportamiento del sistema. Las transiciones dependen de eventos como teclas, sensores o tiempos.

- **ROOM_STATE_LOCKED:**
El sistema está bloqueado. Solo permite iniciar ingreso de contraseña.

- **ROOM_STATE_INPUT_PASSWORD:**
El usuario está ingresando la contraseña. Se aceptan dígitos y letras ('A'-'D'). Se espera # para validar.

- **ROOM_STATE_UNLOCKED:**
Acceso concedido. Se activa el indicador de acceso y se muestra temperatura y nivel del ventilador. Puede volver a bloquearse con *.

- **ROOM_STATE_ACCESS_DENIED:**
Acceso denegado. Se muestra mensaje de error y regresa a LOCKED tras un tiempo.


stateDiagram-v2
  direction TB
  [*] --> LOCKED: Inicio
  LOCKED --> INPUT_PASSWORD: Tecla válida
  INPUT_PASSWORD --> CHECK_PASSWORD: Presiona #
  INPUT_PASSWORD --> LOCKED: Espera (10 seg)
  CHECK_PASSWORD --> UNLOCKED: Contraseña correcta
  CHECK_PASSWORD --> ACCESS_DENIED: Contraseña incorrecta
  UNLOCKED --> LOCKED: Presiona *
  ACCESS_DENIED --> LOCKED: Espera (3 seg)
Diagrama de Flujo del Firmware

flowchart TD
  subgraph MainLoop["main.c Super Loop"]
    A1["Inicio"]
    A2["Inicialización de periféricos"]
    A3["ssd1306 Init"]
    A4["room control init"]
    A5["while 1"]
    A6["Leer temperatura"]
    A7["Actualizar room control"]
    A8["Procesar teclado"]
    A10["Actualizar display"]
    A11["Actualizar LEDs"]
    A12["Procesar comandos UART (command_parser)"]
  end

  subgraph RoomControl["room_control.c State Machine"]
    B1[["LOCKED"]]
    B2[["INPUT PASSWORD"]]
    B3[["UNLOCKED"]]
    B4[["ACCESS DENIED"]]
    B6["room control update"]
    B7["room control process key"]
    B8["room control set temperature"]
    B9["room control update display"]
    B10["room control update door"]
    B11["room control update fan"]
    B12["command_parser_process_debug / process_esp01"]
  end

  A1 --> A2 --> A3 --> A4 --> A5
  A5 --> A6 --> A7 --> B6
  A5 --> A8 --> B7
  A5 --> A10 --> B9
  A5 --> A11
  A5 --> A12 --> B12
  B6 --> B9 & B10 & B11 & B1 & B2 & B3 & B4
  B8 --> B11

  style RoomControl stroke:#00C853
  style MainLoop stroke:#2962FF

- **main.c:** Inicializa periféricos y ejecuta el super loop.

- **room_control.c:** Implementa la máquina de estados y lógica de acceso.

- **temperature_sensor.c:** Lee y convierte señales del sensor.

- **keypad.c:** Procesa entradas del usuario.

- **ssd1306.c:** Controla la pantalla OLED.

- **command_parser.c:** Procesa comandos por UART.

## **3. Protocolo de Comandos**
El sistema implementa un protocolo por UART para control remoto desde ESP-01:

- **GET_TEMP**
  Solicita temperatura actual.

  Respuesta: temperatura en °C.

- **GET_STATUS**
  Solicita estado del sistema (LOCKED/UNLOCKED, fan level).

  Respuesta: estado y nivel.

- **SET_PASS:1234**
  Cambia la clave de acceso.
  Formato: 4 caracteres.
  Si es válido, se guarda como nueva clave.

- **FORCE_FAN:2**
  Fuerza nivel del ventilador (0-3).
  Respuesta: OK si se aplica, ERROR si no es válido.