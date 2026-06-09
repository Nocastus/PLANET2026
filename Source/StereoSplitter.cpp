#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct RiffHeader
{
    char     riff[4];
    uint32_t fileSize;
    char     wave[4];
};

struct FmtChunk
{
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct BextChunk
{
    char     description[256];
    char     originator[32];
    char     originatorRef[32];
    char     originationDate[10];
    char     originationTime[8];
    uint32_t timeRefLow;
    uint32_t timeRefHigh;
    uint16_t version;
};

struct SmplChunk
{
    uint32_t manufacturer;
    uint32_t product;
    uint32_t samplePeriod;
    uint32_t midiUnityNote;
    uint32_t midiPitchFraction;
    uint32_t smpteFormat;
    uint32_t smpteOffset;
    uint32_t numSampleLoops;
    uint32_t samplerDataSize;
};

struct SmplLoop
{
    uint32_t id;
    uint32_t type;
    uint32_t start;
    uint32_t end;
    uint32_t fraction;
    uint32_t playCount;
};

struct InstChunk
{
    int8_t   unshiftedNote;
    int8_t   fineTune;
    int8_t   gain;
    uint8_t  lowNote;
    uint8_t  highNote;
    uint8_t  lowVelocity;
    uint8_t  highVelocity;
};

struct CuePoint
{
    uint32_t id;
    uint32_t position;
    char     dataChunkId[4];
    uint32_t chunkStart;
    uint32_t blockStart;
    uint32_t sampleOffset;
};

struct AcidChunk
{
    uint32_t flags;
    uint16_t rootNote;
    uint16_t unknown1;
    float    unknown2;
    uint32_t numBeats;
    uint16_t meterDenom;
    uint16_t meterNumer;
    float    tempo;
};
#pragma pack(pop)

static void waitForKeypress()
{
    printf("\nPress any key to exit...\n");
    getchar();
}

static int exitWithError(const char* msg, FILE* f1 = nullptr, FILE* f2 = nullptr, FILE* f3 = nullptr)
{
    printf("%s\n", msg);
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    if (f3) fclose(f3);
    waitForKeypress();
    return 1;
}

// Read a fixed-length string field, trimming trailing spaces/nulls
static std::string readFixedString(const char* src, size_t maxLen)
{
    size_t len = 0;
    while (len < maxLen && src[len] != '\0') len++;
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\0')) len--;
    return std::string(src, len);
}

// Read a null-terminated string from file, up to maxLen bytes
static std::string readStringFromFile(FILE* f, uint32_t maxLen)
{
    std::vector<char> buf(maxLen + 1, 0);
    fread(buf.data(), 1, maxLen, f);
    // Trim trailing spaces/nulls
    size_t len = strlen(buf.data());
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\0')) len--;
    return std::string(buf.data(), len);
}

static const char* noteName(int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static char buf[16];
    if (midiNote < 0 || midiNote > 127) { sprintf(buf, "%d", midiNote); return buf; }
    sprintf(buf, "%s%d", names[midiNote % 12], (midiNote / 12) - 1);
    return buf;
}

static const char* loopTypeName(uint32_t type)
{
    switch (type)
    {
        case 0: return "Forward";
        case 1: return "Ping-Pong";
        case 2: return "Reverse";
        default: return "Unknown";
    }
}

static void printListInfo(FILE* f, uint32_t chunkSize)
{
    char listType[4];
    if (fread(listType, 4, 1, f) != 1) return;

    if (memcmp(listType, "INFO", 4) != 0) return;

    printf("\n--- INFO Metadata ---\n");

    uint32_t bytesRead = 4;
    while (bytesRead < chunkSize)
    {
        char subId[4];
        uint32_t subSize;
        if (fread(subId, 4, 1, f) != 1 || fread(&subSize, 4, 1, f) != 1) break;
        bytesRead += 8;

        std::string value = readStringFromFile(f, subSize);
        bytesRead += subSize;

        // Pad to even
        if (subSize & 1) { fseek(f, 1, SEEK_CUR); bytesRead++; }

        if (value.empty()) continue;

        const char* label = "Unknown";
        if      (memcmp(subId, "INAM", 4) == 0) label = "Title";
        else if (memcmp(subId, "IART", 4) == 0) label = "Artist";
        else if (memcmp(subId, "IPRD", 4) == 0) label = "Album";
        else if (memcmp(subId, "IGNR", 4) == 0) label = "Genre";
        else if (memcmp(subId, "ICRD", 4) == 0) label = "Date";
        else if (memcmp(subId, "ICMT", 4) == 0) label = "Comment";
        else if (memcmp(subId, "ICOP", 4) == 0) label = "Copyright";
        else if (memcmp(subId, "ISFT", 4) == 0) label = "Software";
        else if (memcmp(subId, "IENG", 4) == 0) label = "Engineer";
        else if (memcmp(subId, "ITCH", 4) == 0) label = "Technician";
        else if (memcmp(subId, "ISRC", 4) == 0) label = "Source";
        else if (memcmp(subId, "ISBJ", 4) == 0) label = "Subject";
        else if (memcmp(subId, "IKEY", 4) == 0) label = "Keywords";
        else if (memcmp(subId, "IMED", 4) == 0) label = "Medium";
        else { label = nullptr; printf("  %.4s: %s\n", subId, value.c_str()); }

        if (label) printf("  %s: %s\n", label, value.c_str());
    }
}

static void printBext(FILE* f, uint32_t chunkSize)
{
    if (chunkSize < sizeof(BextChunk)) return;

    BextChunk bext;
    memset(&bext, 0, sizeof(bext));
    fread(&bext, sizeof(BextChunk), 1, f);

    printf("\n--- Broadcast Extension (bext) ---\n");

    std::string desc = readFixedString(bext.description, 256);
    std::string orig = readFixedString(bext.originator, 32);
    std::string origRef = readFixedString(bext.originatorRef, 32);
    std::string date = readFixedString(bext.originationDate, 10);
    std::string time = readFixedString(bext.originationTime, 8);

    if (!desc.empty())    printf("  Description: %s\n", desc.c_str());
    if (!orig.empty())    printf("  Originator: %s\n", orig.c_str());
    if (!origRef.empty()) printf("  Originator Ref: %s\n", origRef.c_str());
    if (!date.empty())    printf("  Date: %s\n", date.c_str());
    if (!time.empty())    printf("  Time: %s\n", time.c_str());

    uint64_t timeRef = ((uint64_t)bext.timeRefHigh << 32) | bext.timeRefLow;
    if (timeRef > 0)      printf("  Time Reference: %llu samples\n", (unsigned long long)timeRef);
    if (bext.version > 0) printf("  BWF Version: %d\n", bext.version);
}

static void printSmpl(FILE* f, uint32_t chunkSize)
{
    if (chunkSize < sizeof(SmplChunk)) return;

    SmplChunk smpl;
    fread(&smpl, sizeof(SmplChunk), 1, f);

    printf("\n--- Sampler (smpl) ---\n");
    printf("  MIDI Note: %s (%u)\n", noteName(smpl.midiUnityNote), smpl.midiUnityNote);

    if (smpl.midiPitchFraction > 0)
    {
        double cents = (smpl.midiPitchFraction / 4294967296.0) * 100.0;
        printf("  Fine Tune: +%.1f cents\n", cents);
    }

    if (smpl.samplePeriod > 0)
        printf("  Sample Period: %u ns (%.1f Hz)\n", smpl.samplePeriod, 1000000000.0 / smpl.samplePeriod);

    if (smpl.numSampleLoops > 0)
    {
        printf("  Loops: %u\n", smpl.numSampleLoops);
        for (uint32_t i = 0; i < smpl.numSampleLoops; i++)
        {
            SmplLoop loop;
            if (fread(&loop, sizeof(SmplLoop), 1, f) != 1) break;
            printf("    Loop %u: %s, Start: %u, End: %u",
                   i + 1, loopTypeName(loop.type), loop.start, loop.end);
            if (loop.playCount > 0)
                printf(", Count: %u", loop.playCount);
            printf("\n");
        }
    }
}

static void printInst(FILE* f, uint32_t chunkSize)
{
    if (chunkSize < sizeof(InstChunk)) return;

    InstChunk inst;
    fread(&inst, sizeof(InstChunk), 1, f);

    printf("\n--- Instrument (inst) ---\n");
    printf("  Root Note: %s (%d)\n", noteName(inst.unshiftedNote), inst.unshiftedNote);
    if (inst.fineTune != 0) printf("  Fine Tune: %+d cents\n", inst.fineTune);
    if (inst.gain != 0)     printf("  Gain: %+d dB\n", inst.gain);
    printf("  Key Range: %s - %s\n", noteName(inst.lowNote), noteName(inst.highNote));
    printf("  Velocity Range: %d - %d\n", inst.lowVelocity, inst.highVelocity);
}

static void printCue(FILE* f, uint32_t chunkSize)
{
    if (chunkSize < 4) return;

    uint32_t numCues;
    fread(&numCues, 4, 1, f);
    if (numCues == 0) return;

    printf("\n--- Cue Points ---\n");
    for (uint32_t i = 0; i < numCues; i++)
    {
        CuePoint cue;
        if (fread(&cue, sizeof(CuePoint), 1, f) != 1) break;
        printf("  Cue %u: sample %u\n", cue.id, cue.sampleOffset);
    }
}

static void printAcid(FILE* f, uint32_t chunkSize)
{
    if (chunkSize < sizeof(AcidChunk)) return;

    AcidChunk acid;
    fread(&acid, sizeof(AcidChunk), 1, f);

    printf("\n--- ACID Loop Info ---\n");

    bool isOneShot = (acid.flags & 0x01) != 0;
    bool isLoop    = (acid.flags & 0x02) != 0;
    bool isDisk    = (acid.flags & 0x08) != 0;

    if (isOneShot)    printf("  Type: One-shot\n");
    else if (isLoop)  printf("  Type: Loop\n");
    if (isDisk)       printf("  Disk-based\n");

    if (acid.tempo > 0.0f)    printf("  Tempo: %.1f BPM\n", acid.tempo);
    if (acid.numBeats > 0)    printf("  Beats: %u\n", acid.numBeats);
    if (acid.meterNumer > 0)  printf("  Time Sig: %d/%d\n", acid.meterNumer, acid.meterDenom);
    if (acid.rootNote > 0 && acid.rootNote <= 127)
        printf("  Root Note: %s (%d)\n", noteName(acid.rootNote), acid.rootNote);
}

static void printIxml(FILE* f, uint32_t chunkSize)
{
    if (chunkSize == 0 || chunkSize > 1024 * 1024) return;

    std::vector<char> buf(chunkSize + 1, 0);
    fread(buf.data(), 1, chunkSize, f);

    // Just show a trimmed preview — full iXML can be very large
    std::string xml(buf.data());
    printf("\n--- iXML Metadata ---\n");

    // Extract a few useful tags if present
    auto extractTag = [&](const char* tagOpen, const char* tagClose, const char* label)
    {
        const char* start = strstr(xml.c_str(), tagOpen);
        if (!start) return;
        start += strlen(tagOpen);
        const char* end = strstr(start, tagClose);
        if (!end) return;
        std::string val(start, end - start);
        if (!val.empty()) printf("  %s: %s\n", label, val.c_str());
    };

    extractTag("<SCENE>", "</SCENE>", "Scene");
    extractTag("<TAKE>", "</TAKE>", "Take");
    extractTag("<NOTE>", "</NOTE>", "Note");
    extractTag("<PROJECT>", "</PROJECT>", "Project");
    extractTag("<TAPE>", "</TAPE>", "Tape");
    extractTag("<CIRCLED>", "</CIRCLED>", "Circled");
    extractTag("<SPEED>", "</SPEED>", "Speed");
    extractTag("<FILE_UID>", "</FILE_UID>", "File UID");

    // Count track info
    const char* trackCount = strstr(xml.c_str(), "<TRACK_COUNT>");
    if (trackCount)
    {
        trackCount += 13;
        const char* end = strstr(trackCount, "</TRACK_COUNT>");
        if (end) printf("  Track Count: %.*s\n", (int)(end - trackCount), trackCount);
    }
}

// ============================================================================

struct ChunkInfo
{
    char     id[4];
    uint32_t size;
    long     dataOffset; // file position of chunk data
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Stereo WAV Splitter\n");
        printf("Usage: Drag and drop a stereo .wav file onto this exe\n");
        printf("Creates <name>_L.wav and <name>_R.wav in the same folder\n");
        waitForKeypress();
        return 1;
    }

    const char* inputPath = argv[1];

    FILE* inFile = fopen(inputPath, "rb");
    if (!inFile)
    {
        printf("Error: Cannot open '%s'\n", inputPath);
        waitForKeypress();
        return 1;
    }

    // Read RIFF header (12 bytes)
    RiffHeader riff;
    if (fread(&riff, sizeof(RiffHeader), 1, inFile) != 1)
        return exitWithError("Error: Cannot read file header", inFile);

    if (memcmp(riff.riff, "RIFF", 4) != 0 || memcmp(riff.wave, "WAVE", 4) != 0)
        return exitWithError("Error: Not a valid WAV file", inFile);

    // --- Pass 1: Catalogue all chunks ---
    std::vector<ChunkInfo> chunks;
    while (true)
    {
        ChunkInfo ci;
        if (fread(ci.id, 4, 1, inFile) != 1) break;
        if (fread(&ci.size, 4, 1, inFile) != 1) break;
        ci.dataOffset = ftell(inFile);
        chunks.push_back(ci);

        uint32_t skipSize = ci.size + (ci.size & 1);
        fseek(inFile, skipSize, SEEK_CUR);
    }

    // --- Find fmt chunk ---
    int fmtIdx = -1, dataIdx = -1;
    for (int i = 0; i < (int)chunks.size(); i++)
    {
        if (memcmp(chunks[i].id, "fmt ", 4) == 0) fmtIdx = i;
        if (memcmp(chunks[i].id, "data", 4) == 0) dataIdx = i;
    }

    if (fmtIdx < 0) return exitWithError("Error: Could not find 'fmt ' chunk", inFile);
    if (dataIdx < 0) return exitWithError("Error: Could not find 'data' chunk", inFile);

    // Read fmt
    fseek(inFile, chunks[fmtIdx].dataOffset, SEEK_SET);
    FmtChunk fmt;
    if (chunks[fmtIdx].size < sizeof(FmtChunk))
        return exitWithError("Error: fmt chunk too small", inFile);
    if (fread(&fmt, sizeof(FmtChunk), 1, inFile) != 1)
        return exitWithError("Error: Cannot read fmt chunk", inFile);

    if (fmt.numChannels != 2)
    {
        printf("Error: File is not stereo (has %d channels)\n", fmt.numChannels);
        fclose(inFile);
        waitForKeypress();
        return 1;
    }

    if (fmt.audioFormat != 1 && fmt.audioFormat != 3)
        return exitWithError("Error: Unsupported format (only PCM and IEEE float supported)", inFile);

    // --- Print file info ---
    printf("Input: %s\n", inputPath);
    printf("Format: %s, %d-bit, %d Hz, Stereo\n",
           fmt.audioFormat == 1 ? "PCM" : "IEEE Float",
           fmt.bitsPerSample, fmt.sampleRate);

    uint32_t dataSize = chunks[dataIdx].size;
    uint16_t bytesPerSample = fmt.bitsPerSample / 8;
    uint32_t frameSize = bytesPerSample * 2;
    uint32_t numFrames = dataSize / frameSize;
    double duration = (double)numFrames / fmt.sampleRate;
    int mins = (int)(duration / 60.0);
    double secs = duration - mins * 60.0;

    printf("Duration: %d:%05.2f (%u frames)\n", mins, secs, numFrames);

    // --- Print metadata from all chunks ---
    printf("\n--- Chunks found ---\n");
    for (auto& ci : chunks)
        printf("  %.4s  (%u bytes)\n", ci.id, ci.size);

    for (auto& ci : chunks)
    {
        fseek(inFile, ci.dataOffset, SEEK_SET);

        if      (memcmp(ci.id, "LIST", 4) == 0) printListInfo(inFile, ci.size);
        else if (memcmp(ci.id, "bext", 4) == 0) printBext(inFile, ci.size);
        else if (memcmp(ci.id, "smpl", 4) == 0) printSmpl(inFile, ci.size);
        else if (memcmp(ci.id, "inst", 4) == 0) printInst(inFile, ci.size);
        else if (memcmp(ci.id, "cue ", 4) == 0) printCue(inFile, ci.size);
        else if (memcmp(ci.id, "acid", 4) == 0) printAcid(inFile, ci.size);
        else if (memcmp(ci.id, "iXML", 4) == 0) printIxml(inFile, ci.size);
    }

    // --- Split ---
    printf("\n--- Splitting ---\n");

    uint32_t monoDataSize = numFrames * bytesPerSample;

    char leftPath[1024], rightPath[1024];
    strncpy(leftPath, inputPath, sizeof(leftPath) - 1);
    leftPath[sizeof(leftPath) - 1] = '\0';
    strncpy(rightPath, inputPath, sizeof(rightPath) - 1);
    rightPath[sizeof(rightPath) - 1] = '\0';

    char* dotL = strrchr(leftPath, '.');
    char* dotR = strrchr(rightPath, '.');

    if (dotL) strcpy(dotL, "_L.wav");
    else      strcat(leftPath, "_L.wav");

    if (dotR) strcpy(dotR, "_R.wav");
    else      strcat(rightPath, "_R.wav");

    // Build minimal output header
    uint32_t monoFileSize = 44 - 8 + monoDataSize;

    struct OutputHeader
    {
        char     riffId[4];
        uint32_t riffSize;
        char     waveId[4];
        char     fmtId[4];
        uint32_t fmtChunkSize;
        uint16_t audioFormat;
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        char     dataId[4];
        uint32_t dataChunkSize;
    };

    OutputHeader outHeader;
    memcpy(outHeader.riffId, "RIFF", 4);
    outHeader.riffSize = monoFileSize;
    memcpy(outHeader.waveId, "WAVE", 4);
    memcpy(outHeader.fmtId, "fmt ", 4);
    outHeader.fmtChunkSize = 16;
    outHeader.audioFormat = fmt.audioFormat;
    outHeader.numChannels = 1;
    outHeader.sampleRate = fmt.sampleRate;
    outHeader.byteRate = fmt.sampleRate * bytesPerSample;
    outHeader.blockAlign = bytesPerSample;
    outHeader.bitsPerSample = fmt.bitsPerSample;
    memcpy(outHeader.dataId, "data", 4);
    outHeader.dataChunkSize = monoDataSize;

    FILE* leftFile = fopen(leftPath, "wb");
    FILE* rightFile = fopen(rightPath, "wb");

    if (!leftFile || !rightFile)
        return exitWithError("Error: Cannot create output files", inFile, leftFile, rightFile);

    fwrite(&outHeader, sizeof(OutputHeader), 1, leftFile);
    fwrite(&outHeader, sizeof(OutputHeader), 1, rightFile);

    // Seek to data chunk
    fseek(inFile, chunks[dataIdx].dataOffset, SEEK_SET);

    const uint32_t bufferFrames = 65536;
    std::vector<uint8_t> buffer(bufferFrames * frameSize);
    std::vector<uint8_t> leftBuf(bufferFrames * bytesPerSample);
    std::vector<uint8_t> rightBuf(bufferFrames * bytesPerSample);

    uint32_t framesRemaining = numFrames;

    while (framesRemaining > 0)
    {
        uint32_t framesToRead = framesRemaining < bufferFrames ? framesRemaining : bufferFrames;
        size_t bytesToRead = framesToRead * frameSize;

        if (fread(buffer.data(), 1, bytesToRead, inFile) != bytesToRead)
        {
            printf("Error: Unexpected end of audio data\n");
            break;
        }

        for (uint32_t i = 0; i < framesToRead; i++)
        {
            memcpy(&leftBuf[i * bytesPerSample],
                   &buffer[i * frameSize],
                   bytesPerSample);

            memcpy(&rightBuf[i * bytesPerSample],
                   &buffer[i * frameSize + bytesPerSample],
                   bytesPerSample);
        }

        fwrite(leftBuf.data(), 1, framesToRead * bytesPerSample, leftFile);
        fwrite(rightBuf.data(), 1, framesToRead * bytesPerSample, rightFile);

        framesRemaining -= framesToRead;
    }

    fclose(inFile);
    fclose(leftFile);
    fclose(rightFile);

    printf("Created: %s\n", leftPath);
    printf("Created: %s\n", rightPath);
    printf("Done.\n");
    waitForKeypress();

    return 0;
}
