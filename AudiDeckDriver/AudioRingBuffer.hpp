/*
 *  AudioRingBuffer.hpp
 *  AudioRouterDriver
 *
 *  C++ Lock-free ring buffer for audio data transfer
 */

#ifndef AudioRingBuffer_hpp
#define AudioRingBuffer_hpp

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>

class AudioRingBuffer {
public:
    AudioRingBuffer(uint32_t bufferSizeFrames, uint32_t channelCount)
        : mChannelCount(channelCount)
        , mWriteIndex(0)
        , mReadIndex(0)
    {
        // Round up to power of 2 for efficient modulo
        uint32_t size = 1;
        while (size < bufferSizeFrames) {
            size <<= 1;
        }
        mBufferSize = size;
        mBufferMask = size - 1;
        
        mBuffer = std::make_unique<float[]>(static_cast<size_t>(size) * channelCount);
        std::memset(mBuffer.get(), 0, static_cast<size_t>(size) * channelCount * sizeof(float));
    }
    
    void reset() {
        mWriteIndex.store(0, std::memory_order_relaxed);
        mReadIndex.store(0, std::memory_order_relaxed);
        std::memset(mBuffer.get(), 0, static_cast<size_t>(mBufferSize) * mChannelCount * sizeof(float));
    }
    
    uint32_t availableFrames() const {
        uint64_t write = mWriteIndex.load(std::memory_order_acquire);
        uint64_t read = mReadIndex.load(std::memory_order_acquire);
        return static_cast<uint32_t>(write - read);
    }
    
    uint32_t freeFrames() const {
        return mBufferSize - availableFrames();
    }
    
    uint32_t write(const float* data, uint32_t frameCount) {
        uint32_t free = freeFrames();
        uint32_t toWrite = (frameCount < free) ? frameCount : free;
        
        if (toWrite == 0) return 0;
        
        uint64_t writeIdx = mWriteIndex.load(std::memory_order_relaxed);
        
        for (uint32_t i = 0; i < toWrite; i++) {
            uint32_t bufferPos = static_cast<uint32_t>((writeIdx + i) & mBufferMask);
            size_t bufferOffset = static_cast<size_t>(bufferPos) * mChannelCount;
            size_t dataOffset = static_cast<size_t>(i) * mChannelCount;
            
            for (uint32_t ch = 0; ch < mChannelCount; ch++) {
                mBuffer[bufferOffset + ch] = data[dataOffset + ch];
            }
        }
        
        std::atomic_thread_fence(std::memory_order_release);
        mWriteIndex.store(writeIdx + toWrite, std::memory_order_release);
        
        return toWrite;
    }
    
    uint32_t read(float* data, uint32_t frameCount) {
        uint32_t available = availableFrames();
        uint32_t toRead = (frameCount < available) ? frameCount : available;
        
        if (toRead == 0) {
            // Fill with silence
            std::memset(data, 0, static_cast<size_t>(frameCount) * mChannelCount * sizeof(float));
            return 0;
        }
        
        uint64_t readIdx = mReadIndex.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        
        for (uint32_t i = 0; i < toRead; i++) {
            uint32_t bufferPos = static_cast<uint32_t>((readIdx + i) & mBufferMask);
            size_t bufferOffset = static_cast<size_t>(bufferPos) * mChannelCount;
            size_t dataOffset = static_cast<size_t>(i) * mChannelCount;
            
            for (uint32_t ch = 0; ch < mChannelCount; ch++) {
                data[dataOffset + ch] = mBuffer[bufferOffset + ch];
            }
        }
        
        mReadIndex.store(readIdx + toRead, std::memory_order_release);
        
        // Fill remaining with silence
        if (toRead < frameCount) {
            size_t offset = static_cast<size_t>(toRead) * mChannelCount;
            size_t remaining = static_cast<size_t>(frameCount - toRead) * mChannelCount;
            std::memset(data + offset, 0, remaining * sizeof(float));
        }
        
        return toRead;
    }
    
private:
    std::unique_ptr<float[]> mBuffer;
    uint32_t mBufferSize;
    uint32_t mBufferMask;
    uint32_t mChannelCount;
    std::atomic<uint64_t> mWriteIndex;
    std::atomic<uint64_t> mReadIndex;
};

#endif /* AudioRingBuffer_hpp */

