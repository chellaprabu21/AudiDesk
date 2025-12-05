/*
 *  AudiDeckDriver.cpp
 *  AudiDeck Virtual Audio Driver
 *
 *  A HAL audio plugin that creates a virtual audio device for audio routing
 *  Based on Apple's AudioServerPlugIn architecture
 */

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>

// ============================================================================
// Constants
// ============================================================================

#define kPlugIn_BundleID            "com.audideck.driver"
#define kDevice_UID                 "AudiDeck_VirtualDevice"
#define kDevice_ModelUID            "AudiDeck_Model"
#define kDevice_Name                "AudiDeck Virtual Output"
#define kDevice_Manufacturer        "AudiDeck"

#define kDevice_SampleRate          48000.0
#define kDevice_ChannelCount        2
#define kDevice_BufferSize          512
#define kDevice_RingBufferSize      (48000 * 2)  // 2 seconds

// Object IDs - must be unique and > 0
enum {
    kObjectID_PlugIn                = 1,
    kObjectID_Device                = 2,
    kObjectID_Stream_Output         = 3,
    kObjectID_Stream_Input          = 4,
    kObjectID_Volume_Master         = 5,
    kObjectID_Mute_Master           = 6
};

// ============================================================================
// Ring Buffer (Lock-Free)
// ============================================================================

class RingBuffer {
public:
    RingBuffer(uint32_t frames, uint32_t channels) 
        : mFrames(frames), mChannels(channels), mWritePos(0), mReadPos(0) {
        mBuffer = new float[frames * channels]();
    }
    
    ~RingBuffer() { delete[] mBuffer; }
    
    void reset() {
        mWritePos.store(0);
        mReadPos.store(0);
        memset(mBuffer, 0, mFrames * mChannels * sizeof(float));
    }
    
    uint32_t write(const float* data, uint32_t frames) {
        uint64_t wp = mWritePos.load();
        uint64_t rp = mReadPos.load();
        uint32_t available = mFrames - (uint32_t)(wp - rp);
        uint32_t toWrite = std::min(frames, available);
        
        for (uint32_t i = 0; i < toWrite; i++) {
            uint32_t idx = (wp + i) % mFrames;
            for (uint32_t c = 0; c < mChannels; c++) {
                mBuffer[idx * mChannels + c] = data[i * mChannels + c];
            }
        }
        mWritePos.store(wp + toWrite);
        return toWrite;
    }
    
    uint32_t read(float* data, uint32_t frames) {
        uint64_t wp = mWritePos.load();
        uint64_t rp = mReadPos.load();
        uint32_t available = (uint32_t)(wp - rp);
        uint32_t toRead = std::min(frames, available);
        
        for (uint32_t i = 0; i < toRead; i++) {
            uint32_t idx = (rp + i) % mFrames;
            for (uint32_t c = 0; c < mChannels; c++) {
                data[i * mChannels + c] = mBuffer[idx * mChannels + c];
            }
        }
        // Fill rest with silence
        for (uint32_t i = toRead; i < frames; i++) {
            for (uint32_t c = 0; c < mChannels; c++) {
                data[i * mChannels + c] = 0.0f;
            }
        }
        mReadPos.store(rp + toRead);
        return toRead;
    }
    
private:
    float* mBuffer;
    uint32_t mFrames;
    uint32_t mChannels;
    std::atomic<uint64_t> mWritePos;
    std::atomic<uint64_t> mReadPos;
};

// ============================================================================
// Plugin State
// ============================================================================

struct PlugInState {
    AudioServerPlugInHostRef host = nullptr;
    
    Float64 sampleRate = kDevice_SampleRate;
    std::atomic<bool> isRunning{false};
    std::atomic<UInt32> clientCount{0};
    
    UInt64 anchorHostTime = 0;
    std::atomic<UInt64> timestampCounter{0};
    
    std::atomic<Float32> volume{1.0f};
    std::atomic<bool> muted{false};
    
    std::unique_ptr<RingBuffer> ringBuffer;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
};

static PlugInState* gState = nullptr;
static mach_timebase_info_data_t gTimebase = {0, 0};

// ============================================================================
// Forward Declarations
// ============================================================================

static HRESULT Plugin_QueryInterface(void* driver, REFIID iid, LPVOID* ppv);
static ULONG Plugin_AddRef(void* driver);
static ULONG Plugin_Release(void* driver);
static OSStatus Plugin_Initialize(AudioServerPlugInDriverRef driver, AudioServerPlugInHostRef host);
static OSStatus Plugin_CreateDevice(AudioServerPlugInDriverRef driver, CFDictionaryRef desc, const AudioServerPlugInClientInfo* clientInfo, AudioObjectID* outID);
static OSStatus Plugin_DestroyDevice(AudioServerPlugInDriverRef driver, AudioObjectID deviceID);
static OSStatus Plugin_AddDeviceClient(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, const AudioServerPlugInClientInfo* clientInfo);
static OSStatus Plugin_RemoveDeviceClient(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, const AudioServerPlugInClientInfo* clientInfo);
static OSStatus Plugin_PerformConfigChange(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt64 action, void* info);
static OSStatus Plugin_AbortConfigChange(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt64 action, void* info);
static Boolean Plugin_HasProperty(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address);
static OSStatus Plugin_IsPropertySettable(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, Boolean* outSettable);
static OSStatus Plugin_GetPropertyDataSize(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32* outSize);
static OSStatus Plugin_GetPropertyData(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32 inSize, UInt32* outSize, void* outData);
static OSStatus Plugin_SetPropertyData(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32 dataSize, const void* data);
static OSStatus Plugin_StartIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID);
static OSStatus Plugin_StopIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID);
static OSStatus Plugin_GetZeroTimeStamp(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, Float64* outSampleTime, UInt64* outHostTime, UInt64* outSeed);
static OSStatus Plugin_WillDoIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, Boolean* outWillDo, Boolean* outWillDoInPlace);
static OSStatus Plugin_BeginIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo);
static OSStatus Plugin_DoIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, AudioObjectID streamID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo, void* mainBuffer, void* secondaryBuffer);
static OSStatus Plugin_EndIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo);

// ============================================================================
// Plugin Interface (COM-style)
// ============================================================================

static AudioServerPlugInDriverInterface gInterface = {
    // _reserved
    NULL,
    // COM methods
    Plugin_QueryInterface,
    Plugin_AddRef,
    Plugin_Release,
    // Plugin methods
    Plugin_Initialize,
    Plugin_CreateDevice,
    Plugin_DestroyDevice,
    Plugin_AddDeviceClient,
    Plugin_RemoveDeviceClient,
    Plugin_PerformConfigChange,
    Plugin_AbortConfigChange,
    Plugin_HasProperty,
    Plugin_IsPropertySettable,
    Plugin_GetPropertyDataSize,
    Plugin_GetPropertyData,
    Plugin_SetPropertyData,
    Plugin_StartIO,
    Plugin_StopIO,
    Plugin_GetZeroTimeStamp,
    Plugin_WillDoIO,
    Plugin_BeginIO,
    Plugin_DoIO,
    Plugin_EndIO
};

static AudioServerPlugInDriverInterface* gInterfacePtr = &gInterface;
static AudioServerPlugInDriverRef gDriverRef = &gInterfacePtr;

// ============================================================================
// Entry Point
// ============================================================================

extern "C" void* AudiDeckDriverCreate(CFAllocatorRef allocator, CFUUIDRef typeUUID) {
    if (!CFEqual(typeUUID, kAudioServerPlugInTypeUUID)) {
        return nullptr;
    }
    
    if (!gState) {
        gState = new PlugInState();
        gState->ringBuffer = std::make_unique<RingBuffer>(kDevice_RingBufferSize, kDevice_ChannelCount);
        mach_timebase_info(&gTimebase);
    }
    
    return gDriverRef;
}

// ============================================================================
// COM Methods
// ============================================================================

static HRESULT Plugin_QueryInterface(void* driver, REFIID iid, LPVOID* ppv) {
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);
    CFUUIDRef pluginUUID = CFUUIDGetConstantUUIDWithBytes(NULL, 
        0x44, 0x3A, 0xBA, 0xB8, 0xE7, 0xB3, 0x49, 0x1A,
        0xB9, 0x85, 0xBE, 0xB9, 0x18, 0x70, 0x30, 0xDB);
    CFUUIDRef unknownUUID = CFUUIDGetConstantUUIDWithBytes(NULL,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
    
    Boolean match = CFEqual(interfaceID, pluginUUID) || CFEqual(interfaceID, unknownUUID);
    CFRelease(interfaceID);
    
    if (match) {
        Plugin_AddRef(driver);
        *ppv = driver;
        return kAudioHardwareNoError;
    }
    
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG Plugin_AddRef(void* driver) {
    return 1;  // Singleton, never deallocated
}

static ULONG Plugin_Release(void* driver) {
    return 1;  // Singleton, never deallocated
}

// ============================================================================
// Plugin Lifecycle
// ============================================================================

static OSStatus Plugin_Initialize(AudioServerPlugInDriverRef driver, AudioServerPlugInHostRef host) {
    gState->host = host;
    return kAudioHardwareNoError;
}

static OSStatus Plugin_CreateDevice(AudioServerPlugInDriverRef driver, CFDictionaryRef desc, const AudioServerPlugInClientInfo* clientInfo, AudioObjectID* outID) {
    return kAudioHardwareUnsupportedOperationError;
}

static OSStatus Plugin_DestroyDevice(AudioServerPlugInDriverRef driver, AudioObjectID deviceID) {
    return kAudioHardwareUnsupportedOperationError;
}

static OSStatus Plugin_AddDeviceClient(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, const AudioServerPlugInClientInfo* clientInfo) {
    return kAudioHardwareNoError;
}

static OSStatus Plugin_RemoveDeviceClient(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, const AudioServerPlugInClientInfo* clientInfo) {
    return kAudioHardwareNoError;
}

static OSStatus Plugin_PerformConfigChange(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt64 action, void* info) {
    return kAudioHardwareNoError;
}

static OSStatus Plugin_AbortConfigChange(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt64 action, void* info) {
    return kAudioHardwareNoError;
}

// ============================================================================
// Property Queries
// ============================================================================

static Boolean Plugin_HasProperty(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address) {
    switch (objectID) {
        case kObjectID_PlugIn:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyManufacturer:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioPlugInPropertyDeviceList:
                case kAudioPlugInPropertyTranslateUIDToDevice:
                case kAudioPlugInPropertyResourceBundle:
                    return true;
            }
            break;
            
        case kObjectID_Device:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioObjectPropertyName:
                case kAudioObjectPropertyManufacturer:
                case kAudioObjectPropertyOwnedObjects:
                case kAudioDevicePropertyDeviceUID:
                case kAudioDevicePropertyModelUID:
                case kAudioDevicePropertyTransportType:
                case kAudioDevicePropertyRelatedDevices:
                case kAudioDevicePropertyClockDomain:
                case kAudioDevicePropertyDeviceIsAlive:
                case kAudioDevicePropertyDeviceIsRunning:
                case kAudioDevicePropertyDeviceCanBeDefaultDevice:
                case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                case kAudioDevicePropertyLatency:
                case kAudioDevicePropertyStreams:
                case kAudioObjectPropertyControlList:
                case kAudioDevicePropertySafetyOffset:
                case kAudioDevicePropertyNominalSampleRate:
                case kAudioDevicePropertyAvailableNominalSampleRates:
                case kAudioDevicePropertyIsHidden:
                case kAudioDevicePropertyZeroTimeStampPeriod:
                    return true;
            }
            break;
            
        case kObjectID_Stream_Output:
        case kObjectID_Stream_Input:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioStreamPropertyIsActive:
                case kAudioStreamPropertyDirection:
                case kAudioStreamPropertyTerminalType:
                case kAudioStreamPropertyStartingChannel:
                case kAudioStreamPropertyLatency:
                case kAudioStreamPropertyVirtualFormat:
                case kAudioStreamPropertyPhysicalFormat:
                case kAudioStreamPropertyAvailableVirtualFormats:
                case kAudioStreamPropertyAvailablePhysicalFormats:
                    return true;
            }
            break;
            
        case kObjectID_Volume_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioLevelControlPropertyScalarValue:
                case kAudioLevelControlPropertyDecibelValue:
                case kAudioLevelControlPropertyDecibelRange:
                    return true;
            }
            break;
            
        case kObjectID_Mute_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioBooleanControlPropertyValue:
                    return true;
            }
            break;
    }
    return false;
}

static OSStatus Plugin_IsPropertySettable(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, Boolean* outSettable) {
    *outSettable = false;
    
    switch (objectID) {
        case kObjectID_Device:
            if (address->mSelector == kAudioDevicePropertyNominalSampleRate) {
                *outSettable = true;
            }
            break;
        case kObjectID_Volume_Master:
            if (address->mSelector == kAudioLevelControlPropertyScalarValue ||
                address->mSelector == kAudioLevelControlPropertyDecibelValue) {
                *outSettable = true;
            }
            break;
        case kObjectID_Mute_Master:
            if (address->mSelector == kAudioBooleanControlPropertyValue) {
                *outSettable = true;
            }
            break;
    }
    return kAudioHardwareNoError;
}

static OSStatus Plugin_GetPropertyDataSize(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32* outSize) {
    *outSize = 0;
    
    switch (objectID) {
        case kObjectID_PlugIn:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyManufacturer:
                case kAudioPlugInPropertyResourceBundle:
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioObjectPropertyOwnedObjects:
                case kAudioPlugInPropertyDeviceList:
                case kAudioPlugInPropertyTranslateUIDToDevice:
                    *outSize = sizeof(AudioObjectID);
                    break;
            }
            break;
            
        case kObjectID_Device:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyName:
                case kAudioObjectPropertyManufacturer:
                case kAudioDevicePropertyDeviceUID:
                case kAudioDevicePropertyModelUID:
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioDevicePropertyTransportType:
                case kAudioDevicePropertyClockDomain:
                case kAudioDevicePropertyLatency:
                case kAudioDevicePropertySafetyOffset:
                case kAudioDevicePropertyZeroTimeStampPeriod:
                case kAudioDevicePropertyDeviceIsAlive:
                case kAudioDevicePropertyDeviceIsRunning:
                case kAudioDevicePropertyDeviceCanBeDefaultDevice:
                case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                case kAudioDevicePropertyIsHidden:
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioObjectPropertyOwnedObjects:
                    *outSize = sizeof(AudioObjectID) * 4;
                    break;
                case kAudioDevicePropertyRelatedDevices:
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioDevicePropertyStreams:
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioObjectPropertyControlList:
                    *outSize = sizeof(AudioObjectID) * 2;
                    break;
                case kAudioDevicePropertyNominalSampleRate:
                    *outSize = sizeof(Float64);
                    break;
                case kAudioDevicePropertyAvailableNominalSampleRates:
                    *outSize = sizeof(AudioValueRange) * 1;
                    break;
            }
            break;
            
        case kObjectID_Stream_Output:
        case kObjectID_Stream_Input:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioStreamPropertyIsActive:
                case kAudioStreamPropertyDirection:
                case kAudioStreamPropertyTerminalType:
                case kAudioStreamPropertyStartingChannel:
                case kAudioStreamPropertyLatency:
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyVirtualFormat:
                case kAudioStreamPropertyPhysicalFormat:
                    *outSize = sizeof(AudioStreamBasicDescription);
                    break;
                case kAudioStreamPropertyAvailableVirtualFormats:
                case kAudioStreamPropertyAvailablePhysicalFormats:
                    *outSize = sizeof(AudioStreamRangedDescription);
                    break;
            }
            break;
            
        case kObjectID_Volume_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioLevelControlPropertyScalarValue:
                case kAudioLevelControlPropertyDecibelValue:
                    *outSize = sizeof(Float32);
                    break;
                case kAudioLevelControlPropertyDecibelRange:
                    *outSize = sizeof(AudioValueRange);
                    break;
            }
            break;
            
        case kObjectID_Mute_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                case kAudioObjectPropertyClass:
                case kAudioObjectPropertyOwner:
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioControlPropertyScope:
                case kAudioControlPropertyElement:
                case kAudioBooleanControlPropertyValue:
                    *outSize = sizeof(UInt32);
                    break;
            }
            break;
    }
    
    return (*outSize > 0) ? kAudioHardwareNoError : kAudioHardwareUnknownPropertyError;
}

static OSStatus Plugin_GetPropertyData(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32 inSize, UInt32* outSize, void* outData) {
    
    switch (objectID) {
        case kObjectID_PlugIn:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *((AudioClassID*)outData) = kAudioObjectClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyClass:
                    *((AudioClassID*)outData) = kAudioPlugInClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyOwner:
                    *((AudioObjectID*)outData) = kAudioObjectUnknown;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioObjectPropertyManufacturer:
                    *((CFStringRef*)outData) = CFSTR(kDevice_Manufacturer);
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioObjectPropertyOwnedObjects:
                case kAudioPlugInPropertyDeviceList:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioPlugInPropertyTranslateUIDToDevice:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioPlugInPropertyResourceBundle:
                    *((CFStringRef*)outData) = CFSTR("");
                    *outSize = sizeof(CFStringRef);
                    break;
                default:
                    return kAudioHardwareUnknownPropertyError;
            }
            break;
            
        case kObjectID_Device:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *((AudioClassID*)outData) = kAudioObjectClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyClass:
                    *((AudioClassID*)outData) = kAudioDeviceClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyOwner:
                    *((AudioObjectID*)outData) = kObjectID_PlugIn;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioObjectPropertyName:
                    *((CFStringRef*)outData) = CFSTR(kDevice_Name);
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioObjectPropertyManufacturer:
                    *((CFStringRef*)outData) = CFSTR(kDevice_Manufacturer);
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioDevicePropertyDeviceUID:
                    *((CFStringRef*)outData) = CFSTR(kDevice_UID);
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioDevicePropertyModelUID:
                    *((CFStringRef*)outData) = CFSTR(kDevice_ModelUID);
                    *outSize = sizeof(CFStringRef);
                    break;
                case kAudioDevicePropertyTransportType:
                    *((UInt32*)outData) = kAudioDeviceTransportTypeVirtual;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyRelatedDevices:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioDevicePropertyClockDomain:
                    *((UInt32*)outData) = 0;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyDeviceIsAlive:
                    *((UInt32*)outData) = 1;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyDeviceIsRunning:
                    *((UInt32*)outData) = gState->isRunning ? 1 : 0;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyDeviceCanBeDefaultDevice:
                case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                    *((UInt32*)outData) = 1;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyLatency:
                case kAudioDevicePropertySafetyOffset:
                    *((UInt32*)outData) = 0;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyZeroTimeStampPeriod:
                    *((UInt32*)outData) = kDevice_BufferSize;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioDevicePropertyIsHidden:
                    *((UInt32*)outData) = 0;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioObjectPropertyOwnedObjects: {
                    AudioObjectID* ids = (AudioObjectID*)outData;
                    ids[0] = kObjectID_Stream_Output;
                    ids[1] = kObjectID_Stream_Input;
                    ids[2] = kObjectID_Volume_Master;
                    ids[3] = kObjectID_Mute_Master;
                    *outSize = sizeof(AudioObjectID) * 4;
                    break;
                }
                case kAudioDevicePropertyStreams:
                    if (address->mScope == kAudioObjectPropertyScopeOutput) {
                        *((AudioObjectID*)outData) = kObjectID_Stream_Output;
                    } else {
                        *((AudioObjectID*)outData) = kObjectID_Stream_Input;
                    }
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioObjectPropertyControlList: {
                    AudioObjectID* ids = (AudioObjectID*)outData;
                    ids[0] = kObjectID_Volume_Master;
                    ids[1] = kObjectID_Mute_Master;
                    *outSize = sizeof(AudioObjectID) * 2;
                    break;
                }
                case kAudioDevicePropertyNominalSampleRate:
                    *((Float64*)outData) = gState->sampleRate;
                    *outSize = sizeof(Float64);
                    break;
                case kAudioDevicePropertyAvailableNominalSampleRates: {
                    AudioValueRange* range = (AudioValueRange*)outData;
                    range->mMinimum = kDevice_SampleRate;
                    range->mMaximum = kDevice_SampleRate;
                    *outSize = sizeof(AudioValueRange);
                    break;
                }
                default:
                    return kAudioHardwareUnknownPropertyError;
            }
            break;
            
        case kObjectID_Stream_Output:
        case kObjectID_Stream_Input: {
            bool isOutput = (objectID == kObjectID_Stream_Output);
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *((AudioClassID*)outData) = kAudioObjectClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyClass:
                    *((AudioClassID*)outData) = kAudioStreamClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyOwner:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioStreamPropertyIsActive:
                    *((UInt32*)outData) = 1;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyDirection:
                    *((UInt32*)outData) = isOutput ? 0 : 1;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyTerminalType:
                    *((UInt32*)outData) = isOutput ? kAudioStreamTerminalTypeSpeaker : kAudioStreamTerminalTypeMicrophone;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyStartingChannel:
                    *((UInt32*)outData) = 1;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyLatency:
                    *((UInt32*)outData) = 0;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioStreamPropertyVirtualFormat:
                case kAudioStreamPropertyPhysicalFormat: {
                    AudioStreamBasicDescription* desc = (AudioStreamBasicDescription*)outData;
                    desc->mSampleRate = gState->sampleRate;
                    desc->mFormatID = kAudioFormatLinearPCM;
                    desc->mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                    desc->mBytesPerPacket = kDevice_ChannelCount * sizeof(Float32);
                    desc->mFramesPerPacket = 1;
                    desc->mBytesPerFrame = kDevice_ChannelCount * sizeof(Float32);
                    desc->mChannelsPerFrame = kDevice_ChannelCount;
                    desc->mBitsPerChannel = 32;
                    *outSize = sizeof(AudioStreamBasicDescription);
                    break;
                }
                case kAudioStreamPropertyAvailableVirtualFormats:
                case kAudioStreamPropertyAvailablePhysicalFormats: {
                    AudioStreamRangedDescription* desc = (AudioStreamRangedDescription*)outData;
                    desc->mFormat.mSampleRate = gState->sampleRate;
                    desc->mFormat.mFormatID = kAudioFormatLinearPCM;
                    desc->mFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
                    desc->mFormat.mBytesPerPacket = kDevice_ChannelCount * sizeof(Float32);
                    desc->mFormat.mFramesPerPacket = 1;
                    desc->mFormat.mBytesPerFrame = kDevice_ChannelCount * sizeof(Float32);
                    desc->mFormat.mChannelsPerFrame = kDevice_ChannelCount;
                    desc->mFormat.mBitsPerChannel = 32;
                    desc->mSampleRateRange.mMinimum = gState->sampleRate;
                    desc->mSampleRateRange.mMaximum = gState->sampleRate;
                    *outSize = sizeof(AudioStreamRangedDescription);
                    break;
                }
                default:
                    return kAudioHardwareUnknownPropertyError;
            }
            break;
        }
        
        case kObjectID_Volume_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *((AudioClassID*)outData) = kAudioControlClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyClass:
                    *((AudioClassID*)outData) = kAudioVolumeControlClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyOwner:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioControlPropertyScope:
                    *((UInt32*)outData) = kAudioObjectPropertyScopeOutput;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioControlPropertyElement:
                    *((UInt32*)outData) = kAudioObjectPropertyElementMain;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioLevelControlPropertyScalarValue:
                    *((Float32*)outData) = gState->volume.load();
                    *outSize = sizeof(Float32);
                    break;
                case kAudioLevelControlPropertyDecibelValue: {
                    Float32 vol = gState->volume.load();
                    *((Float32*)outData) = (vol > 0) ? (20.0f * log10f(vol)) : -96.0f;
                    *outSize = sizeof(Float32);
                    break;
                }
                case kAudioLevelControlPropertyDecibelRange: {
                    AudioValueRange* range = (AudioValueRange*)outData;
                    range->mMinimum = -96.0;
                    range->mMaximum = 0.0;
                    *outSize = sizeof(AudioValueRange);
                    break;
                }
                default:
                    return kAudioHardwareUnknownPropertyError;
            }
            break;
            
        case kObjectID_Mute_Master:
            switch (address->mSelector) {
                case kAudioObjectPropertyBaseClass:
                    *((AudioClassID*)outData) = kAudioControlClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyClass:
                    *((AudioClassID*)outData) = kAudioMuteControlClassID;
                    *outSize = sizeof(AudioClassID);
                    break;
                case kAudioObjectPropertyOwner:
                    *((AudioObjectID*)outData) = kObjectID_Device;
                    *outSize = sizeof(AudioObjectID);
                    break;
                case kAudioControlPropertyScope:
                    *((UInt32*)outData) = kAudioObjectPropertyScopeOutput;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioControlPropertyElement:
                    *((UInt32*)outData) = kAudioObjectPropertyElementMain;
                    *outSize = sizeof(UInt32);
                    break;
                case kAudioBooleanControlPropertyValue:
                    *((UInt32*)outData) = gState->muted ? 1 : 0;
                    *outSize = sizeof(UInt32);
                    break;
                default:
                    return kAudioHardwareUnknownPropertyError;
            }
            break;
            
        default:
            return kAudioHardwareBadObjectError;
    }
    
    return kAudioHardwareNoError;
}

static OSStatus Plugin_SetPropertyData(AudioServerPlugInDriverRef driver, AudioObjectID objectID, pid_t clientPID, const AudioObjectPropertyAddress* address, UInt32 qualifierSize, const void* qualifier, UInt32 dataSize, const void* data) {
    
    switch (objectID) {
        case kObjectID_Volume_Master:
            if (address->mSelector == kAudioLevelControlPropertyScalarValue) {
                gState->volume.store(*((Float32*)data));
            }
            break;
        case kObjectID_Mute_Master:
            if (address->mSelector == kAudioBooleanControlPropertyValue) {
                gState->muted.store(*((UInt32*)data) != 0);
            }
            break;
    }
    
    return kAudioHardwareNoError;
}

// ============================================================================
// IO Operations
// ============================================================================

static OSStatus Plugin_StartIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID) {
    pthread_mutex_lock(&gState->mutex);
    
    if (gState->clientCount.fetch_add(1) == 0) {
        gState->isRunning.store(true);
        gState->anchorHostTime = mach_absolute_time();
        gState->timestampCounter.store(0);
        gState->ringBuffer->reset();
    }
    
    pthread_mutex_unlock(&gState->mutex);
    return kAudioHardwareNoError;
}

static OSStatus Plugin_StopIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID) {
    pthread_mutex_lock(&gState->mutex);
    
    if (gState->clientCount.fetch_sub(1) == 1) {
        gState->isRunning.store(false);
    }
    
    pthread_mutex_unlock(&gState->mutex);
    return kAudioHardwareNoError;
}

static OSStatus Plugin_GetZeroTimeStamp(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, Float64* outSampleTime, UInt64* outHostTime, UInt64* outSeed) {
    UInt64 currentTime = mach_absolute_time();
    Float64 elapsedNanos = (Float64)(currentTime - gState->anchorHostTime) * gTimebase.numer / gTimebase.denom;
    Float64 elapsedSamples = elapsedNanos * gState->sampleRate / 1000000000.0;
    
    UInt64 cycles = (UInt64)(elapsedSamples / kDevice_BufferSize);
    
    *outSampleTime = cycles * kDevice_BufferSize;
    *outHostTime = gState->anchorHostTime + (UInt64)(cycles * kDevice_BufferSize / gState->sampleRate * 1000000000.0 * gTimebase.denom / gTimebase.numer);
    *outSeed = gState->timestampCounter.load();
    
    return kAudioHardwareNoError;
}

static OSStatus Plugin_WillDoIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, Boolean* outWillDo, Boolean* outWillDoInPlace) {
    *outWillDo = (operationID == kAudioServerPlugInIOOperationReadInput || 
                  operationID == kAudioServerPlugInIOOperationWriteMix);
    *outWillDoInPlace = true;
    return kAudioHardwareNoError;
}

static OSStatus Plugin_BeginIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo) {
    return kAudioHardwareNoError;
}

static OSStatus Plugin_DoIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, AudioObjectID streamID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo, void* mainBuffer, void* secondaryBuffer) {
    Float32* buffer = (Float32*)mainBuffer;
    
    if (operationID == kAudioServerPlugInIOOperationWriteMix) {
        // Apps writing audio to our device
        gState->ringBuffer->write(buffer, bufferFrames);
    } 
    else if (operationID == kAudioServerPlugInIOOperationReadInput) {
        // Apps reading audio from our device (loopback)
        gState->ringBuffer->read(buffer, bufferFrames);
        
        // Apply volume & mute
        if (gState->muted.load()) {
            memset(buffer, 0, bufferFrames * kDevice_ChannelCount * sizeof(Float32));
        } else {
            Float32 vol = gState->volume.load();
            if (vol != 1.0f) {
                for (UInt32 i = 0; i < bufferFrames * kDevice_ChannelCount; i++) {
                    buffer[i] *= vol;
                }
            }
        }
    }
    
    return kAudioHardwareNoError;
}

static OSStatus Plugin_EndIO(AudioServerPlugInDriverRef driver, AudioObjectID deviceID, UInt32 clientID, UInt32 operationID, UInt32 bufferFrames, const AudioServerPlugInIOCycleInfo* cycleInfo) {
    return kAudioHardwareNoError;
}

