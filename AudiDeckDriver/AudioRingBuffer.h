/*
 *  AudioRingBuffer.h
 *  AudioRouterDriver
 *
 *  Lock-free ring buffer for audio data transfer
 */

#ifndef AudioRingBuffer_h
#define AudioRingBuffer_h

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ring buffer structure
typedef struct {
    float* buffer;                      // The actual audio data
    uint32_t bufferSize;               // Total size in frames
    uint32_t channelCount;             // Number of audio channels
    atomic_uint_fast64_t writeIndex;   // Write position (frames)
    atomic_uint_fast64_t readIndex;    // Read position (frames)
} AudioRingBuffer;

// Create a new ring buffer
// bufferSizeFrames: Size in audio frames (not bytes)
// channelCount: Number of audio channels
AudioRingBuffer* AudioRingBuffer_Create(uint32_t bufferSizeFrames, uint32_t channelCount);

// Destroy a ring buffer
void AudioRingBuffer_Destroy(AudioRingBuffer* buffer);

// Reset the buffer (clear all data)
void AudioRingBuffer_Reset(AudioRingBuffer* buffer);

// Get the number of frames available for reading
uint32_t AudioRingBuffer_GetAvailableFrames(AudioRingBuffer* buffer);

// Get the number of frames available for writing
uint32_t AudioRingBuffer_GetFreeFrames(AudioRingBuffer* buffer);

// Write frames to the buffer
// Returns the number of frames actually written
uint32_t AudioRingBuffer_Write(AudioRingBuffer* buffer, const float* data, uint32_t frameCount);

// Read frames from the buffer
// Returns the number of frames actually read
uint32_t AudioRingBuffer_Read(AudioRingBuffer* buffer, float* data, uint32_t frameCount);

// Peek at frames without consuming them
// Returns the number of frames actually peeked
uint32_t AudioRingBuffer_Peek(AudioRingBuffer* buffer, float* data, uint32_t frameCount);

// Skip frames (advance read position without reading data)
void AudioRingBuffer_Skip(AudioRingBuffer* buffer, uint32_t frameCount);

#ifdef __cplusplus
}
#endif

#endif /* AudioRingBuffer_h */

