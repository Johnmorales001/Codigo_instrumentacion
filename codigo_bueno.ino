#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 64
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;

// Pines (ajustados para ESP32)
const int lm35Pin = 34;             // ADC válido para ESP32
const int relePin = 2;
const int releAmarillo = 4;
const int releRojo = 5;
const int resetPin = 18;
const int historialPin = 19;

// Variables
float temperatura = 0;
bool focoEncendido = false;
bool habilitadoParaEncender = true;
bool alarma1Activa = false;
bool alarma2Activa = false;
int turnoAlarma = 1;

bool mostrarReset = false;
unsigned long tiempoReset = 0;

// Historial con lista enlazada
struct Evento {
  DateTime fechaHora;
  int tipoAlarma; // 1 = Alarma 1, 2 = Alarma 2, 3 = Foco encendido
  Evento* siguiente;
};
Evento* historialInicio = nullptr;
int totalEventos = 0;

void guardarEvento(DateTime fechaHora, int tipo) {
  Evento* nuevo = new Evento{fechaHora, tipo, nullptr};
  totalEventos++;

  if (historialInicio == nullptr) {
    historialInicio = nuevo;
  } else {
    Evento* actual = historialInicio;
    while (actual->siguiente != nullptr) {
      actual = actual->siguiente;
    }
    actual->siguiente = nuevo;
  }
}

void setup() {
  pinMode(relePin, OUTPUT);
  pinMode(releAmarillo, OUTPUT);
  pinMode(releRojo, OUTPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(historialPin, INPUT_PULLUP);

  digitalWrite(relePin, HIGH);
  digitalWrite(releAmarillo, HIGH);
  digitalWrite(releRojo, HIGH);

  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se encontró la pantalla OLED");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!rtc.begin()) {
    Serial.println("No se encontró el DS3231");
    while (true);
  }

  //rtc.adjust(DateTime(2025, 6, 29, 11, 55, 0)); // Solo una vez
}

void loop() {
  DateTime now = rtc.now();
  int lectura = analogRead(lm35Pin);
  temperatura = (lectura * 3.3 * 100.0) / 4095.0;

  // Foco verde con histéresis
  if (!alarma1Activa && !alarma2Activa) {
    if (habilitadoParaEncender && temperatura >= 40.0 && temperatura < 50.0) {
      if (!focoEncendido) {
        focoEncendido = true;
        digitalWrite(relePin, LOW);
        guardarEvento(now, 3); // Solo al encender
      }
    }
    if (focoEncendido && temperatura >= 50.0) {
      focoEncendido = false;
      habilitadoParaEncender = false;
      digitalWrite(relePin, HIGH);
    }
    if (temperatura < 40.0) {
      habilitadoParaEncender = true;
    }
  } else {
    digitalWrite(relePin, HIGH);
  }

  // Activación de alarmas
  if (!alarma1Activa && !alarma2Activa && temperatura > 65.0 && turnoAlarma == 1) {
    alarma1Activa = true;
    digitalWrite(releAmarillo, LOW);
    guardarEvento(now, 1);
  }

  if (!alarma2Activa && !alarma1Activa && temperatura > 75.0 && turnoAlarma == 2) {
    alarma2Activa = true;
    digitalWrite(releRojo, LOW);
    guardarEvento(now, 2);
  }

  // Botón RESET
  if (digitalRead(resetPin) == LOW) {
    delay(200);
    mostrarReset = true;
    tiempoReset = millis();
    if (alarma1Activa) {
      alarma1Activa = false;
      digitalWrite(releAmarillo, HIGH);
      turnoAlarma = 2;
    } else if (alarma2Activa) {
      alarma2Activa = false;
      digitalWrite(releRojo, HIGH);
      turnoAlarma = 1;
    }
    while (digitalRead(resetPin) == LOW);
  }

  // Mostrar historial completo (paginado de 5 en 5)
  if (digitalRead(historialPin) == LOW && historialInicio != nullptr) {
    Evento* actual = historialInicio;
    int pagina = 1;
    while (actual != nullptr) {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Historial (Pag ");
      display.print(pagina++);
      display.println("):");

      for (int i = 0; i < 5 && actual != nullptr; i++) {
        display.setCursor(0, 10 + i * 10);
        if (actual->tipoAlarma == 1) display.print("A1 ");
        else if (actual->tipoAlarma == 2) display.print("A2 ");
        else if (actual->tipoAlarma == 3) display.print("Foco ");

        DateTime dt = actual->fechaHora;
        if (dt.hour() < 10) display.print('0');
        display.print(dt.hour()); display.print(':');
        if (dt.minute() < 10) display.print('0');
        display.print(dt.minute()); display.print(' ');
        if (dt.day() < 10) display.print('0');
        display.print(dt.day()); display.print('-');
        if (dt.month() < 10) display.print('0');
        display.print(dt.month());

        actual = actual->siguiente;
      }

      display.display();
      delay(2000);
    }
    return;
  }

  // Mostrar datos en OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(temperatura, 1);
  display.println(" C");

  int hour = now.hour();
  bool isPM = false;
  if (hour >= 12) {
    isPM = true;
    if (hour > 12) hour -= 12;
  }
  if (hour == 0) hour = 12;

  display.setCursor(0, 10);
  display.print("Hora: ");
  if (hour < 10) display.print('0');
  display.print(hour); display.print(':');
  if (now.minute() < 10) display.print('0');
  display.print(now.minute()); display.print(':');
  if (now.second() < 10) display.print('0');
  display.print(now.second());
  display.print(isPM ? " PM" : " AM");

  display.setCursor(0, 20);
  display.print("Fecha: ");
  if (now.day() < 10) display.print('0');
  display.print(now.day()); display.print('-');
  if (now.month() < 10) display.print('0');
  display.print(now.month()); display.print('-');
  display.print(now.year());

  display.setCursor(0, 40);
  if (mostrarReset) {
    display.println("Boton RESET presionado");
    if (millis() - tiempoReset > 2000) mostrarReset = false;
  } else if (alarma1Activa) {
    display.println("ALERTA: ALARMA 1");
  } else if (alarma2Activa) {
    display.println("ALERTA: ALARMA 2");
  } else if (focoEncendido) {
    display.println("Estado: Foco Verde");
  } else {
    display.println("Estado: Normal");
  }

  display.display();
  delay(1000);
}
