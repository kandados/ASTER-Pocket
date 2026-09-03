#pragma once

#include <Arduino.h>


struct CoreAudioTurnResult
{
    String transcription;
    String answer;

    String assistantMessageId;
    String provider;
    String model;
    String rawResponse;
};

enum class CoreAudioStreamEventType : uint8_t
{
    Transcription,
    Start,
    Delta,
    TextDone,
    AudioStart,
    AudioPcm,
    AudioEnd,
    AudioError,
    Done,
    Error
};


using CoreAudioStreamCallback = bool (*)(
    CoreAudioStreamEventType eventType,
    const String &content,
    void *context
);


using CoreSpeechPcmCallback = bool (*)(

    const int16_t *samples,

    size_t sampleCount,

    void *context

);



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
        CoreAudioTurnResult &result
    );

    bool sendAudioStream(
        const String &conversationId,
        const uint8_t *audioData,
        size_t audioBytes,
        uint32_t sampleRate,
        CoreAudioTurnResult &result,
        CoreAudioStreamCallback callback,
        void *context = nullptr
    );

    bool streamSpeechText(
        const String &conversationId,
        const String &text,
        CoreSpeechPcmCallback callback,
        void *context = nullptr
    );

    bool streamSpeech(

        const String &conversationId,

        const String &messageId,

        CoreSpeechPcmCallback callback,

        void *context = nullptr

    );


private:
    void addAuthenticationHeader(
        class HTTPClient &http
    );
};


extern CoreClientClass CoreClient;
