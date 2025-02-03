#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Servo.h>
#include <WiFiClientSecure.h>
#include <base64.h>

// Définition des broches
const int trigPin1 = 5;  // Capteur 1 (extérieur)
const int echoPin1 = 4;
const int trigPin2 = 12; // Capteur 2 (intérieur)
const int echoPin2 = 13;

const int ledPin = 0;
const int buzzerPin = 2;
const int servoPin = 14;
unsigned long lastGitHubUpdate = 0; // Stocke le temps du dernier envoi
const int GITHUB_UPDATE_INTERVAL = 30000; // Intervalle en millisecondes (30 sec)

// Variables pour les distances
long duration1, duration;
int distance1, distance;

// Informations WiFi
const char* ssid = "Dark";
const char* password = "123456789";

// Serveur WebSocket sur le port 81
WebSocketsServer webSocket(81);

// Initialisation du servo
Servo servoMotor;

// GitHub API
const char* githubHost = "api.github.com";
const char* githubToken = "github_";  // ⚠️ Remplace avec un token sécurisé
const char* repoOwner = "130shadow135";
const char* repoName = "ESP8266";
const char* filePath = "data.json";

// Client sécurisé pour GitHub
WiFiClientSecure client;

void setup() {
    pinMode(trigPin1, OUTPUT);
    pinMode(echoPin1, INPUT);
    pinMode(trigPin2, OUTPUT);
    pinMode(echoPin2, INPUT);
    pinMode(ledPin, OUTPUT);
    pinMode(buzzerPin, OUTPUT);
    
    servoMotor.attach(servoPin);

    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connexion au WiFi...");
    }
    Serial.println("Connecté au WiFi");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    client.setInsecure();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Déconnecté !\n", num);
            break;

        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connecté depuis %s\n", num, ip.toString().c_str());
            break;
        }

        case WStype_TEXT:
            Serial.printf("[%u] Message reçu : %s\n", num, payload);
            break;
    }
}

void loop() {
    mesurerDistances();
    gererAlarme();
    
    webSocket.loop();
    Serial.println("WebSocket actif !"); // Vérification
    
    sendWebSocketData();
    
    if (millis() - lastGitHubUpdate >= GITHUB_UPDATE_INTERVAL) {
        sendToGitHub();
        lastGitHubUpdate = millis();
    }
    
    delay(500);
}


// Fonction pour mesurer la distance avec un capteur ultrasonique
int mesurerDistance(int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH, 10000); // Timeout 10ms

    return duration * 0.034 / 2;
}

// Mesurer les distances des deux capteurs
void mesurerDistances() {
    distance1 = mesurerDistance(trigPin1, echoPin1);

    if (distance1 > 15) {  
        // Si le capteur extérieur ne détecte rien, on active la mesure du capteur intérieur
        distance = mesurerDistance(trigPin2, echoPin2);
    } else {
        // Si le capteur extérieur détecte un objet, on désactive le capteur intérieur
        distance = -1; // Valeur spéciale pour signaler qu'il est désactivé
    }

    Serial.print("Distance 1: ");
    Serial.print(distance1);
    Serial.print(" cm | Distance : ");
    if (distance == -1) {
        Serial.println("Désactivé");
    } else {
        Serial.print(distance);
        Serial.println(" cm");
    }
}

// Gestion de l'activation des composants
void gererAlarme() {
    if (distance1 <= 15) {
        // Si le premier capteur détecte un objet proche, on désactive tout
        servoMotor.write(120);

    }  else if (distance != -1 && distance <= 12) {
        // Si le capteur 1 est dégagé (distance1 > 15) et capteur 2 détecte un objet
        servoMotor.write(0);
        digitalWrite(ledPin, LOW);
        digitalWrite(buzzerPin, LOW);
    } else {
        // Sinon, on active seulement le premier composant
        servoMotor.write(120);
        digitalWrite(ledPin, HIGH);
        digitalWrite(buzzerPin, HIGH);
    }
}

// Envoi des données via WebSocket
void sendWebSocketData() {
    String json = "{";
    json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    json += "\"distance1\":\"" + String(distance1) + "\",";
    json += "\"distance\":\"" + (distance == -1 ? "désactivé" : String(distance)) + "\"}";
    webSocket.broadcastTXT(json);
}

// Envoi des données vers GitHub
void sendToGitHub() {
    Serial.println("Connexion à GitHub...");

    if (client.connect(githubHost, 443)) {
        Serial.println("Connexion réussie !");
        
        String jsonData = "{\"timestamp\":\"2025-01-30 12:00:00\", \"distance1\":\"" + String(distance1) + " cm\", \"distance\":\"" + (distance == -1 ? "désactivé" : String(distance) + " cm") + "\"}";
        String encodedJson = base64::encode(jsonData);

        String url = "/repos/" + String(repoOwner) + "/" + String(repoName) + "/contents/" + String(filePath);
        String payload = "{ \"message\": \"Maj depuis ESP\", \"content\": \"" + encodedJson + "\", \"branch\": \"main\" }";

        client.print(String("PUT ") + url + " HTTP/1.1\r\n" +
                     "Host: " + githubHost + "\r\n" +
                     "Authorization: token " + githubToken + "\r\n" +
                     "User-Agent: ESP8266\r\n" +
                     "Content-Type: application/json\r\n" +
                     "Content-Length: " + payload.length() + "\r\n" +
                     "Connection: close\r\n\r\n" +
                     payload);

        while (client.available()) {
            String response = client.readString();
            Serial.println(response);
        }
    } else {
        Serial.println("Connexion échouée !");
    }
}
