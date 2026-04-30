#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>

// ---------------------------------------------------------
// CONFIGURACIÓN DE RED WIFI (INTERNET)
// ---------------------------------------------------------
// ⚠️ REEMPLAZA CON LOS DATOS DE TU ROUTER PARA TENER INTERNET ⚠️
const char* ssid = "Escuela Hogar Del Niño"; 
const char* password = "HDN@escuela26";

// ---------------------------------------------------------
// CONFIGURACIÓN DEL SENSOR LOCAL (DHT11)
// ---------------------------------------------------------
#define DHTPIN1 4     
#define DHTTYPE1 DHT11
DHT dht1(DHTPIN1, DHTTYPE1);

// Variables para almacenar las lecturas
float current_t1 = NAN;
float current_h1 = NAN;
unsigned long lastReadTime1 = 0;

// ---------------------------------------------------------
// CONFIGURACIÓN DE THINGSPEAK
// ---------------------------------------------------------
const char* thingspeak_api_key = "0NNDZJ5SIJBV8GGY";
unsigned long lastThingSpeakTime = 0;
const unsigned long thingspeakInterval = 15000; // Enviar cada 15 segundos (límite de ThingSpeak gratuito)

// Servidor web local de respaldo
WebServer server(80);

// ---------------------------------------------------------
// FUNCIÓN PARA ENVIAR DATOS A THINGSPEAK
// ---------------------------------------------------------
void sendToThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.thingspeak.com/update?api_key=" + String(thingspeak_api_key);
    
    // Validar si el sensor está fallando
    if (isnan(current_t1) || isnan(current_h1)) {
      Serial.println("⚠️ ADVERTENCIA: El sensor DHT11 está fallando (Lee NAN). Revisa los cables en el Pin 4.");
    } else {
      url += "&field1=" + String(current_t1, 1);
      url += "&field2=" + String(current_h1, 1);
    }

    Serial.print("Enviando a ThingSpeak: ");
    Serial.println(url);

    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Seguir redirecciones si ThingSpeak devuelve 302
    
    int httpCode = http.GET();
    String responseBody = http.getString();
    
    if (httpCode > 0) {
      Serial.printf("ThingSpeak Respondió: %d, Cuerpo: %s\n", httpCode, responseBody.c_str());
      if (responseBody == "0") {
        Serial.println("  -> ¡Atención! ThingSpeak devolvió 0. Esto significa que los datos NO se guardaron. Puede ser por el límite de 15s o una API Key incorrecta.");
      }
    } else {
      Serial.printf("Error en ThingSpeak: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("No se puede enviar a ThingSpeak: WiFi desconectado.");
  }
}

// ---------------------------------------------------------
// FUNCIÓN PARA MANEJAR LA PÁGINA PRINCIPAL LOCAL
// ---------------------------------------------------------
void handleRoot() {
  String html = "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>TempeMaster - Local</title>";
  html += "<style>body{font-family:sans-serif; background:#0b1120; color:#fff; text-align:center; padding:50px;} h1{color:#38bdf8;} .data{font-size:2rem; margin:20px; color:#f87171;} .hum{color:#60a5fa;}</style>";
  html += "</head><body>";
  html += "<h1>🌡️ Cómputo 3</h1>";
  if (isnan(current_t1) || isnan(current_h1)) {
    html += "<p>Error leyendo el sensor DHT11 en el pin 4.</p>";
  } else {
    html += "<div class='data'>Temperatura: " + String(current_t1, 1) + " °C</div>";
    html += "<div class='data hum'>Humedad: " + String(current_h1, 1) + " %</div>";
  }
  html += "<p style='color:#10b981; margin-top:30px;'>✔ Subiendo datos a ThingSpeak cada 15s...</p>";
  html += "<p style='color:#94a3b8; font-size:0.9rem;'>Para ver el panel global, abre el archivo dashboard.html</p>";
  html += "<script>setTimeout(function(){ location.reload(); }, 5000);</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Inicializar sensor local
  dht1.begin();
  Serial.println("Sensor DHT inicializado.");

  // Configurar WiFi en modo Cliente (Por DHCP)
  Serial.println("Conectando a la red WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Esperar conexión
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n¡Conectado a Internet!");
  Serial.print("Dirección IP local asignada (DHCP): ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor HTTP local iniciado.");
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();

  // Leer Sensor Local cada 5 segundos
  if (currentMillis - lastReadTime1 >= 5000) {
    lastReadTime1 = currentMillis;
    current_t1 = dht1.readTemperature();
    current_h1 = dht1.readHumidity();
    
    // Verificar si el WiFi se desconectó
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado. Intentando reconectar...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }

  // Enviar datos a ThingSpeak cada 20 segundos
  if (currentMillis - lastThingSpeakTime >= thingspeakInterval) {
    lastThingSpeakTime = currentMillis;
    sendToThingSpeak();
  }
}
