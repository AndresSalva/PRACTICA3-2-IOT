#ifndef AppLogic_h
#define AppLogic_h

#include "MQTTManager.h"
#include "MoistureSensor.h" 
#include "EmotionalServo.h"
#include <ArduinoJson.h>

class AppLogic {
  public:
    AppLogic();
    void setup();
    void loop();

  private:
    MQTTManager _mqtt;
    MoistureSensor _sensor;
    EmotionalServo _servo;

    String _shadowUpdateTopic;
    String _shadowDeltaTopic;
    String _shadowGetTopic;
    String _shadowGetAcceptedTopic;
    String _shadowGetRejectedTopic;
    String _shadowUpdateAcceptedTopic;
    String _shadowUpdateRejectedTopic;

    unsigned long _lastTelemetryMillis;
    unsigned long _lastReconnectAttempt;
    const long _reconnectInterval = 5000;

    unsigned long _currentShadowVersion;
    HumidityRange _lastReportedHumidityRange;
    String _lastProcessedEmotion; 

    bool _reportJustSentByCallback;

    void connectWiFi();
    void syncNTPTime();
    void setupAWSMQTT();
    void generateShadowTopics();
    void handleMQTTMessage(const String& topic, const String& payload);
    void handleShadowDelta(JsonObjectConst deltaState);
    void handleShadowGetAccepted(JsonObjectConst shadowState);
    bool publishShadowReport();
    void handleShadowUpdateAccepted(JsonObjectConst acceptedPayload);
    void handleShadowUpdateRejected(JsonObjectConst rejectedPayload);
};

#endif