#include <WiFi.h>
#include <WebServer.h>
#include "driver/temp_sensor.h"  // Añadido para el sensor de temperatura

// Configuración del Access Point
const char* ap_ssid = "ESP32_Debug";
const char* ap_password = "12345678";

WebServer server(80);
String debugLog = "";

// Variables para estadisticas
int messageCount = 0;
float lastTemperature = 0.0;  // Para almacenar la última temperatura

// Página web mejorada con temperatura
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Debug Console</title>
    <meta http-equiv="refresh" content="2">
    <style>
        body { 
            background: #1e1e1e; 
            color: #d4d4d4; 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, monospace;
            margin: 20px;
        }
        h1 { 
            color: #569cd6; 
            text-align: center;
        }
        .console { 
            height: 400px; 
            overflow-y: scroll; 
            border: 2px solid #333; 
            border-radius: 5px;
            padding: 10px; 
            margin-bottom: 20px;
            font-family: 'Courier New', monospace;
        }
        .stats {
            background: #252526;
            padding: 10px;
            border-radius: 5px;
            margin-bottom: 20px;
            display: flex;
            gap: 20px;
            flex-wrap: wrap;
        }
        .stat-item {
            color: #9cdcfe;
        }
        .info { color: #9cdcfe; }
        .warning { color: #ce9178; }
        .error { color: #f48771; }
        .success { color: #6a9955; }
        .temperature { color: #4ec9b0; }
        .timestamp {
            color: #808080;
            font-size: 12px;
            margin-right: 10px;
        }
        button {
            background: #0e639c;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 3px;
            cursor: pointer;
            margin-right: 10px;
            font-size: 14px;
        }
        button:hover {
            background: #1177bb;
        }
        .button-restart {
            background: #a1260d;
        }
        .button-restart:hover {
            background: #c13b1f;
        }
        .log-entry {
            margin: 4px 0;
            padding: 2px 5px;
            border-left: 3px solid transparent;
        }
        .info { border-left-color: #569cd6; }
        .warning { border-left-color: #ce9178; }
        .error { border-left-color: #f48771; }
        .success { border-left-color: #6a9955; }
        .temperature { border-left-color: #4ec9b0; }
    </style>
</head>
<body>
    <h1>ESP32 Debug Console</h1>
    
    <div class="stats">
        <span class="stat-item">⏱️ Tiempo: <span id="uptime">%UPTIME%</span></span>
        <span class="stat-item">💾 Memoria: <span id="heap">%HEAP%</span></span>
        <span class="stat-item">📱 Clientes: <span id="clients">%CLIENTS%</span></span>
        <span class="stat-item">📝 Mensajes: <span id="msgCount">%MSGCOUNT%</span></span>
        <span class="stat-item">🌡️ Temperatura: <span id="temperature">%TEMPERATURE%</span></span>
    </div>

    <div class="console" id="console">
        %DEBUG_LOG%
    </div>

    <button onclick="fetch('/clear')">Limpiar</button>
    <button onclick="fetch('/restart').then(() => location.reload())" class="button-restart">Reiniciar ESP32</button>
    <button onclick="location.reload()">Refrescar</button>

    <script>
        window.onload = function() {
            const console = document.getElementById('console');
            console.scrollTop = console.scrollHeight;
        }
    </script>
</body>
</html>
)rawliteral";

// Función para inicializar el sensor de temperatura
void initTemperatureSensor() {
    temp_sensor_config_t temp_sensor_config = {
        .dac_offset = TSENS_DAC_L2,  // offset = 0 (-10°C ~ 80°C, mejor precisión)
        .clk_div = 6
    };
    
    temp_sensor_set_config(temp_sensor_config);
    temp_sensor_start();
}

// Función para leer la temperatura
float readTemperature() {
    float temp = 0;
    esp_err_t result = temp_sensor_read_celsius(&temp);
    
    if (result == ESP_OK) {
        return temp;
    } else {
        return -273.15;  // Error: devuelve cero absoluto como indicador
    }
}

// Funcion mejorada para añadir mensajes con diferentes tipos y colores
void addToLog(String msg, String type = "info") {
    String colorClass = "info";
    if (type == "warning") colorClass = "warning";
    if (type == "error") colorClass = "error";
    if (type == "success") colorClass = "success";
    if (type == "temperature") colorClass = "temperature";
    
    String timestamp = "[" + String(millis()/1000) + "s]";
    String formattedMsg = "<div class='log-entry " + colorClass + "'><span class='timestamp'>" + timestamp + "</span> " + msg + "</div>";
    
    debugLog += formattedMsg;
    messageCount++;
    
    // Limitar tamaño del log
    if (debugLog.length() > 3000) {
        int cutIndex = debugLog.indexOf("</div>", debugLog.length() - 2500);
        if (cutIndex != -1) {
            debugLog = debugLog.substring(cutIndex + 6);
        } else {
            debugLog = debugLog.substring(debugLog.length() - 2000);
        }
    }
}

void handleRoot() {
    String html = String(index_html);
    
    // Calcular tiempo activo
    long uptime = millis() / 1000;
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    String uptimeStr = String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
    
    // Leer temperatura actual para mostrarla
    float currentTemp = readTemperature();
    String tempStr;
    if (currentTemp > -100) {
        tempStr = String(currentTemp, 1) + " °C";
    } else {
        tempStr = "Error";
    }
    
    html.replace("%DEBUG_LOG%", debugLog);
    html.replace("%UPTIME%", uptimeStr);
    html.replace("%HEAP%", String(ESP.getFreeHeap()) + " bytes");
    html.replace("%CLIENTS%", String(WiFi.softAPgetStationNum()));
    html.replace("%MSGCOUNT%", String(messageCount));
    html.replace("%TEMPERATURE%", tempStr);
    
    server.send(200, "text/html", html);
}

void handleClear() {
    debugLog = "";
    messageCount = 0;
    addToLog("Log limpiado", "success");
    server.send(200, "text/plain", "OK");
}

// Funcion para reiniciar el ESP32
void handleRestart() {
    server.send(200, "text/html", "<html><body><h1>Reiniciando ESP32...</h1></body></html>");
    delay(100);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("=== INICIANDO ===");
    
    // Inicializar sensor de temperatura
    initTemperatureSensor();
    Serial.println("Sensor de temperatura iniciado");
    
    // Configurar WiFi
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP: ");
    Serial.println(IP);
    
    // Configurar servidor
    server.on("/", handleRoot);
    server.on("/clear", handleClear);
    server.on("/restart", handleRestart);
    
    server.begin();
    Serial.println("Servidor iniciado");
    
    addToLog("Sistema listo", "success");
    addToLog("Conectate a: http://" + IP.toString(), "info");
    addToLog("SSID: " + String(ap_ssid), "info");
}

void loop() {
    server.handleClient();
    
    static unsigned long lastMsg = 0;
    static int counter = 0;
    
    if (millis() - lastMsg > 5000) {
        lastMsg = millis();
        counter++;
        
        // Leer temperatura actual
        float temp = readTemperature();
        
        // Diferentes tipos de mensajes SIN caracteres especiales
        int randomType = random(0, 7);  // Aumentado a 7 para incluir temperatura
        
        switch(randomType) {
            case 0: {
                String msg = "Memoria libre: " + String(ESP.getFreeHeap()) + " bytes";
                Serial.println(msg);
                addToLog(msg, "info");
                break;
            }
            case 1: {
                int clients = WiFi.softAPgetStationNum();
                String msg = "Clientes conectados: " + String(clients);
                Serial.println(msg);
                addToLog(msg, "info");
                break;
            }
            case 2: {
                String msg = "ADVERTENCIA: Temperatura " + String(random(35, 45)) + "C";
                Serial.println("[WARN] " + msg);
                addToLog(msg, "warning");
                break;
            }
            case 3: {
                String msg = "ERROR: Sensor " + String(random(1, 4)) + " fallo";
                Serial.println("[ERROR] " + msg);
                addToLog(msg, "error");
                break;
            }
            case 4: {
                String msg = "OK: Operacion completada en " + String(random(10, 100)) + "ms";
                Serial.println("[OK] " + msg);
                addToLog(msg, "success");
                break;
            }
            case 5: {
                String msg = "Contador: " + String(counter) + " | Tiempo: " + String(millis()/1000) + "s";
                Serial.println(msg);
                addToLog(msg, "info");
                break;
            }
            case 6: {
                if (temp > -100) {
                    String msg = "Temperatura interna: " + String(temp, 1) + " C";
                    Serial.println("[TEMP] " + msg);
                    addToLog(msg, "temperature");
                } else {
                    String msg = "Error leyendo temperatura";
                    Serial.println("[ERROR] " + msg);
                    addToLog(msg, "error");
                }
                break;
            }
        }
    }
    
    delay(10);
}