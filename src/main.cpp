#include <HTTPClient.h>
#include "ArduinoJson.h"
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "RESTClient.h"
#include "DHTesp.h"

DHTesp dht;
int dhtPin = GPIO_NUM_2;

int cont;

uint8_t pin_led_rojo = GPIO_NUM_4;
uint8_t pin_led_verde = GPIO_NUM_5;

// Replace 0 by ID of this current device
const int DEVICE_ID = 69;
const int GROUP_ID = 1;

// Replace 0.0.0.0 by your server local IP (ipconfig [windows] or ifconfig [Linux o MacOS] gets IP assigned to your PC)
String serverName = "http://192.168.1.72:8080/";
HTTPClient http;

// Replace WifiName and WifiPassword by your WiFi credentials
#define STASSID "MOVISTAR_20FB"       //"Your_Wifi_SSID"
#define STAPSK "9957F92A40EAA826499A" //"Your_Wifi_PASSWORD"

// MQTT configuration
WiFiClient espClient;
PubSubClient client(espClient);

// Server IP, where de MQTT broker is deployed
const char *MQTT_BROKER_ADRESS = "192.168.1.72";
const uint16_t MQTT_PORT = 1883;

// Name for this MQTT client
const char *MQTT_CLIENT_NAME = "ESP32Client";

String serializeActuador(int idActuador, int idPlaca, float valor, int tipoActuador, long tiempo, int idGroup);
// callback a ejecutar cuando se recibe un mensaje
// en este ejemplo, muestra por serial el mensaje recibido
void OnMqttReceived(char *topic, byte *payload, unsigned int length)
{
  String content = "";
  for (size_t i = 0; i < length; i++)
  {
    content.concat((char)payload[i]);
  }

  int estado = content.toInt();
  if (estado == 0)
  {
    digitalWrite(pin_led_rojo, HIGH);
    digitalWrite(pin_led_verde, LOW);
    String valores_actuador = serializeActuador(1, DEVICE_ID, estado, 1, millis(), GROUP_ID);
    String serverPath = serverName + "api/actuadores";
    http.begin(serverPath.c_str());
    http.POST(valores_actuador);
  }
  else if (estado == 1)
  {
    digitalWrite(pin_led_rojo, LOW);
    digitalWrite(pin_led_verde, HIGH);
    String valores_actuador = serializeActuador(1, DEVICE_ID, estado, 1, millis(), GROUP_ID);
    String serverPath = serverName + "api/actuadores";
    http.begin(serverPath.c_str());
    http.POST(valores_actuador);
  }
}

// inicia la comunicacion MQTT
// inicia establece el servidor y el callback al recibir un mensaje
void InitMqtt()
{
  client.setServer(MQTT_BROKER_ADRESS, MQTT_PORT);
  client.setCallback(OnMqttReceived);
}

String response;

String serializePlaca(int idPlaca, int idGroup)
{
  DynamicJsonDocument doc(ESP.getMaxAllocHeap());

  doc["idPlaca"] = idPlaca;
  doc["idGroup"] = idGroup;

  String output;
  serializeJson(doc, output);
  return output;
}

// Setup
void setup()
{
  cont = 0;

  pinMode(pin_led_rojo, OUTPUT);
  pinMode(pin_led_verde, OUTPUT);

  Serial.begin(9600);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(STASSID);

  /* Explicitly set the ESP32 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  InitMqtt();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  dht.setup(dhtPin, DHTesp::DHT11);

  String device_body = serializePlaca(DEVICE_ID, GROUP_ID);
  String serverPath = serverName + "api/placas";
  http.begin(serverPath.c_str());
  (http.POST(device_body));
  Serial.println(device_body);

  Serial.println("Setup!");
}

// conecta o reconecta al MQTT
// consigue conectar -> suscribe a topic y publica un mensaje
// no -> espera 5 segundos
void ConnectMqtt()
{
  Serial.print("Starting MQTT connection...");
  if (client.connect(MQTT_CLIENT_NAME))
  {
    client.publish("Bienvenida", MQTT_CLIENT_NAME);
    client.subscribe("Group_1");
  }
  else
  {
    Serial.print("Failed MQTT connection, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");

    delay(5000);
  }
}
// gestiona la comunicación MQTT
// comprueba que el cliente está conectado
// no -> intenta reconectar
// si -> llama al MQTT loop
void HandleMqtt()
{
  if (!client.connected())
  {
    ConnectMqtt();
  }
  client.loop();
}

String serializeSensor(int idSensor, int idPlaca, float valor1, float valor2, int tipoSensor, long tiempo, int idGroup)
{
  // StaticJsonObject allocates memory on the stack, it can be
  // replaced by DynamicJsonDocument which allocates in the heap.
  //
  DynamicJsonDocument doc(ESP.getMaxAllocHeap());

  // Add values in the document
  //
  doc["idSensor"] = idSensor;
  doc["idPlaca"] = idPlaca;
  doc["valor1"] = valor1;
  doc["valor2"] = valor2;
  doc["tipoSensor"] = tipoSensor;
  doc["tiempo"] = tiempo;
  doc["idGroup"] = idGroup;

  // Generate the minified JSON and send it to the Serial port.
  //
  String output;
  serializeJson(doc, output);
  // Serial.println(output);

  return output;
}
String serializeActuador(int idActuador, int idPlaca, float valor, int tipoActuador, long tiempo, int idGroup)
{
  DynamicJsonDocument doc(ESP.getMaxAllocHeap());

  doc["idActuador"] = idActuador;
  doc["idPlaca"] = idPlaca;
  doc["valor"] = valor;
  doc["tipoActuador"] = tipoActuador;
  doc["tiempo"] = tiempo;
  doc["idGroup"] = idGroup;

  String output;
  serializeJson(doc, output);
  return output;
}

void deserializePlaca(int httpResponseCode)
{
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String responseJson = http.getString();
    DynamicJsonDocument doc(ESP.getMaxAllocHeap());

    DeserializationError error = deserializeJson(doc, responseJson);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonObject placa : array)
    {
      int idValue = placa["idValue"];
      int idPlaca = placa["idPlaca"];
      int idGroup = placa["idGroup"];

      Serial.println((
                         "Placa: [idValue: " + String(idValue) +
                         ", idPlaca: " + String(idPlaca) +
                         ", idGroup: " + String(idGroup) +
                         "]")
                         .c_str());
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
}
void deserializeSensor(int httpResponseCode)
{

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String responseJson = http.getString();
    // allocate the memory for the document
    DynamicJsonDocument doc(ESP.getMaxAllocHeap());

    // parse a JSON array
    DeserializationError error = deserializeJson(doc, responseJson);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // extract the values
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject sensor : array)
    {
      int idValue = sensor["idValue"];
      int idSensor = sensor["idSensor"];
      int idPlaca = sensor["idPlaca"];
      double valor1 = sensor["valor1"];
      double valor2 = sensor["valor2"];
      int tipoSensor = sensor["tipoSensor"];
      long tiempo = sensor["tiempo"];
      int idGroup = sensor["idGroup"];

      Serial.println((
                         "Sensor: [idValue: " + String(idValue) +
                         ", idSensor: " + String(idSensor) +
                         ", idPlaca: " + String(idPlaca) +
                         ", valor1: " + String(valor1) +
                         ", valor2: " + String(valor2) +
                         ", tipoSensor: " + String(tipoSensor) +
                         ", tiempo: " + String(tiempo) +
                         ", idGroup: " + String(idGroup) +
                         "]")
                         .c_str());
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
}
void deserializeActuador(int httpResponseCode)
{

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String responseJson = http.getString();
    // allocate the memory for the document
    DynamicJsonDocument doc(ESP.getMaxAllocHeap());

    // parse a JSON array
    DeserializationError error = deserializeJson(doc, responseJson);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    // extract the values
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject sensor : array)
    {
      int idValue = sensor["idValue"];
      int idActuador = sensor["idActuador"];
      int idPlaca = sensor["idPlaca"];
      double valor = sensor["valor"];
      int tipoActuador = sensor["tipoActuador"];
      long tiempo = sensor["tiempo"];
      int idGroup = sensor["idGroup"];

      Serial.println((
                         "Actuador: [idValue: " + String(idValue) +
                         ", idActuador: " + String(idActuador) +
                         ", idPlaca: " + String(idPlaca) +
                         ", valor: " + String(valor) +
                         ", tipoActuador: " + String(tipoActuador) +
                         ", tiempo: " + String(tiempo) +
                         ", idGroup: " + String(idGroup) +
                         "]")
                         .c_str());
    }
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
}

void GET_tests()
{
  Serial.println("Test GET Placa por ID");
  String serverPath = serverName + "api/placas/" + String(DEVICE_ID);
  http.begin(serverPath.c_str());
  deserializePlaca(http.GET());

  delay(1000);

  Serial.println("Test GET Sensores de Placa por ID");
  serverPath = serverName + "api/placas/" + String(DEVICE_ID) + "/sensores";
  http.begin(serverPath.c_str());
  deserializeSensor(http.GET());

  delay(1000);

  Serial.println("Test GET Actuadores de Placa por ID");
  serverPath = serverName + "api/placas/" + String(DEVICE_ID) + "/actuadores";
  http.begin(serverPath.c_str());
  deserializeActuador(http.GET());
}

void POST_tests()
{
  String sensor_value_body = serializeSensor(3, DEVICE_ID, random(-500, 5000) / 100, random(0, 10000) / 100, 1, millis(), GROUP_ID);
  Serial.println("Test POST Sensor con valores");
  String serverPath = serverName + "api/sensores";
  http.begin(serverPath.c_str());
  http.POST(sensor_value_body);

  String actuator_states_body = serializeActuador(3, DEVICE_ID, random(-500, 5000) / 100, 1, millis(), GROUP_ID);
  Serial.println("Test POST Actuador con valores");
  serverPath = serverName + "api/actuadores";
  http.begin(serverPath.c_str());
  http.POST(actuator_states_body);

  String device_body = serializePlaca(DEVICE_ID, GROUP_ID);
  Serial.println("Test POST Placa con valores");
  serverPath = serverName + "api/placas";
  http.begin(serverPath.c_str());
  http.POST(device_body);
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Conectando al servidor MQTT");
    if (client.connect(MQTT_CLIENT_NAME))
    {
      Serial.println("Conectado");
      client.publish("Bienvenida", MQTT_CLIENT_NAME);
      client.subscribe("Group_1");
    }
    else
    {
      Serial.print("Error, rc=");
      Serial.print(client.state());
      Serial.println(" Reintentando en 5 segundos");
      delay(5000);
    }
  }
}

// Run the tests!
void loop()
{
  /*
    Serial.println(millis());

    GET_tests();
    delay(5000);

    POST_tests();
    delay(5000);

    HandleMqtt();
    delay(5000);

    client.loop();
    delay(5000);
    snprintf(msg, 75, "Son las %ld", millis());
    client.publish("topic_2", msg);
  */

  HandleMqtt();

  if (cont == 10)
  {
    TempAndHumidity tem_hum = dht.getTempAndHumidity();
    // Serial.println(("Temperatura: " + String(tem_hum.temperature) + "; Humedad: " + String(tem_hum.humidity)).c_str());
    String valores_sensor = serializeSensor(1, DEVICE_ID, tem_hum.temperature, tem_hum.humidity, 1, millis(), GROUP_ID);
    String serverPath = serverName + "api/sensores";
    http.begin(serverPath.c_str());
    http.POST(valores_sensor);

    cont = 0;
  }

  cont++;
  delay(1000);
}