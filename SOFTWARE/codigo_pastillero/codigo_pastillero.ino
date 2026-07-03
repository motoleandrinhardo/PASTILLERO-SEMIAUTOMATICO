#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------ PINES ------------------

#define CNY_LUNES      36
#define CNY_MARTES     39
#define CNY_MIERCOLES  34
#define CNY_JUEVES     35
#define CNY_VIERNES    32
#define CNY_SABADO     33
#define CNY_DOMINGO    27

#define LED_VERDE      19
#define LED_ROJO       18
#define BUZZER         23

// ------------------ CONFIG ------------------

const int ALARM_HOUR = 10;
const int ALARM_MINUTE = 35;

const unsigned long BUZZER_TIME = 90000; // 1,5 min

// Ajustar luego con pruebas reales
const int UMBRAL_APERTURA = 300;

// ------------------ VARIABLES ------------------

bool pastillaTomada = false;
bool alarmaActiva = false;
unsigned long buzzerStart = 0;

int ultimoDiaMes = -1;

// -------------------------------------------------

int obtenerSensorDia(int diaSemana)
{
  switch(diaSemana)
  {
    case 1: return CNY_LUNES;
    case 2: return CNY_MARTES;
    case 3: return CNY_MIERCOLES;
    case 4: return CNY_JUEVES;
    case 5: return CNY_VIERNES;
    case 6: return CNY_SABADO;
    case 0: return CNY_DOMINGO;

    default: return CNY_LUNES;
  }
}

// -------------------------------------------------

String nombreDia(int diaSemana)
{
  switch(diaSemana)
  {
    case 1: return "LUN";
    case 2: return "MAR";
    case 3: return "MIE";
    case 4: return "JUE";
    case 5: return "VIE";
    case 6: return "SAB";
    case 0: return "DOM";
  }

  return "";
}

// -------------------------------------------------

void setup()
{
  Serial.begin(115200);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(CNY_LUNES, INPUT);
  pinMode(CNY_MARTES, INPUT);
  pinMode(CNY_MIERCOLES, INPUT);
  pinMode(CNY_JUEVES, INPUT);
  pinMode(CNY_VIERNES, INPUT);
  pinMode(CNY_SABADO, INPUT);
  pinMode(CNY_DOMINGO, INPUT);

  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, HIGH);
  digitalWrite(BUZZER, LOW);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  if(!rtc.begin())
  {
    lcd.clear();
    lcd.print("ERROR RTC");
    while(true);
  }

  DateTime now = rtc.now();
  ultimoDiaMes = now.day();
}

// -------------------------------------------------

void loop()
{
  DateTime now = rtc.now();

  // Reinicio diario
  if(now.day() != ultimoDiaMes)
  {
    ultimoDiaMes = now.day();

    pastillaTomada = false;
    alarmaActiva = false;

    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, HIGH);

    noTone(BUZZER);
  }

  // Mostrar hora
  lcd.setCursor(0,0);

  char buffer[17];

  sprintf(buffer,"%02d:%02d:%02d",
          now.hour(),
          now.minute(),
          now.second());

  lcd.print(buffer);
  lcd.print("   ");

  lcd.setCursor(0,1);

  String dia = nombreDia(now.dayOfTheWeek());

  if(pastillaTomada)
  {
    lcd.print(dia + " TOMADA   ");
  }
  else
  {
    lcd.print(dia + " PENDIENTE");
  }

  // Activación alarma
  if(!pastillaTomada &&
     now.hour() == ALARM_HOUR &&
     now.minute() == ALARM_MINUTE &&
     !alarmaActiva)
  {
      alarmaActiva = true;
      buzzerStart = millis();
  }

  // Buzzer
  if(alarmaActiva)
  {
      tone(BUZZER, 2500);

      if(millis() - buzzerStart >= BUZZER_TIME)
      {
          noTone(BUZZER);
          alarmaActiva = false;
      }
  }

  // Sensor del día actual
  int pinSensor = obtenerSensorDia(now.dayOfTheWeek());

  // Promedio de lecturas para máxima sensibilidad
  long suma = 0;

  for(int i=0;i<10;i++)
  {
      suma += analogRead(pinSensor);
      delay(2);
  }

  int lectura = suma / 10;

  Serial.print("Sensor ");
  Serial.print(nombreDia(now.dayOfTheWeek()));
  Serial.print(": ");
  Serial.println(lectura);

  // Detectar apertura
  if(!pastillaTomada && lectura < UMBRAL_APERTURA)
  {
      pastillaTomada = true;

      digitalWrite(LED_ROJO, LOW);
      digitalWrite(LED_VERDE, HIGH);

      noTone(BUZZER);

      alarmaActiva = false;

      Serial.println("PASTILLA TOMADA");
  }

  delay(100);
}
