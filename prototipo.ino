#include <ArduinoJson.h>
#include <SoftwareSerial.h>

int pinoLedVermelho = 4;
int pinoLedVerde    = 5;
int pinoBuzzer      = 6;
int pinoSensorChama = 7;
int pinoSensorMQ2   = A0;

const int leituraSensor = 300;

int fireAlertCounter = 0;
int fireAlertSent    = 0;

int gasAlertCounter = 0;
int gasAlertSent    = 0;

int alertResetCounter = 0;
const int alertResetTime = 50000;

const String serverAddress = "192.168.11.4";

const int maxAlertSentCounter = 3;

String rede     = "Casa";
String senha    = "11122593";
String resposta = "";
String json     = "";

StaticJsonDocument<200> doc;

SoftwareSerial ESP_Serial(10, 11);

void setup()
{
  pinMode(pinoSensorChama, INPUT );
  pinMode(pinoSensorMQ2  , INPUT );
  pinMode(pinoLedVermelho, OUTPUT);
  pinMode(pinoLedVerde   , OUTPUT);
  pinMode(pinoBuzzer     , OUTPUT);

  digitalWrite(pinoLedVermelho, LOW );
  digitalWrite(pinoLedVerde   , HIGH);

  Serial.begin(9600);
  ESP_Serial.begin(9600);

  Serial.println("Inicializando...");
  delay(1000);

  Serial.println("Chamando atencao do modulo com AT...");
  sendCommand("AT");
  readResponse(1000);

  Serial.println("Mudando o modo com CWMODE=1...");
  sendCommand("AT+CWMODE=1");
  readResponse(1000);

  Serial.println("Verificando redes WiFi disponíveis com CWLAP...");
  sendCommand("AT+CWLAP");
  readResponse(15000);

  Serial.println("Conectando a rede...");
  String CWJAP = "\"AT+CWJAP=\"";
  CWJAP += rede;
  CWJAP += "\",\"";
  CWJAP += senha;
  CWJAP += "\"";
  sendCommand(CWJAP);
  readResponse(15000);

  delay(2000);

  if (resposta.indexOf("OK") == -1)
  {
    Serial.println("Atencao: Nao foi possivel conectar a rede WiFi.");
    Serial.println("Verifique se o nome da rede e senha foram preenchidos corretamente no codigo e tente novamente.");
  }
  else
  {
    Serial.println("Sucesso! Conectado a rede WiFi.");
  }
}

void loop()
{
  int sensorMQ2 = analogRead(pinoSensorMQ2);
  int sensorChamaDigital = digitalRead(pinoSensorChama);

  if (sensorChamaDigital != 1)
  {
    digitalWrite(pinoLedVermelho, HIGH);
    digitalWrite(pinoLedVerde, LOW);
    tone(pinoBuzzer, 1000, 100);
    delay(125);
    fireAlertCounter += 1;

    if ((fireAlertCounter >= 10) && (fireAlertSent < maxAlertSentCounter))
    {
      sendAlertPostRequest(1);
      fireAlertSent += 1;

      if (fireAlertSent == maxAlertSentCounter)
        alertResetCounter = millis();
    }

    if ((fireAlertSent == maxAlertSentCounter) && ((alertResetCounter + alertResetTime) <= millis()))
    {
      fireAlertSent    = 0;
      fireAlertCounter = 0;
    }
  }
  else if (sensorMQ2 > leituraSensor)
  {
    digitalWrite(pinoLedVermelho, HIGH);
    digitalWrite(pinoLedVerde, LOW);
    tone(pinoBuzzer, 1000, 100);
    delay(125);
    gasAlertCounter += 1;

    if ((gasAlertCounter >= 10) && (gasAlertSent < maxAlertSentCounter))
    {
      sendAlertPostRequest(2);
      gasAlertSent += 1;

      if (gasAlertSent == maxAlertSentCounter)
        alertResetCounter = millis();
    }
  }
  else
  {
    digitalWrite(pinoLedVermelho, LOW);
    digitalWrite(pinoLedVerde, HIGH);
    noTone(pinoBuzzer);

    fireAlertCounter = 0;
    gasAlertCounter  = 0;
  }

  delay(50);
}

void sendCommand(String cmd)
{
  ESP_Serial.println(cmd);
}

void readResponse(unsigned int timeout)
{
  unsigned long timeIn = millis();
  resposta = "";

  while (timeIn + timeout > millis())
  {
    if (ESP_Serial.available())
    {
      char c = ESP_Serial.read();
      resposta += c;
    }
  }

  Serial.println(resposta);
}

void readResponseWithJson(unsigned int timeout)
{
  unsigned long timeIn = millis();
  resposta = "";
  json = "";
  bool seenBrace = false;

  while (timeIn + timeout > millis())
  {
    if (ESP_Serial.available())
    {
      char c = ESP_Serial.read();

      if (c == '[')
        seenBrace = true;
      
      if (seenBrace) json += c;
      else           resposta += c;
    }
  }

  Serial.println(json);
  Serial.println(resposta);
  DeserializationError error = deserializeJson(doc, json);
}

void sendAlertPostRequest(int alertCode)
{
  sendCommand("AT+CIPMUX=1");
  readResponse(1000);

  sendCommand("AT+CIPSTART=4,\"TCP\",\""+ serverAddress +"\",3000");
  readResponse(2500);

  String request = "POST /send_alert/" + String(alertCode) +"\n";
  sendCommand("AT+CIPSEND=4,"+ String(request.length() + 4));
  readResponse(2500);

  sendCommand(request);
  readResponse(10000);

  ESP_Serial.println();
  // delay(1000);

  // sendCommand("AT+CIPCLOSE=4");
  readResponse(5000);
}

void sendGetRequest()
{
  sendCommand("AT+CIPMUX=1");
  readResponse(1000);

  sendCommand("AT+CIPSTART=3,\"TCP\",\""+ serverAddress +"\",3000");
  readResponse(2500);

  String request = "GET /users/";

  sendCommand("AT+CIPSEND=3,"+ String(request.length() + 4));
  readResponse(2500);

  sendCommand(request);
  readResponseWithJson(30000);

  String name = doc[0]["name"];
  Serial.println(name);

  ESP_Serial.println();
  readResponse(10000);
  // delay(1000);

  // sendCommand("AT+CIPCLOSE=4");
  // readResponse(1000);
}
