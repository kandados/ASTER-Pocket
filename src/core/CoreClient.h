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

private:
    void addAuthenticationHeader(
        class HTTPClient &http
    );
};


extern CoreClientClass CoreClient;
