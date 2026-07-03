// ============================================================
//  PASTILLERO SEMIAUTOMÁTICO - ESP32 WROOM 38 PINES
//  Componentes: RTC DS1307, LCD I2C 16x2, 7x CNY70,
//               Buzzer, LED Rojo, LED Verde
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>   // LCD I2C
#include <RTClib.h>               // RTC DS1307 / DS3231

// ─────────────────────────────────────────────
//  PINES  (según tu esquemático)
// ─────────────────────────────────────────────

// Sensores CNY70 (entradas digitales, activo BAJO)
// Pines reasignados por corrección del PCB (de abajo hacia arriba en la bornera)
#define CNY_DOMINGO   34
#define CNY_LUNES     35
#define CNY_MARTES    32
#define CNY_MIERCOLES 33
#define CNY_JUEVES    25
#define CNY_VIERNES   26
#define CNY_SABADO    27

// Actuadores
#define LED_ROJO      8    // G8  → LED NO (rojo)
#define LED_VERDE     9    // G9  → LED SI (verde)  ← ajustá si tu cableado difiere
#define PIN_BUZZER    22   // G22 → BUZZER

// I2C  (ESP32 por defecto: SDA=21, SCL=22 — pero en tu esquema usás otros pines)
// Según el esquemático: RTC/LCD SCL → G23 (pin 3 J2),  RTC/LCD SDA → G21 (pin 6 J2)
// Si querés pines distintos descomentá y cambiá la línea Wire.begin() más abajo
// #define I2C_SDA 21
// #define I2C_SCL 23

// ─────────────────────────────────────────────
//  CONFIGURACIÓN DE ALARMA
//  Editá estas tres líneas para cambiar cuándo
//  tiene que sonar el buzzer cada día.
// ─────────────────────────────────────────────
#define HORA_ALARMA   8    // 8 AM  (formato 24 h)
#define MIN_ALARMA    0    // :00
#define DUR_ALARMA_MS 60000UL   // 60 segundos = 1 minuto

// ─────────────────────────────────────────────
//  OBJETOS
// ─────────────────────────────────────────────
RTC_DS1307 rtc;                           // cambiá a RTC_DS3231 si usás ese módulo
LiquidCrystal_I2C lcd(0x27, 16, 2);      // dirección I2C más común; probá 0x3F si no funciona

// ─────────────────────────────────────────────
//  VARIABLES GLOBALES
// ─────────────────────────────────────────────

// Días de la semana (0 = domingo en RTClib)
const char* diasSemana[] = {
  "DOMINGO ", "LUNES   ", "MARTES  ", "MIERCOLES",
  "JUEVES  ", "VIERNES ", "SABADO  "
};

// Pines de los 7 sensores en orden domingo→sabado
const int pinSensores[7] = {
  CNY_DOMINGO,  // índice 0
  CNY_LUNES,    // índice 1
  CNY_MARTES,
  CNY_MIERCOLES,
  CNY_JUEVES,
  CNY_VIERNES,
  CNY_SABADO    // índice 6
};

// Estado de pastilla por día (true = tomada esta semana)
bool pastillaTomada[7] = { false };

// Tiempo en que comenzó la alarma (0 = alarma inactiva)
unsigned long tiempoInicioAlarma = 0;
bool alarmaActiva = false;

// Para evitar re-disparar la alarma en el mismo minuto
int ultimoMinutoAlarma = -1;

// Para refrescar el LCD sólo cuando algo cambia
int ultimoMinuto = -1;

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // --- I2C ---
  // Si querés pines personalizados descomentá la siguiente línea:
  // Wire.begin(I2C_SDA, I2C_SCL);
  Wire.begin();   // usa GPIO21=SDA, GPIO22=SCL por defecto en ESP32

  // --- LCD ---
  lcd.init();
  lcd.backlight();          // ← ESTO ENCIENDE LA LUZ DE FONDO
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pastillero v1.0");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // --- RTC ---
  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("ERROR: RTC no");
    lcd.setCursor(0, 1);
    lcd.print("encontrado!");
    Serial.println("ERROR: RTC no encontrado");
    while (true) delay(1000);   // se queda acá hasta que conectes el RTC
  }

  // Si el RTC perdió la hora (primera vez o batería descargada), la seteamos
  if (!rtc.isrunning()) {
    Serial.println("RTC no estaba corriendo. Ajustando hora de compilacion.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // ↑ pone la hora en que compilaste el código. Después podés ajustarla
    //   descomentando la línea de abajo con tu hora real:
    // rtc.adjust(DateTime(2025, 6, 10, 8, 0, 0));  // año,mes,día,hora,min,seg
  }

  // --- Pines de sensores (INPUT con pull-up interno) ---
  for (int i = 0; i < 7; i++) {
    pinMode(pinSensores[i], INPUT_PULLUP);
  }
  // Nota: el CNY70 ya tiene su resistencia pull-down de 10k en el esquemático.
  // Con INPUT_PULLUP el pin lee HIGH cuando no hay reflejo y LOW cuando hay objeto.
  // Ajustá la lógica en leerSensor() si tu montaje es al revés.

  // --- LEDs y Buzzer ---
  pinMode(LED_ROJO,   OUTPUT);
  pinMode(LED_VERDE,  OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(LED_ROJO,   HIGH);   // LED rojo SIEMPRE encendido al arrancar
  digitalWrite(LED_VERDE,  LOW);
  digitalWrite(PIN_BUZZER, LOW);

  delay(2000);
  lcd.clear();
}

// ─────────────────────────────────────────────
//  LOOP PRINCIPAL
// ─────────────────────────────────────────────
void loop() {
  DateTime ahora = rtc.now();

  int diaActual  = ahora.dayOfTheWeek();  // 0=Dom … 6=Sáb
  int horaActual = ahora.hour();
  int minActual  = ahora.minute();

  // ── 1. Detectar si se abrió la pastillera hoy ──────────────
  verificarSensor(diaActual);

  // ── 2. Alarma diaria ───────────────────────────────────────
  manejarAlarma(horaActual, minActual);

  // ── 3. Actualizar LCD (sólo cuando el minuto cambia) ───────
  if (minActual != ultimoMinuto) {
    ultimoMinuto = minActual;
    actualizarLCD(ahora, diaActual);
  }

  // ── 4. LED rojo siempre encendido ──────────────────────────
  //    (ya se puso HIGH en setup; lo reafirmamos por si algo lo apagó)
  digitalWrite(LED_ROJO, HIGH);

  // ── 5. LED verde según si ya se tomó la pastilla hoy ───────
  digitalWrite(LED_VERDE, pastillaTomada[diaActual] ? HIGH : LOW);

  // ── 6. Reset semanal: a medianoche del lunes reiniciamos ───
  //    (opcional: limpia el registro de pastillas cada semana)
  if (diaActual == 1 && horaActual == 0 && minActual == 0) {
    for (int i = 0; i < 7; i++) pastillaTomada[i] = false;
  }

  delay(200);   // revisamos 5 veces por segundo, suficiente para esta aplicación
}

// ─────────────────────────────────────────────
//  FUNCIONES
// ─────────────────────────────────────────────

// Lee si el sensor del día indicado detecta que se abrió la tapa
// CNY70: emite IR y lee el reflejo. Tapa cerrada = reflejo alto → HIGH.
//        Tapa abierta  = sin reflejo               → LOW.
// (Si tu montaje es al revés, cambiá LOW por HIGH aquí abajo)
bool leerSensor(int dia) {
  return (digitalRead(pinSensores[dia]) == LOW);
}

// Verifica el sensor del día actual y actualiza la bandera
void verificarSensor(int dia) {
  if (!pastillaTomada[dia] && leerSensor(dia)) {
    pastillaTomada[dia] = true;
    Serial.print("Pastilla tomada el día: ");
    Serial.println(diasSemana[dia]);

    // Feedback inmediato en el LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  PASTILLA OK!  ");
    lcd.setCursor(0, 1);
    lcd.print("  ");
    lcd.print(diasSemana[dia]);
    lcd.print("  ");

    // Pitido corto de confirmación
    tone(PIN_BUZZER, 1000, 300);   // 1 kHz durante 300 ms
    delay(1500);
    lcd.clear();
    ultimoMinuto = -1;   // fuerza refresco del LCD en el próximo ciclo
  }
}

// Dispara la alarma a la hora configurada durante DUR_ALARMA_MS milisegundos
void manejarAlarma(int hora, int minuto) {
  // ¿Es la hora de la alarma y aún no disparó en este minuto?
  if (hora == HORA_ALARMA && minuto == MIN_ALARMA && minuto != ultimoMinutoAlarma) {
    alarmaActiva        = true;
    tiempoInicioAlarma  = millis();
    ultimoMinutoAlarma  = minuto;
    Serial.println("ALARMA ACTIVADA");
  }

  if (alarmaActiva) {
    unsigned long transcurrido = millis() - tiempoInicioAlarma;

    if (transcurrido < DUR_ALARMA_MS) {
      // Tono intermitente: 500 ms encendido, 500 ms apagado
      if ((transcurrido / 500) % 2 == 0) {
        tone(PIN_BUZZER, 880);    // nota A5
      } else {
        noTone(PIN_BUZZER);
      }
    } else {
      // Tiempo de alarma cumplido
      noTone(PIN_BUZZER);
      alarmaActiva = false;
      Serial.println("Alarma terminada");
    }
  }
}

// Dibuja hora, día y estado en el LCD
void actualizarLCD(DateTime ahora, int dia) {
  // ── Línea 0: DIA  HH:MM ──────────────────────────────────
  lcd.setCursor(0, 0);

  // Día abreviado (5 chars)
  String diaStr = String(diasSemana[dia]).substring(0, 5);
  // Rellenamos con espacios hasta 5 chars
  while (diaStr.length() < 5) diaStr += " ";
  lcd.print(diaStr);
  lcd.print("  ");

  // Hora HH:MM
  if (ahora.hour()   < 10) lcd.print("0");
  lcd.print(ahora.hour());
  lcd.print(":");
  if (ahora.minute() < 10) lcd.print("0");
  lcd.print(ahora.minute());

  lcd.print("   ");   // relleno derecho

  // ── Línea 1: estado de la pastilla ───────────────────────
  lcd.setCursor(0, 1);
  if (pastillaTomada[dia]) {
    lcd.print("Pastilla tomada ");
  } else {
    lcd.print("Pastilla pend.  ");
  }
}
