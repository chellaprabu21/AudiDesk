/*
 *  AudioRingBuffer.c
 *  AudioRouterDriver
 *
 *  Lock-free ring buffer implementation for audio data transfer
 */

#include "AudioRingBuffer.h"
#include <stdlib.h>
#include <string.h>

AudioRingBuffer* AudioRingBuffer_Create(uint32_t bufferSizeFrames, uint32_t channelCount) {
    AudioRingBuffer* buffer = (AudioRingBuffer*)malloc(sizeof(AudioRingBuffer));
    if (!buffer) {
        return NULL;
    }
    
    // Round up to power of 2 for efficient modulo operation
    uint32_t powerOf2Size = 1;
    while (powerOf2Size < bufferSizeFrames) {
        powerOf2Size <<= 1;
    }
    
    buffer->bufferSize = powerOf2Size;
    buffer->channelCount = channelCount;
    
    // Allocate interleaved buffer
    size_t totalSamples = (size_t)powerOf2Size * channelCount;
    buffer->buffer = (float*)calloc(totalSamples, sizeof(float));
    
    if (!buffer->buffer) {
        free(buffer);
        return NULL;
    }
    
    atomic_init(&buffer->writeIndex, 0);
    atomic_init(&buffer->readIndex, 0);
    
    return buffer;
}

void AudioRingBuffer_Destroy(AudioRingBuffer* buffer) {
    if (buffer) {
        if (buffer->buffer) {
            free(buffer->buffer);
        }
        free(buffer);
    }
}

void AudioRingBuffer_Reset(AudioRingBuffer* buffer) {
    if (buffer) {
        atomic_store(&buffer->writeIndex, 0);
        atomic_store(&buffer->readIndex, 0);
        
        size_t totalSamples = (size_t)buffer->bufferSize * buffer->channelCount;
        memset(buffer->buffer, 0, totalSamples * sizeof(float));
    }
}

uint32_t AudioRingBuffer_GetAvailableFrames(AudioRingBuffer* buffer) {
    if (!buffer) return 0;
    
    uint64_t writeIdx = atomic_load(&buffer->writeIndex);
    uint64_t readIdx = atomic_load(&buffer->readIndex);
    
    return (uint32_t)(writeIdx - readIdx);
}

uint32_t AudioRingBuffer_GetFreeFrames(AudioRingBuffer* buffer) {
    if (!buffer) return 0;
    
    return buffer->bufferSize - AudioRingBuffer_GetAvailableFrames(buffer);
}

uint32_t AudioRingBuffer_Write(AudioRingBuffer* buffer, const float* data, uint32_t frameCount) {
    if (!buffer || !data || frameCount == 0) return 0;
    
    uint32_t freeFrames = AudioRingBuffer_GetFreeFrames(buffer);
    uint32_t framesToWrite = (frameCount < freeFrames) ? frameCount : freeFrames;
    
    if (framesToWrite == 0) return 0;
    
    uint64_t writeIdx = atomic_load(&buffer->writeIndex);
    uint32_t bufferMask = buffer->bufferSize - 1;
    uint32_t channelCount = buffer->channelCount;
    
    for (uint32_t i = 0; i < framesToWrite; i++) {
        uint32_t bufferPos = (uint32_t)((writeIdx + i) & bufferMask);
        size_t bufferOffset = (size_t)bufferPos * channelCount;
        size_t dataOffset = (size_t)i * channelCount;
        
        for (uint32_t ch = 0; ch < channelCount; ch++) {
            buffer->buffer[bufferOffset + ch] = data[dataOffset + ch];
        }
    }
    
    // Memory barrier to ensure data is written before updating index
    atomic_thread_fence(memory_order_release);
    atomic_store(&buffer->writeIndex, writeIdx + framesToWrite);
    
    return framesToWrite;
}

uint32_t AudioRingBuffer_Read(AudioRingBuffer* buffer, float* data, uint32_t frameCount) {
    if (!buffer || !data || frameCount == 0) return 0;
    
    uint32_t availableFrames = AudioRingBuffer_GetAvailableFrames(buffer);
    uint32_t framesToRead = (frameCount < availableFrames) ? frameCount : availableFrames;
    
    if (framesToRead == 0) {
        // Fill with silence if no data available
        memset(data, 0, (size_t)frameCount * buffer->channelCount * sizeof(float));
        return 0;
    }
    
    uint64_t readIdx = atomic_load(&buffer->readIndex);
    uint32_t bufferMask = buffer->bufferSize - 1;
    uint32_t channelCount = buffer->channelCount;
    
    // Memory barrier to ensure we read the latest data
    atomic_thread_fence(memory_order_acquire);
    
    for (uint32_t i = 0; i < framesToRead; i++) {
        uint32_t bufferPos = (uint32_t)((readIdx + i) & bufferMask);
        size_t bufferOffset = (size_t)bufferPos * channelCount;
        size_t dataOffset = (size_t)i * channelCount;
        
        for (uint32_t ch = 0; ch < channelCount; ch++) {
            data[dataOffset + ch] = buffer->buffer[bufferOffset + ch];
        }
    }
    
    atomic_store(&buffer->readIndex, readIdx + framesToRead);
    
    // Fill remaining frames with silence if needed
    if (framesToRead < frameCount) {
        size_t remainingOffset = (size_t)framesToRead * channelCount;
        size_t remainingSamples = (size_t)(frameCount - framesToRead) * channelCount;
        memset(data + remainingOffset, 0, remainingSamples * sizeof(float));
    }
    
    return framesToRead;
}

uint32_t AudioRingBuffer_Peek(AudioRingBuffer* buffer, float* data, uint32_t frameCount) {
    if (!buffer || !data || frameCount == 0) return 0;
    
    uint32_t availableFrames = AudioRingBuffer_GetAvailableFrames(buffer);
    uint32_t framesToPeek = (frameCount < availableFrames) ? frameCount : availableFrames;
    
    if (framesToPeek == 0) return 0;
    
    uint64_t readIdx = atomic_load(&buffer->readIndex);
    uint32_t bufferMask = buffer->bufferSize - 1;
    uint32_t channelCount = buffer->channelCount;
    
    atomic_thread_fence(memory_order_acquire);
    
    for (uint32_t i = 0; i < framesToPeek; i++) {
        uint32_t bufferPos = (uint32_t)((readIdx + i) & bufferMask);
        size_t bufferOffset = (size_t)bufferPos * channelCount;
        size_t dataOffset = (size_t)i * channelCount;
        
        for (uint32_t ch = 0; ch < channelCount; ch++) {
            data[dataOffset + ch] = buffer->buffer[bufferOffset + ch];
        }
    }
    
    return framesToPeek;
}

void AudioRingBuffer_Skip(AudioRingBuffer* buffer, uint32_t frameCount) {
    if (!buffer || frameCount == 0) return;
    
    uint32_t availableFrames = AudioRingBuffer_GetAvailableFrames(buffer);
    uint32_t framesToSkip = (frameCount < availableFrames) ? frameCount : availableFrames;
    
    if (framesToSkip > 0) {
        uint64_t readIdx = atomic_load(&buffer->readIndex);
        atomic_store(&buffer->readIndex, readIdx + framesToSkip);
    }
}

