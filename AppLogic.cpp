#include "AppLogic.h"
#include "aws_iot_config.h"
#include <WiFi.h>
#include <time.h>

AppLogic::AppLogic()
    : _mqtt(AWS_IOT_ENDPOINT, 8883, THING_NAME),
      _sensor(SOIL_MOISTURE_PIN, SOIL_DRY_VALUE, SOIL_WET_VALUE),
      _servo(SERVO_PIN),
      _lastTelemetryMillis(0), 
      _lastReconnectAttempt(0),
      _currentShadowVersion(0),
      _lastReportedHumidityRange(RANGE_UNKNOWN), 
      _lastProcessedEmotion("NEUTRAL"),
      _reportJustSentByCallback(false)
       {}

void AppLogic::generateShadowTopics() {
    _shadowUpdateTopic = String("$aws/things/") + THING_NAME + "/shadow/update";
    _shadowDeltaTopic = String("$aws/things/") + THING_NAME + "/shadow/update/delta";
    _shadowGetTopic = String("$aws/things/") + THING_NAME + "/shadow/get";
    _shadowGetAcceptedTopic = String("$aws/things/") + THING_NAME + "/shadow/get/accepted";
    _shadowGetRejectedTopic = String("$aws/things/") + THING_NAME + "/shadow/get/rejected";
    _shadowUpdateAcceptedTopic = String("$aws/things/") + THING_NAME + "/shadow/update/accepted";
    _shadowUpdateRejectedTopic = String("$aws/things/") + THING_NAME + "/shadow/update/rejected";
}

void AppLogic::connectWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi. Will restart.");
        delay(1000);
        ESP.restart();
    }
}

void AppLogic::syncNTPTime() {
    Serial.print("Syncing NTP time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < 1000000000L && attempts < 20) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    if (now < 1000000000L) {
        Serial.println("\nFailed to sync NTP time. Check network. Will restart.");
        delay(1000);
        ESP.restart();
    } else {
      Serial.println("\nNTP time synchronized!");
      struct tm timeinfo;
      gmtime_r(&now, &timeinfo);
      Serial.print("Current UTC time: ");
      Serial.println(asctime(&timeinfo));
    }
}

void AppLogic::setupAWSMQTT() {
    Serial.println("Configuring AWS MQTT...");
    _mqtt.setCertificates(AWS_ROOT_CA, DEVICE_CERTIFICATE, DEVICE_PRIVATE_KEY);
    auto mqttCallbackWrapper = [this](const String& topic, const String& message) {
        this->handleMQTTMessage(topic, message);
    };
    _mqtt.subscribe(_shadowDeltaTopic, mqttCallbackWrapper);
    _mqtt.subscribe(_shadowGetAcceptedTopic, mqttCallbackWrapper);
    _mqtt.subscribe(_shadowGetRejectedTopic, mqttCallbackWrapper);
    _mqtt.subscribe(_shadowUpdateAcceptedTopic, mqttCallbackWrapper);
    _mqtt.subscribe(_shadowUpdateRejectedTopic, mqttCallbackWrapper);

    if (_mqtt.connect()) {
        Serial.println("Initial MQTT connection successful.");
        Serial.println("Requesting current shadow state (GET)...");
        if (!_mqtt.publish(_shadowGetTopic, "")) {
             Serial.println("Failed to publish GET request.");
        }
    } else {
        Serial.println("Initial MQTT connection failed. Will retry in loop.");
        _lastReconnectAttempt = millis();
    }
}

void AppLogic::setup() {
    Serial.begin(115200);
    while(!Serial);
    Serial.println("Starting AppLogic setup...");
    generateShadowTopics();
    connectWiFi();
    syncNTPTime();
    _servo.attach();
    _servo.setNeutral();
    setupAWSMQTT();
    Serial.println("AppLogic setup completed.");
}

void AppLogic::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Attempting to reconnect...");
        if (_mqtt.connected()) _mqtt.disconnect();
        connectWiFi();
        _lastReconnectAttempt = millis();
        return;
    }

    if (!_mqtt.connected()) {
        if (millis() - _lastReconnectAttempt > _reconnectInterval) {
            Serial.println("Attempting to reconnect MQTT...");
            if (_mqtt.connect()) {
                 Serial.println("MQTT reconnected successfully.");
                 Serial.println("Requesting current shadow state (GET) after reconnect...");
                if (!_mqtt.publish(_shadowGetTopic, "")) {
                    Serial.println("Failed to publish GET request post-reconnect.");
                }
            } else {
                Serial.println("MQTT reconnection failed.");
            }
            _lastReconnectAttempt = millis();
        }
    } else {
      _mqtt.update(); 
    }

    _sensor.update(); 

    HumidityRange currentSensorRange = _sensor.getCurrentRange();


    if (!_reportJustSentByCallback) {
        if (_mqtt.connected() && currentSensorRange != _lastReportedHumidityRange && currentSensorRange != RANGE_UNKNOWN) {

            const unsigned long minIntervalBetweenRangeReports = 10000; // 10 segundos
            if (millis() - _lastTelemetryMillis > minIntervalBetweenRangeReports) {

                Serial.print("Loop: Humidity range changed. Old: ");
                Serial.print(MoistureSensor::rangeToString(_lastReportedHumidityRange));
                Serial.print(" New: "); Serial.println(MoistureSensor::rangeToString(currentSensorRange));

                if (_currentShadowVersion > 0) { 
                    if (publishShadowReport()) {
                        _lastReportedHumidityRange = currentSensorRange; 
                        _lastTelemetryMillis = millis(); 
                    } else {
                        Serial.println("Loop: Report due to humidity range change FAILED.");
                    }
                } else {
                    Serial.println("Loop: Report due to humidity range change SKIPPED, _currentShadowVersion is 0. Waiting for GET.");
                    if (millis() - _lastReconnectAttempt > 60000 && !_mqtt.publish(_shadowGetTopic, "")) {
                        Serial.println("Loop: Attempting GET due to missing version for range change report.");
                    }
                }
            } else {
                Serial.println("Loop: Humidity range changed, but report rate limited. Will try later.");
            }
        }
    }


    if (_reportJustSentByCallback) {
        _lastReportedHumidityRange = _sensor.getCurrentRange();
        _lastTelemetryMillis = millis();
        _reportJustSentByCallback = false; 
        Serial.println("Loop: _reportJustSentByCallback was true. Resetting. _lastReportedHumidityRange and _lastTelemetryMillis updated.");
    }

    delay(100); // Pequeña pausa
}


void AppLogic::handleMQTTMessage(const String& topic, const String& payload) {
    Serial.println("\n--- AppLogic::handleMQTTMessage ---");
    Serial.print("Topic: "); Serial.println(topic);

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        return;
    }

    if (doc.containsKey("version")) {
        unsigned long newVersionInPayload = doc["version"].as<unsigned long>();
        if (topic == _shadowUpdateAcceptedTopic) {
            _currentShadowVersion = newVersionInPayload;
            Serial.print("INFO (UPDATE_ACCEPTED Specific): _currentShadowVersion DEFINITIVELY updated to: "); Serial.println(_currentShadowVersion);
        } else if (topic == _shadowDeltaTopic) {
             _currentShadowVersion = newVersionInPayload;
             Serial.print("INFO (DELTA Specific): _currentShadowVersion set to DELTA document version: "); Serial.println(_currentShadowVersion);
        } else if (topic == _shadowGetAcceptedTopic) {
            if (newVersionInPayload > _currentShadowVersion || _currentShadowVersion == 0) {
                _currentShadowVersion = newVersionInPayload;
                Serial.print("INFO (GET_ACCEPTED): _currentShadowVersion updated to: "); Serial.println(_currentShadowVersion);
            } else {
                 Serial.print("INFO (GET_ACCEPTED): Received version "); Serial.print(newVersionInPayload);
                 Serial.print(" is not newer than current _currentShadowVersion "); Serial.println(_currentShadowVersion);
            }
        }
    } else {
        if (topic == _shadowDeltaTopic || topic == _shadowGetAcceptedTopic || topic == _shadowUpdateAcceptedTopic) {
            Serial.print("WARN: Message on topic "); Serial.print(topic); Serial.println(" did not contain 'version'. Version sync might be impacted.");
        }
    }

    if (topic == _shadowDeltaTopic) {
        Serial.println("Processing Shadow DELTA...");
        if (!doc.containsKey("version")) {
             Serial.println("CRITICAL (DELTA): Delta message did not contain 'version'. Aborting.");
             return;
        }
        if (doc.containsKey("state")) {
            handleShadowDelta(doc["state"].as<JsonObjectConst>());
        } else {
            Serial.println("DELTA: Message does not contain 'state' object.");
        }
        Serial.println("DELTA: Attempting to publish shadow report to clear delta.");
        if (publishShadowReport()) {

            _reportJustSentByCallback = true; 
            Serial.print("DELTA: Report SUCCESS. Expected new cloud version post-accept: ");
            Serial.println(_currentShadowVersion + 1);
        } else {
             Serial.println("DELTA: Report FAILED after processing delta. Delta might persist.");
        }

    } else if (topic == _shadowGetAcceptedTopic) {
        Serial.println("Processing Shadow GET_ACCEPTED...");
        handleShadowGetAccepted(doc.as<JsonObjectConst>()); 

    } else if (topic == _shadowGetRejectedTopic) {
        Serial.print("Shadow GET request REJECTED: "); Serial.println(payload);
        if (doc.containsKey("code") && doc["code"] == 404) {
            Serial.println("GET_REJECTED: Shadow not found (404). Initial report might be needed if `isInitialReportScenario` triggers in GET_ACCEPTED logic.");
        }
    } else if (topic == _shadowUpdateAcceptedTopic) {
        Serial.println("Processing Shadow UPDATE_ACCEPTED...");
        handleShadowUpdateAccepted(doc.as<JsonObjectConst>());
    } else if (topic == _shadowUpdateRejectedTopic) {
        Serial.println("Processing Shadow UPDATE_REJECTED...");
        handleShadowUpdateRejected(doc.as<JsonObjectConst>());
    }
}

void AppLogic::handleShadowDelta(JsonObjectConst deltaState) {
    bool stateChangedByDelta = false;
    String desiredEmotion = _lastProcessedEmotion; 

    if (deltaState.containsKey("servoAngle")) {
        int desiredAngle = deltaState["servoAngle"];
        Serial.print("Delta State: Desired servoAngle: "); Serial.println(desiredAngle);
        if (_servo.getCurrentAngle() != desiredAngle) {
            _servo.setAngle(desiredAngle);
            stateChangedByDelta = true;
            if (!deltaState.containsKey("emotion")) { 
                if (desiredAngle == SERVO_HAPPY_ANGLE) desiredEmotion = "FELIZ";
                else if (desiredAngle == SERVO_SAD_ANGLE) desiredEmotion = "TRISTE";
                else if (desiredAngle == SERVO_NEUTRAL_ANGLE) desiredEmotion = "NEUTRAL";
                else desiredEmotion = "CUSTOM";
            }
        }
    }

    if (deltaState.containsKey("emotion")) {
        String emotionValue = deltaState["emotion"].as<String>();
        Serial.print("Delta State: Desired emotion: "); Serial.println(emotionValue);
        
        if (emotionValue != _lastProcessedEmotion || 
            (emotionValue == "FELIZ" && _servo.getCurrentAngle() != SERVO_HAPPY_ANGLE) ||
            (emotionValue == "TRISTE" && _servo.getCurrentAngle() != SERVO_SAD_ANGLE) ||
            (emotionValue == "NEUTRAL" && _servo.getCurrentAngle() != SERVO_NEUTRAL_ANGLE) ) {
            
            if (emotionValue == "FELIZ") { _servo.setHappy(); desiredEmotion = "FELIZ"; stateChangedByDelta = true;}
            else if (emotionValue == "TRISTE") { _servo.setSad(); desiredEmotion = "TRISTE"; stateChangedByDelta = true;}
            else if (emotionValue == "NEUTRAL") { _servo.setNeutral(); desiredEmotion = "NEUTRAL"; stateChangedByDelta = true;}
            else {
                Serial.print("Delta State: Unknown emotion value: "); Serial.println(emotionValue);
            }
        }
    }

    if (stateChangedByDelta) {
        _lastProcessedEmotion = desiredEmotion;
        Serial.print("Delta State: Local device state changed. _lastProcessedEmotion set to: "); Serial.println(_lastProcessedEmotion);
    } else {
        Serial.println("Delta State: No local device state changes made by delta.");
    }
}

void AppLogic::handleShadowGetAccepted(JsonObjectConst shadowDocument) {
    Serial.print("GET_ACCEPTED: Current _currentShadowVersion is: "); Serial.println(_currentShadowVersion);
    bool needsReportAfterGet = false;
    bool appliedDesired = false;
    bool isInitialReportScenario = false;

    if (shadowDocument.containsKey("state")) {
        JsonObjectConst state = shadowDocument["state"];
        if (state.containsKey("reported")) {
            JsonObjectConst reportedState = state["reported"];
            Serial.println("GET_ACCEPTED: Processing 'reported' state from shadow.");
            if (reportedState.containsKey("emotion")) {
                 String reportedEmotion = reportedState["emotion"].as<String>();
                 if (_lastProcessedEmotion != reportedEmotion && (reportedEmotion == "FELIZ" || reportedEmotion == "TRISTE" || reportedEmotion == "NEUTRAL")) {
                     _lastProcessedEmotion = reportedEmotion;
                     Serial.print("GET_ACCEPTED: Synced _lastProcessedEmotion from shadow's reported to: "); Serial.println(_lastProcessedEmotion);
                     if (reportedEmotion == "FELIZ" && _servo.getCurrentAngle() != SERVO_HAPPY_ANGLE) _servo.setHappy();
                     else if (reportedEmotion == "TRISTE" && _servo.getCurrentAngle() != SERVO_SAD_ANGLE) _servo.setSad();
                     else if (reportedEmotion == "NEUTRAL" && _servo.getCurrentAngle() != SERVO_NEUTRAL_ANGLE) _servo.setNeutral();
                 }
            }
            if (reportedState.containsKey("servoAngle")) {
                int reportedAngle = reportedState["servoAngle"];
                if (_servo.getCurrentAngle() != reportedAngle) {
                    Serial.print("GET_ACCEPTED: Syncing servoAngle from reported: "); Serial.println(reportedAngle);
                    _servo.setAngle(reportedAngle);
                }
            }
            if (reportedState.containsKey("humidityRange")) {
                String reportedRangeStr = reportedState["humidityRange"].as<String>();
                HumidityRange shadowRange = MoistureSensor::stringToRange(reportedRangeStr);
                if (shadowRange != RANGE_UNKNOWN && _lastReportedHumidityRange != shadowRange) {
                    _lastReportedHumidityRange = shadowRange; // <<<--- ACTUALIZAR ESTO
                    Serial.print("GET_ACCEPTED: Synced _lastReportedHumidityRange from shadow's reported to: ");
                    Serial.println(reportedRangeStr);
                }
            } else {

                 if (_sensor.getCurrentRange() != RANGE_UNKNOWN) {
                     Serial.println("GET_ACCEPTED: No 'humidityRange' in reported shadow, but sensor has a range. Flagging for initial report.");
                     isInitialReportScenario = true; 
                     needsReportAfterGet = true;
                 }
            }
        } else {
             Serial.println("GET_ACCEPTED: No 'reported' state in shadow. Will report current device state.");
             isInitialReportScenario = true;
             needsReportAfterGet = true;
        }

        if (state.containsKey("desired")) {
            JsonObjectConst desiredState = state["desired"];
            Serial.println("GET_ACCEPTED: Processing 'desired' state from shadow.");
            if (!desiredState.isNull() && desiredState.size() > 0) {
                handleShadowDelta(desiredState);
                needsReportAfterGet = true;
                appliedDesired = true;
            } else {
                Serial.println("GET_ACCEPTED: 'desired' state is null or empty.");
            }
        }
    } else {
        Serial.println("GET_ACCEPTED: Shadow is empty or has no 'state' object. Will report current device state.");
        isInitialReportScenario = true;
        needsReportAfterGet = true;
    }

    if (needsReportAfterGet) {
        Serial.print("GET_ACCEPTED: Needs to publish a shadow report. Applied desired: "); Serial.print(appliedDesired);
        Serial.print(" | Initial report scenario: "); Serial.println(isInitialReportScenario);
        if (publishShadowReport()) {
            _reportJustSentByCallback = true; 
            Serial.print("GET_ACCEPTED: Report SUCCESS. Expected new cloud version post-accept: ");
            Serial.println(_currentShadowVersion + 1);
        } else {
            Serial.println("GET_ACCEPTED: Report FAILED after processing GET_ACCEPTED.");
        }
    } else {
        Serial.println("GET_ACCEPTED: No report needed after processing.");
        _lastTelemetryMillis = millis();
    }
}

bool AppLogic::publishShadowReport() {
    Serial.println("\n--- publishShadowReport called ---");
    if (!_mqtt.connected()) {
        Serial.println("PUBLISH: MQTT not connected. Cannot publish.");
        return false;
    }
    StaticJsonDocument<512> doc;
    doc["version"] = _currentShadowVersion;
    Serial.print("PUBLISH: Publishing with _currentShadowVersion: "); Serial.println(_currentShadowVersion);

    JsonObject stateObj = doc.createNestedObject("state");
    JsonObject reportedObj = stateObj.createNestedObject("reported");

    reportedObj["rawSoilMoisture"] = _sensor.getRawValue();
    reportedObj["soilMoisturePercent"] = _sensor.getPercentage();
    reportedObj["humidityRange"] = _sensor.getRangeString(); 
    reportedObj["servoAngle"] = _servo.getCurrentAngle();

    if (_lastProcessedEmotion == "FELIZ" || _lastProcessedEmotion == "TRISTE" || _lastProcessedEmotion == "NEUTRAL") {
        reportedObj["emotion"] = _lastProcessedEmotion;
    } else {
        int currentAngle = _servo.getCurrentAngle();
        if (currentAngle == SERVO_HAPPY_ANGLE) reportedObj["emotion"] = "FELIZ";
        else if (currentAngle == SERVO_SAD_ANGLE) reportedObj["emotion"] = "TRISTE";
        else if (currentAngle == SERVO_NEUTRAL_ANGLE) reportedObj["emotion"] = "NEUTRAL";
        else reportedObj["emotion"] = "CUSTOM";
    }
    Serial.print("PUBLISH: Reporting emotion as: "); Serial.println(reportedObj["emotion"].as<String>());
    Serial.print("PUBLISH: Reporting humidityRange as: "); Serial.println(reportedObj["humidityRange"].as<String>());


    stateObj["desired"] = nullptr;
    String payloadStr;
    serializeJson(doc, payloadStr);
    Serial.print("PUBLISH: Publishing Payload: "); Serial.println(payloadStr);
    bool success = _mqtt.publish(_shadowUpdateTopic, payloadStr);

    if(success) {
        Serial.print("PUBLISH: Shadow Report SUCCEEDED for version "); Serial.println(_currentShadowVersion);
        return true;
    } else {
        Serial.println("PUBLISH: Shadow Report FAILED (MQTTManager::publish returned false).");
        return false;
    }
}

void AppLogic::handleShadowUpdateAccepted(JsonObjectConst acceptedPayload) {
    Serial.println("UPDATE_ACCEPTED: Shadow update was ACCEPTED by AWS IoT.");
    Serial.print("UPDATE_ACCEPTED: Confirmed version is now: "); Serial.println(_currentShadowVersion);
}

void AppLogic::handleShadowUpdateRejected(JsonObjectConst rejectedPayload) {
    Serial.print("UPDATE_REJECTED: Shadow update was REJECTED. Payload: ");
    String rejectedPayloadStr;
    serializeJson(rejectedPayload, rejectedPayloadStr);
    Serial.println(rejectedPayloadStr);
    if (rejectedPayload.containsKey("code") && rejectedPayload["code"] == 409) {
        Serial.println("UPDATE_REJECTED: Version conflict (409). Requesting current shadow to re-sync.");
        if (!_mqtt.publish(_shadowGetTopic, "")) {
            Serial.println("Failed to publish GET request after 409.");
        }
    } else if (rejectedPayload.containsKey("code")) {
        Serial.print("UPDATE_REJECTED: Shadow update rejected with code: ");
        Serial.println(rejectedPayload["code"].as<int>());
    }
}