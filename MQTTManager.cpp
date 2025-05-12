#include "MQTTManager.h"
#include <Arduino.h>

MQTTManager::MQTTManager(const char* mqttServer, int mqttPort, const char* clientId)
  : _mqttServer(mqttServer), _mqttPort(mqttPort), _clientId(clientId) {
    
    _mqttClient.setClient(_wifiClientSecure);
    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->mqttCallback(topic, payload, length);
    });
    _mqttClient.setBufferSize(1024);
}

void MQTTManager::setCertificates(const char* caCert, const char* clientCert, const char* privateKey) {
    _wifiClientSecure.setCACert(caCert);
    _wifiClientSecure.setCertificate(clientCert);
    _wifiClientSecure.setPrivateKey(privateKey);
}

bool MQTTManager::connect() {
    if (_mqttClient.connected()) {
        return true;
    }

    Serial.print("Attempting MQTT connection to ");
    Serial.print(_mqttServer);
    Serial.print(" as ");
    Serial.println(_clientId);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Cannot connect to MQTT.");
        return false;
    }

    if (_mqttClient.connect(_clientId)) {
        Serial.println("MQTT connected!");
        for (const auto& sub : _subscriptions) {
            Serial.print("Resubscribing to: ");
            Serial.println(sub.topic);
            _mqttClient.subscribe(sub.topic.c_str());
        }
        return true;
    } else {
        Serial.print("MQTT connect failed, rc=");
        Serial.print(_mqttClient.state());
        char lastError[100];
        _wifiClientSecure.lastError(lastError, 100); 
        Serial.print(", WiFiClientSecure last error: ");
        Serial.println(lastError);
        return false;
    }
}

void MQTTManager::disconnect() {
    _mqttClient.disconnect();
    Serial.println("MQTT disconnected.");
}

bool MQTTManager::publish(const String& topic, const String& message, bool retained) {
    if (!connected()) {
        Serial.println("MQTT not connected. Cannot publish.");
        return false; 
    }
    
    // El método publish de PubSubClient devuelve un booleano
    bool success = _mqttClient.publish(topic.c_str(), message.c_str(), retained);
    
    if (success) {
        Serial.print("Successfully published to "); Serial.print(topic); Serial.print(": "); Serial.println(message);
    } else {
        Serial.print("Failed to publish to topic: "); Serial.println(topic);
    }
    return success; 
}

void MQTTManager::subscribe(const String& topic, MessageCallback callback) {
    _subscriptions.push_back({topic, callback}); 
    
    if (connected()) {
        if (_mqttClient.subscribe(topic.c_str())) {
            Serial.print("Subscribed to: "); Serial.println(topic);
        } else {
            Serial.print("Failed to subscribe to: "); Serial.println(topic);
        }
    } else {
        Serial.print("MQTT not connected. Subscription to '"); 
        Serial.print(topic); 
        Serial.println("' will activate on next connection.");
    }
}

void MQTTManager::update() {
    if (WiFi.status() != WL_CONNECTED) {
        if (_mqttClient.connected()) {
            _mqttClient.disconnect();
            Serial.println("WiFi lost, MQTT disconnected.");
        }
        return;
    }

    if (_mqttClient.connected()) {
        _mqttClient.loop();
    }
}

bool MQTTManager::connected() {
    return _mqttClient.connected();
}

void MQTTManager::mqttCallback(char* topicChar, byte* payload, unsigned int length) {
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    String topicStr(topicChar);
    for (const auto& sub : _subscriptions) {
        if (topicStr == sub.topic) {
            sub.callback(topicStr, message);
            return;
        }
    }
    Serial.print("No callback registered for MQTT topic: "); Serial.println(topicStr);
}