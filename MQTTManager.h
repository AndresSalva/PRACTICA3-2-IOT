#ifndef MQTTManager_h
#define MQTTManager_h

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <functional>
#include <vector> 

class MQTTManager {
  public:
    using MessageCallback = std::function<void(const String& topic, const String& message)>;
    
    MQTTManager(const char* mqttServer, int mqttPort, const char* clientId);
    
    void setCertificates(const char* caCert, const char* clientCert, const char* privateKey);
    
    bool connect();
    void disconnect();
    bool publish(const String& topic, const String& message, bool retained = false); 
    void subscribe(const String& topic, MessageCallback callback);
    void update();
    bool connected();
    
  private:
    struct Subscription {
        String topic;
        MessageCallback callback;
    };
    
    const char* _mqttServer;
    int _mqttPort;
    const char* _clientId;
    
    WiFiClientSecure _wifiClientSecure;
    PubSubClient _mqttClient;
    std::vector<Subscription> _subscriptions;
    
    void mqttCallback(char* topic, byte* payload, unsigned int length);
};

#endif