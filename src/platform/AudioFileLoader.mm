#import "AudioFileLoader.h"
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>

bool loadAudioFile(const std::string& path, DecodedAudio& out)
{
    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    NSURL*    url    = [NSURL fileURLWithPath:nsPath];

    ExtAudioFileRef af = nullptr;
    if (ExtAudioFileOpenURL((__bridge CFURLRef)url, &af) != noErr || !af)
        return false;

    // Query source format for channel count and sample rate
    AudioStreamBasicDescription srcFmt{};
    UInt32 propSize = sizeof(srcFmt);
    if (ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileDataFormat,
                                &propSize, &srcFmt) != noErr) {
        ExtAudioFileDispose(af);
        return false;
    }

    const int channels = std::min((int)srcFmt.mChannelsPerFrame, 2);

    // Request interleaved 16-bit signed integer PCM at source sample rate
    AudioStreamBasicDescription outFmt{};
    outFmt.mSampleRate       = srcFmt.mSampleRate;
    outFmt.mFormatID         = kAudioFormatLinearPCM;
    outFmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    outFmt.mBitsPerChannel   = 16;
    outFmt.mChannelsPerFrame = (UInt32)channels;
    outFmt.mBytesPerFrame    = (UInt32)(2 * channels);
    outFmt.mFramesPerPacket  = 1;
    outFmt.mBytesPerPacket   = outFmt.mBytesPerFrame;

    if (ExtAudioFileSetProperty(af, kExtAudioFileProperty_ClientDataFormat,
                                sizeof(outFmt), &outFmt) != noErr) {
        ExtAudioFileDispose(af);
        return false;
    }

    // Query frame count
    SInt64 frameCount = 0;
    propSize = sizeof(frameCount);
    ExtAudioFileGetProperty(af, kExtAudioFileProperty_FileLengthFrames,
                            &propSize, &frameCount);

    out.pcm.clear();
    out.sampleRate = (uint32_t)srcFmt.mSampleRate;
    out.channels   = channels;

    // Read in chunks
    static constexpr UInt32 kChunk = 4096;
    std::vector<int16_t> buf(kChunk * (size_t)channels);

    while (true) {
        AudioBufferList abl;
        abl.mNumberBuffers              = 1;
        abl.mBuffers[0].mNumberChannels = (UInt32)channels;
        abl.mBuffers[0].mDataByteSize   = (UInt32)(buf.size() * sizeof(int16_t));
        abl.mBuffers[0].mData           = buf.data();

        UInt32 frames = kChunk;
        if (ExtAudioFileRead(af, &frames, &abl) != noErr || frames == 0)
            break;

        out.pcm.insert(out.pcm.end(), buf.begin(), buf.begin() + frames * (size_t)channels);
    }

    ExtAudioFileDispose(af);
    return !out.pcm.empty();
}
