#pragma once

#include <Arduino.h>


class CoreClientClass
{
public:
    bool checkHealth();

    bool createConversation(
        String &conversationId
    );

    bool sendMessage(
        const String &conversationId,
        const String &message,
        String &answer,
        String &provider,
        String &model
    );

    bool sendAudio(
        const String &conversationId,
        const uint8_t *audioData,
        size_t audioBytes,
        uint32_t sampleRate,
        String &receipt
    );

private:
    void addAuthenticationHeader(
        class HTTPClient &http
    );
};


extern CoreClientClass CoreClient;
