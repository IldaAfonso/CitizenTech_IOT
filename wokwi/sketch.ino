/*
  CitizenTech - Idoso em casa com mais seguranca
  Prototipo IoT (Arduino Uno) - simulado no Wokwi

  Sensores/atuadores:
    D2  botao de emergencia (INPUT_PULLUP)
    D3  sensor de movimento PIR
    D4  sensor da caixa de remedios (botao: pressionado = caixa aberta)
    D8  LED vermelho (emergencia)
    D7  LED amarelo (horario do remedio)
    D6  LED verde (sistema ok / evento enviado)
    D9  buzzer

  Cada evento sai pela Serial como uma linha JSON. No projeto real, um modulo
  ESP32 (gateway) leria essa linha e enviaria por HTTP para a API.
  Tipos de evento (iguais aos do banco de dados):
    BOTAO_EMERGENCIA, MEDICAMENTO_TOMADO, MEDICAMENTO_NAO_TOMADO, SEM_MOVIMENTO
*/

const char* CODIGO_DISPOSITIVO = "ESP32-0001";

const int PIN_BOTAO   = 2;
const int PIN_PIR     = 3;
const int PIN_CAIXA   = 4;
const int PIN_LED_VM  = 8;
const int PIN_LED_AM  = 7;
const int PIN_LED_VD  = 6;
const int PIN_BUZZER  = 9;

// Tempos DE DEMONSTRACAO (em segundos). No uso real: 6 h sem movimento,
// remedio a cada 8 h com janela de 30 min.
const unsigned long SEM_MOVIMENTO_S   = 20;  // real: 6 * 3600
const unsigned long INTERVALO_REMEDIO = 30;  // real: 8 * 3600
const unsigned long JANELA_REMEDIO_S  = 15;  // real: 30 * 60

unsigned long ultimoMovimento = 0;
unsigned long ultimoRemedio = 0;
unsigned long inicioJanela = 0;
bool aguardandoRemedio = false;
bool alertaSemMovimentoEnviado = false;

bool botaoAnterior = HIGH;
bool caixaAnterior = HIGH;
unsigned long ultimoBotaoMs = 0;
unsigned long ultimoCaixaMs = 0;
unsigned long ledVerdeAteMs = 0;

void enviarEvento(const char* tipo) {
  Serial.print("{\"dispositivo\":\"");
  Serial.print(CODIGO_DISPOSITIVO);
  Serial.print("\",\"tipo\":\"");
  Serial.print(tipo);
  Serial.print("\",\"t\":");
  Serial.print(millis() / 1000);
  Serial.println("}");
  ledVerdeAteMs = millis() + 500;  // pisca o verde: evento enviado
}

void bip(int vezes, int duracao) {
  for (int i = 0; i < vezes; i++) {
    tone(PIN_BUZZER, 1000);
    delay(duracao);
    noTone(PIN_BUZZER);
    delay(duracao);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_BOTAO, INPUT_PULLUP);
  pinMode(PIN_CAIXA, INPUT_PULLUP);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LED_VM, OUTPUT);
  pinMode(PIN_LED_AM, OUTPUT);
  pinMode(PIN_LED_VD, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  ultimoMovimento = millis();
  ultimoRemedio = millis();
  Serial.println("CitizenTech: dispositivo iniciado");
}

void loop() {
  unsigned long agora = millis();

  // 1) Botao de emergencia (com debounce de 50 ms)
  bool botao = digitalRead(PIN_BOTAO);
  if (botao == LOW && botaoAnterior == HIGH && agora - ultimoBotaoMs > 50) {
    ultimoBotaoMs = agora;
    digitalWrite(PIN_LED_VM, HIGH);
    enviarEvento("BOTAO_EMERGENCIA");
    bip(3, 150);
    digitalWrite(PIN_LED_VM, LOW);
  }
  botaoAnterior = botao;

  // 2) Sensor de movimento: qualquer movimento zera o contador
  if (digitalRead(PIN_PIR) == HIGH) {
    ultimoMovimento = agora;
    alertaSemMovimentoEnviado = false;
  }
  if (!alertaSemMovimentoEnviado && (agora - ultimoMovimento) / 1000 >= SEM_MOVIMENTO_S) {
    enviarEvento("SEM_MOVIMENTO");
    alertaSemMovimentoEnviado = true;
    bip(2, 300);
  }

  // 3) Caixa de remedios: chega o horario -> acende o amarelo e abre a janela
  if (!aguardandoRemedio && (agora - ultimoRemedio) / 1000 >= INTERVALO_REMEDIO) {
    aguardandoRemedio = true;
    inicioJanela = agora;
    digitalWrite(PIN_LED_AM, HIGH);
    bip(1, 200);
  }

  bool caixa = digitalRead(PIN_CAIXA);
  if (caixa == LOW && caixaAnterior == HIGH && agora - ultimoCaixaMs > 50) {
    ultimoCaixaMs = agora;
    if (aguardandoRemedio) {
      enviarEvento("MEDICAMENTO_TOMADO");
      aguardandoRemedio = false;
      ultimoRemedio = agora;
      digitalWrite(PIN_LED_AM, LOW);
    }
  }
  caixaAnterior = caixa;

  if (aguardandoRemedio && (agora - inicioJanela) / 1000 >= JANELA_REMEDIO_S) {
    enviarEvento("MEDICAMENTO_NAO_TOMADO");
    aguardandoRemedio = false;
    ultimoRemedio = agora;
    digitalWrite(PIN_LED_AM, LOW);
    bip(2, 300);
  }

  // LED verde: aceso = sistema ligado; apaga rapido quando envia evento
  digitalWrite(PIN_LED_VD, agora < ledVerdeAteMs ? LOW : HIGH);
}
