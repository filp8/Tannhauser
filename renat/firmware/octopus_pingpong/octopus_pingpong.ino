/*
  octopus_pingpong - BTT Octopus V1.1 (STM32F446ZE, quarzo 12 MHz)
  Test di comunicazione USB seriale con il Raspberry Pi.

  Protocollo: righe di testo terminate da '\n'
    PING  -> PONG
    INFO  -> octopus_pingpong <versione> clock=<Hz>
    altro -> ERR <comando>

  LED STATUS (PA13): 0,5 s acceso / 0,5 s spento (10 cicli = 10 s),
  per vedere che il firmware gira e che il clock e' corretto.

  Impostazioni Arduino IDE (menu Strumenti):
    Scheda:            Generic STM32F4 series
    Board part number: Generic F446ZETx
    USB support:       CDC (generic 'Serial' supersede U(S)ART)

  Nella stessa cartella deve esserci build_opt.h con: -DHSE_VALUE=12000000U
*/

#if HSE_VALUE != 12000000U
#error "HSE_VALUE non e' 12 MHz: metti build_opt.h nella cartella dello sketch"
#endif

#if !defined(USBCON) || !defined(USBD_USE_CDC)
#error "In Strumenti scegli: USB support -> CDC (generic 'Serial' supersede U(S)ART)"
#endif

// ---------------------------------------------------------------------------
// Clock: quarzo esterno 12 MHz -> SYSCLK 168 MHz, 48 MHz esatti per la USB
// ---------------------------------------------------------------------------
extern "C" void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 6;              // 12 MHz / 6   = 2 MHz
  RCC_OscInitStruct.PLL.PLLN = 168;            // 2 MHz * 168  = 336 MHz (VCO)
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;  // 336 / 2      = 168 MHz (SYSCLK)
  RCC_OscInitStruct.PLL.PLLQ = 7;              // 336 / 7      = 48 MHz (USB)
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  // HCLK 168 MHz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;   // APB1  42 MHz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;   // APB2  84 MHz
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48CLKSOURCE_PLLQ;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
    Error_Handler();
  }

  // Valore esplicito, cosi' millis()/delay() sono giusti in ogni caso
  SystemCoreClock = 168000000UL;
  HAL_InitTick(TICK_INT_PRIORITY);
}

// ---------------------------------------------------------------------------
const uint32_t LED_STATUS = PA13;
const char FW_VERSION[] = "octopus_pingpong 1.0";

char rxBuf[64];
size_t rxLen = 0;
bool rxOverflow = false;

void handleCommand(const char *cmd) {
  if (strcmp(cmd, "PING") == 0) {
    Serial.println("PONG");
  } else if (strcmp(cmd, "INFO") == 0) {
    Serial.print(FW_VERSION);
    Serial.print(" clock=");
    Serial.println(SystemCoreClock);
  } else {
    Serial.print("ERR ");
    Serial.println(cmd);
  }
}

// Lettura NON bloccante: prende i caratteri disponibili e torna subito,
// cosi' in futuro il loop puo' muovere i motori senza fermarsi ad aspettare.
void readSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxOverflow) {
        Serial.println("ERR TOO_LONG");
      } else if (rxLen > 0) {
        handleCommand(rxBuf);
      }
      rxLen = 0;
      rxOverflow = false;
    } else if (rxLen < sizeof(rxBuf) - 1) {
      rxBuf[rxLen++] = c;
    } else {
      rxOverflow = true;
    }
  }
}

void updateLed() {
  static uint32_t last = 0;
  static bool on = false;
  uint32_t now = millis();
  if (now - last >= 500) {
    last = now;
    on = !on;
    digitalWrite(LED_STATUS, on ? HIGH : LOW);
  }
}

void setup() {
  pinMode(LED_STATUS, OUTPUT);
  Serial.begin(115200);  // su USB il baud rate non conta, ma va chiamato
}

void loop() {
  readSerial();
  updateLed();
}
