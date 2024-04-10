//
//  main.c
//  affix
//
//  Created by Darryl Ramm on 2/5/24.
//

/*
 Copyright (c) 2024 Darryl Ramm

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#import <CoreServices/CoreServices.h>
#include <sys/stat.h>   // stat()
#include <libgen.h>     // basename()
#include "version.h"

// global option flags
Boolean verboseOpt      = FALSE;
Boolean debugOpt        = FALSE;
Boolean noWriteOpt      = FALSE;
Boolean sampleRateOpt   = FALSE;

// global flags
Boolean foundEOF        = FALSE;
Boolean invalidFile     = FALSE;
Boolean aiffIsCompressed;

char * fileName; // global -- file we are currently processing

// Supplements to structures and pointers in AIFF.h
// e.g. ...*.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers/AIFF.h
typedef struct ChunkHeader *        ChunkHeaderPtr;         // Not in AIFF.h
typedef struct ContainerChunk *     ContainerChunkPtr;      // Not in AIFF.h

ChunkHeaderPtr              chunkHeaderPtr;
ContainerChunkPtr           containerChunkPtr;
CommonChunkPtr              commonChunkPtr;
ExtCommonChunkPtr           extCommonChunkPtr;
FormatVersionChunkPtr       formatVersionChunkPtr;

size_t maxChunkSize = sizeof(ExtCommonChunk) + 255;        // 255 extra bytes for Pascal string.

// AIFF/AIFF-C files always start with a FORM chunk
// Following are the local chunks that may follow the FORM
// * Can be one and one in a valid AIFF/AIFF-C file
// + Can be zero or one chunks in a valid AIFF/AIFF-C file
unsigned int commChunkCount;                    // *
unsigned int formatVersionChunkCount;           // * One and only one required in AIFF-C, should not be in AIFF
unsigned int soundDataChunkCount;               // +
unsigned int markerChunkCount;                  // + Optional, only one market chunk per form, each chunk can contain multiple markers
unsigned int instrumentChunkCount;              // + Optional, only one instrument chunk per form, each chunk can represent multiple instruments
unsigned int midiDataChunkCount;                // + The MIDI Data Chunk is optional. Any number of MIDI Data Chunks may exist in a FORM AIFF-C.
unsigned int audioRecordingChunkCount;          // +
unsigned int commentChunkCount;                 // + Only one comment chunk per form, each comment chunk can contain multiple comments and multiple optional links to markers
unsigned int nameChunkCount;                    // +
unsigned int authorChunkCount;                  // +
unsigned int copyrightChunkCount;               // +

// We don't use the following as there are no single chunk limits on these chunks'
// unsigned int applicationSpecificChunkCount
// unsigned int annotationChunkCount;
// The only chunks in AIFF-C file not in AIFF file: formatVersionChunk
// SAXL chunk was proposed as a part of AIFF-C but is not used/standardization was never finished?

// Function declarations
UInt32  getFORMChunk(int fd, ChunkHeaderPtr chunkPtr);
UInt32  getChunks(int fd, ChunkHeaderPtr chunkPtr);
UInt32  getChunkHead(int fd, ChunkHeaderPtr chunkPtr);
UInt32  getChunkBody(int fd, ChunkHeaderPtr chunkPtr, size_t size);
size_t  padOddSize(size_t size);
char *  stringFromUInt32(UInt32 val);
char *  cASCIIStringCopyFromCFString(CFStringRef cfString);
void    usage(const char * ourNameString);
void    printVersion(const char * ourNameString);


int main(int argc, const char * argv[]) {
    
    char * sampleRateString;
    long double sampleRate = 0.0L;
    unsigned int inputSampleRate;
    struct stat sb;
    int fd = 0;
    long double oldRateLD;
    UInt32 id;

    chunkHeaderPtr        = calloc(1, maxChunkSize);                    // Holder for all chucks
    if (chunkHeaderPtr == 0) {
        fprintf(stderr, "calloc(1, maxChunkSize = %lu) failed\n", maxChunkSize);
        exit(-1);
    }
    
    // All these chunk pointers point to the same memory
    // If we were going to do anything with them we'd keep them separate but here we
    // just recycle the same memory as we mostly care about the commonChunk and sample rate in it
    
    containerChunkPtr     = (ContainerChunkPtr) chunkHeaderPtr;
    commonChunkPtr        = (CommonChunkPtr) chunkHeaderPtr;
    extCommonChunkPtr     = (ExtCommonChunkPtr) chunkHeaderPtr;
    formatVersionChunkPtr = (FormatVersionChunkPtr) chunkHeaderPtr;
    
    int c;
    
    while ((c = getopt(argc, (char * const *) argv, "dfs:ntvVh")) != -1) {
        
        switch (c) {
                
            case 'd':
                debugOpt = TRUE;
                break;
                
            case 's':
                sampleRateOpt = TRUE;
                sampleRateString = optarg;
                unsigned long len = strlen(optarg);
                int numchars;
            
                int ret = sscanf(sampleRateString, "%u%n", &inputSampleRate, &numchars); // avoid any entry confusion by forcing input sample rates to be integer values
                
                sampleRate = (long double) inputSampleRate;
                
                if (debugOpt) {
                    fprintf(stderr, "strlen() returned %lu\n", len);
                    fprintf(stderr, "sscanf() returned %d\n", ret);
                    fprintf(stderr, "sscanf() read %d characters\n", numchars);
                    fprintf(stderr, "unsigned int inputSampleRate = %u\n", inputSampleRate);
                    fprintf(stderr, "long double sampleRate = %Lf\n", sampleRate);
                }
                
                if (numchars < len) {
                    fprintf(stderr, "-s sampleRate option must be an integer value\n");
                    exit(-1);
                }
                
                break;
                
            case 'n':
                noWriteOpt = TRUE;
                break;
                
            case 'v':
                verboseOpt = TRUE;
                break;

            case 'V':
                printVersion(basename((char *) argv[0]));
                exit(0);
                break;
                
            case 'h':
                usage(basename((char *) argv[0]));
                exit(1);
                break;
                
            default:
                usage(basename((char *) argv[0]));
                exit(1);
                break;
        }
    }

    if (debugOpt) {
        fprintf(stderr, "DEBUG: debugOpt        = %s\n", debugOpt       ? "TRUE" : "FALSE");
        fprintf(stderr, "DEBUG: sampleRateOpt   = %s\n", sampleRateOpt  ? "TRUE" : "FALSE");
        fprintf(stderr, "DEBUG: verboseOpt      = %s\n", verboseOpt     ? "TRUE" : "FALSE");
    }
    
    if (debugOpt) {
        fprintf(stderr, "DEBUG: optind = %d\n", optind);
    }
    
    // UTC timezone for possible AIFF-C Version date error reporting.
    CFTimeZoneRef utcTz = CFTimeZoneCreateWithName(kCFAllocatorDefault, CFSTR("UTC"), TRUE);
    CFTimeZoneSetDefault(utcTz);
    CFRelease(utcTz);
    
    if (argc == optind) {
        fprintf(stderr, "no file specified. Type %s -h for help\n", basename((char *) argv[0]));
        exit(-1);
    }
    
    for (int i = optind; i < argc ; i++) {
        // Loop over filenames in argv
        
        // We keep count of chunks where there can only be one of that type in the file.
        commChunkCount              = 0;
        formatVersionChunkCount     = 0;
        soundDataChunkCount         = 0;
        markerChunkCount            = 0;
        instrumentChunkCount        = 0;
        midiDataChunkCount          = 0;
        audioRecordingChunkCount    = 0;
        commentChunkCount           = 0;
        nameChunkCount              = 0;
        authorChunkCount            = 0;
        copyrightChunkCount         = 0;
        
        fileName = (char *) argv[i];
        
        if (debugOpt) {
            fprintf(stderr, "DEBUG: processing file: %s\n", fileName);
        }
        
        if (stat(fileName, &sb) == -1) {
            if (errno == ENOENT) {
                fprintf(stderr, "ERROR: %s does not exist\n", fileName);
                break;
            }
        }
        
        if ((stat(fileName, &sb) == 0 && S_ISDIR(sb.st_mode))) {
            fprintf(stderr, "%s is directory, skipping\n", fileName);
            break;
        }
        
        if (!(stat(fileName, &sb) == 0 && S_ISREG(sb.st_mode))) {
            fprintf(stderr, "ERROR: %s is not a standard file, skipping\n", fileName);
            break;
        }
        
        if (sampleRateOpt) {
            // Need file writable as well as readable
            // Be a little anal-retentive about explaining permission problems for non-technical users
            if ((access(fileName, R_OK) == -1) && (access(fileName, W_OK) == 0)) {
                fprintf(stderr, "ERROR: %s is not readable, skipping file\n", fileName);
                break;
            }
            else if ((access(fileName, R_OK) == 0) && (access(fileName, W_OK) == -1)) {
                fprintf(stderr, "ERROR: %s is not writable, skipping file\n", fileName);
                break;
            }
            else if ((access(fileName, R_OK) == -1) && (access(fileName, W_OK) == -1)) {
                fprintf(stderr, "ERROR: %s is not readable and not writable, skipping file\n", fileName);
                break;
            }
            else {
                // open file for reading and writing
                if ((fd = open(fileName, O_RDWR)) == -1) {
                    fprintf(stderr, "ERROR: %s not readable and writable, skipping file\n", fileName);
                    break;
                }
            }
        }
        else {
            // not rateOpt -- only need readable
            if ((fd = open(fileName, O_RDONLY)) == -1) {
                fprintf(stderr, "ERROR: %s: %s, not readable, skipping file\n", fileName, strerror(errno));
                break;
            }
        }

        getFORMChunk(fd, chunkHeaderPtr);
        
        if (invalidFile) {
            continue;
        }
        
        while ((id = getChunks(fd, chunkHeaderPtr))) {
            
            if (foundEOF) {
                continue;
            }
            
            if (debugOpt) {
                fprintf(stderr, "DEBUG: getChunks(%d, 0x%lX) = \'%s\'\n", fd, (unsigned long) chunkHeaderPtr, stringFromUInt32(id));
            }
            
            if (CFSwapInt32(id) == CommonID) {
                
                // A Common or Extended Common chunk so we can print out info and exit or modify the sample rate on disk if asked.
                // Would probably better to just read the important chunk headers into their own buffers. Something to do in future.
    
                off_t backup;
                off_t rememberPosition;
                
                rememberPosition = lseek(fd, 0, SEEK_CUR);

                backup = CFSwapInt32(extCommonChunkPtr->ckSize) + offsetof(ExtCommonChunk, numChannels) - offsetof(ExtCommonChunk, sampleRate);
                    
                if (debugOpt) {
                    fprintf(stderr, "DEBUG: backup = CFSwapInt32(extCommonChunkPtr->ckSize) + offsetof(ExtCommonChunk, numChannels) - offsetof(ExtCommonChunk, sampleRate)\n");
                    fprintf(stderr, "DEBUG: %lld = %u + %lu - %lu\n",
                            backup,
                            CFSwapInt32(extCommonChunkPtr->ckSize),
                            offsetof(ExtCommonChunk, numChannels),
                            offsetof(ExtCommonChunk, sampleRate));
                }

                off_t lseekRet;
                
                if ((lseekRet = lseek(fd, -backup, SEEK_CUR)) == -1) {
                    fprintf(stderr, "ERROR: %s lseek(%d, -%lld, SEEK_CUR) = %lld : %s : skipping file\n", fileName, fd, backup, lseekRet, strerror(errno));
                    continue;       // give up and get next file.
                }

                extended80 testRate;
                long double testRateLD;
                ssize_t r;

                if (sampleRateOpt && noWriteOpt) {
                    
                    // For testing we read not write
                    
                    if ((r = read(fd, &testRate, sizeof(extended80))) == -1) {
                        fprintf(stderr, "%s read(fd=%d, &test=0x%lx, sizeof(extended80)=%ld) = %zd\n", fileName, fd, (unsigned long) &testRate, sizeof(extended80), r);
                    }
                    x80told(&extCommonChunkPtr->sampleRate, &testRateLD);
                    
                    if (debugOpt) {
                        fprintf(stderr, "testRateLD = %0.1Lf\n", testRateLD);
                    }
                }
                else if (sampleRateOpt && !noWriteOpt) {
                    
                    // Actually overwrite the rate with new value

                    extended80 x80;
                    ldtox80(&sampleRate, &x80);
                    
                    if ((r = write(fd, &x80, sizeof(extended80))) == -1) {
                        fprintf(stderr, "%s write(fd=%d, &test=0x%lx, sizeof(extended80)=%ld) = %zd\n", fileName, fd, (unsigned long) &testRate, sizeof(extended80), r);
                    }
                }
                
                size_t ret;
                
                if ((ret = lseek(fd, rememberPosition, SEEK_SET)) != rememberPosition) {
                    fprintf(stderr, "ERROR: %s seek to end of comm/extComm chunk failed : lseek(%d, %lld, SEEK_SET) = %lu\n", fileName, fd, rememberPosition, ret);
                }
                
                if (aiffIsCompressed) {
                    
                    extCommonChunkPtr = (ExtCommonChunkPtr) chunkHeaderPtr;
                    
                    x80told(&extCommonChunkPtr->sampleRate, &oldRateLD);
                    
                    CFStringRef cfCompressionName = CFStringCreateWithPascalString(kCFAllocatorDefault, (ConstStr255Param) extCommonChunkPtr->compressionName, kCFStringEncodingASCII);
                    char * compressionName = calloc(1, extCommonChunkPtr->compressionName[1]+1);
                    CFStringGetCString(cfCompressionName, compressionName, extCommonChunkPtr->compressionName[1]+1, kCFStringEncodingASCII);
                    CFRelease(cfCompressionName);
                    
                    if (verboseOpt) {
                        printf("%s\t%d\t%u\t%d\t%.0Lf\t%s\t%s",
                                fileName,
                                CFSwapInt16(extCommonChunkPtr->numChannels),
                                CFSwapInt32(extCommonChunkPtr->numSampleFrames),
                                CFSwapInt16(extCommonChunkPtr->sampleSize),
                                oldRateLD,
                                "AIFC",
                                compressionName);
                    }
                    else {
                        printf("%s\t%.0Lf",
                                fileName,
                                oldRateLD);
                    }
                    
                } else {
                    
                    commonChunkPtr = (CommonChunkPtr) chunkHeaderPtr;
                    
                    x80told(&extCommonChunkPtr->sampleRate, &oldRateLD);
                    
                    // Corner case of any funky fractional sample rates.
                    
                    long double integral;
                    long double fractional = modfl(oldRateLD, &integral);
                    
                    if (fractional != 0) {
                        printf("%s: file has fractional sample rate, integer value shown is only approximate\n", fileName);
                    }
                
                    if (verboseOpt) {
                        printf("%s\t%d\t%u\t%d\t\%.0Lf\t\%s\t\%s",
                                fileName,
                                CFSwapInt16(commonChunkPtr->numChannels),
                                CFSwapInt32(commonChunkPtr->numSampleFrames),
                                CFSwapInt16(commonChunkPtr->sampleSize),
                                oldRateLD,
                                "AIFF",
                                "not compressed");
        
                    }
                    else {
                        printf("%s\t%.0Lf",
                                fileName,
                                oldRateLD);
                    }
                }
                
                if (sampleRateOpt) {
                    printf("\tsample rate reset to: %.0Lf", sampleRate);
                }
                printf("\n");
            }
        }
        foundEOF=FALSE;
    }
    exit(0);
}


UInt32 getChunkHead(int fd, ChunkHeaderPtr chunkPtr)  {

    ssize_t ret;
    
    memset(chunkPtr, 0, sizeof(maxChunkSize));                              // null out the chunk buffer we read into, makes debugging easier.
    // A more thorough program would read all these into separate chunk memory structures, at least for the small chunks.
    
    if ((ret = read(fd, chunkPtr,  sizeof(ChunkHeader))) != sizeof(ChunkHeader)) {
        
        if (ret != 0) {
            fprintf(stderr, "ERROR: %s: %s: read(fd=%d, chunkPtr=0x%lx, sizeof(ChunkHeader)=%lu)) != sizeof(ChunkHeader) returned %zd bytes\n",
                    fileName, strerror(errno), fd, (unsigned long) chunkPtr, sizeof(ChunkHeader), ret);
            exit(-1);
        }
    
        else {
            
            //found end of file

            if (commChunkCount == 0) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file: no \'COMM\' common chunk found, skipping file\n", fileName);
            }
            
            if (aiffIsCompressed && (formatVersionChunkCount == 0)) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file: no \'FVER\' format version chunk found in an AIFF-C file\n", fileName);
            }
            
            if (debugOpt) {
                printf("%s: getChunkHead(): read()=0 found end of file\n", fileName);
            }
            
            foundEOF = TRUE;
            
            return 0;
        }
    
    }
    
    return chunkPtr->ckID;
}


UInt32 getChunkBody(int fd, ChunkHeaderPtr chunkPtr, size_t size)  {

    ssize_t ret;
    
    if ((ret = read(fd, (char *) chunkPtr + sizeof(ChunkHeader), size)) != size) {
        perror("ERROR: read(fd, (containerChunkPtr + sizeof(ChunkHeader)), size)) != size)");
        fprintf(stderr, "ERROR: read() returned %zd bytes, expected %lu bytes\n", ret, size);
        exit(-1);
    }
    
    return chunkPtr->ckID;
}

UInt32 getFORMChunk(int fd, ChunkHeaderPtr chunkPtr)  {
    
    // Special case as the first chunk in the file needs to be a FORM chunk and there should be
    // nothing else in the file ahead of this.

    ssize_t ret;
    
    memset(containerChunkPtr, 0, sizeof(maxChunkSize));
    
    containerChunkPtr = (ContainerChunkPtr) chunkPtr;
                                                   
    if ((ret = read(fd, containerChunkPtr,  sizeof(ChunkHeader))) != sizeof(ChunkHeader)) {
        perror("ERROR: read(fd, chunkPtr, sizeof(ChunkHeader))) != sizeof(ChunkHeader)");
        fprintf(stderr, "ERROR: %s: read() returned %zd bytes, expected %lu bytes\n", fileName, ret, sizeof(ChunkHeader));
        exit(-1);
    }
    
    if (CFSwapInt32(containerChunkPtr->ckID) == FORMID) {
        if ((ret = read(fd, &containerChunkPtr->formType, sizeof(containerChunkPtr->formType) ) ) != sizeof(containerChunkPtr->formType)) {
            perror("ERROR: read(fd, &containerChunkPtr->formType, sizeof(containerChunkPtr->formType))) != sizeof(containerChunkPtr->formType)");
            fprintf(stderr, "ERROR: %s: read() returned %zd bytes, expected %lu bytes\n", fileName, ret, sizeof(containerChunkPtr->formType));
            exit(-1);
        }
        
        if (CFSwapInt32(containerChunkPtr->formType) == AIFFID) {
            aiffIsCompressed = FALSE;
        }
        else if (CFSwapInt32(containerChunkPtr->formType) == AIFCID) {
            aiffIsCompressed = TRUE;
        }
        else {
            fprintf(stderr, "%s: \'FORM\' contains unexpected type \'%s\', expected \'AIFF\' or \'AIFC\', skipping\n", fileName, stringFromUInt32(containerChunkPtr->formType));
            invalidFile = TRUE;
        }
    }
    else {
        fprintf(stderr, "%s: invalid AIFF/AIFF-C file: expected \'FORM\' chunk is missing, skipping\n", fileName);
        invalidFile = TRUE;
    }
    
    return chunkPtr->ckID;
}


UInt32 getChunks(int fd, ChunkHeaderPtr chunkPtr)  {
        
    UInt32 id;
    
    id = getChunkHead(fd, chunkPtr);
    
    if (foundEOF == TRUE) {
        return 0;
    }
    
    switch (CFSwapInt32(id)) {
            
        case FORMID:
            
            fprintf(stderr, "%s: invalid AIFF/AIFF-C file, more than one \'FORM\' form chunks found, skipping\n", fileName);
            invalidFile = TRUE;
            
            return id;
            break;
            
        case CommonID:
            
            if (++commChunkCount > 1) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, more than one \'COMM\' common chunks found, skipping\n", fileName);
                invalidFile = TRUE;
            }
            
            // If it's a local chunk (i.e. not a FORM chunk) and we care about it then we an use the chkSize in the header to read the rest of the chunk.
            // This correctly handles cases of variable size chunks like the extCommonChunk with variable length pascal string for compressionName strings.
            
            id = getChunkBody(fd, chunkPtr,  padOddSize(CFSwapInt32(chunkPtr->ckSize)));
            extCommonChunkPtr = (ExtCommonChunk *) chunkPtr;
            return id;
            break;
            
        case FormatVersionID:
            
            if (++formatVersionChunkCount > 1) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'FVER\' format version chunk, skipping\n", fileName);
                invalidFile = TRUE;
            }
            
            // A little unnecessary? trip down the AIFF-C Version rabbit hole, but I have run into files with corrupted version dates.

            id = getChunkBody(fd, chunkPtr,  padOddSize(CFSwapInt32(chunkPtr->ckSize)));
            formatVersionChunkPtr = (FormatVersionChunkPtr) chunkPtr;
            
            if (CFSwapInt32(formatVersionChunkPtr->timestamp) != AIFCVersion1) {
                
                CFDateRef date         = CFDateCreate(kCFAllocatorDefault, CFSwapInt32(formatVersionChunkPtr->timestamp) - kCFAbsoluteTimeIntervalSince1904);
                CFDateRef version1Date = CFDateCreate(kCFAllocatorDefault, AIFCVersion1 - kCFAbsoluteTimeIntervalSince1904);
                
                CFLocaleRef currentLocale = CFLocaleCopyCurrent();
                CFDateFormatterRef customDateFormatter = CFDateFormatterCreate(NULL, currentLocale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
                CFStringRef customDateFormat = CFSTR("MMM dd, yyyy, hh:mm:ss a z");
                CFDateFormatterSetFormat(customDateFormatter, customDateFormat);
                CFRelease(currentLocale);
                
                CFStringRef Version1DateString = CFDateFormatterCreateStringWithDate (NULL, customDateFormatter, version1Date);
                CFStringRef dateString = CFDateFormatterCreateStringWithDate (NULL, customDateFormatter, date);
                CFRelease(customDateFormatter);
                CFRelease(version1Date);
                CFRelease(date);
                
                fprintf(stderr, "%s: \'FVER\' version chunk timestamp not AIFF-C Version 1. Expected %s found %s\n",
                        fileName,
                        CFStringGetCStringPtr(Version1DateString, kCFStringEncodingASCII),
                        CFStringGetCStringPtr(dateString, kCFStringEncodingASCII));
                
                CFRelease(Version1DateString);
                CFRelease(dateString);
                
            }
            
            if (debugOpt) {
                fprintf(stderr, "DEBUG: formatVersionChunkPtr->timestamp = %u\n", formatVersionChunkPtr->timestamp);
            }
    
            return id;
            break;
            
        case SoundDataID:
            
            if (++soundDataChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'SSND\' sound data chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case MarkerID:
            
            if (++markerChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'MARK\' marker chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case InstrumentID:
            
            if (++instrumentChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file: contains more than one \'INST\' instrument chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case MIDIDataID:
            
            if (++midiDataChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file: contains more than one \'MIDI\' MIDI data chunk\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case AudioRecordingID:

            if (++audioRecordingChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file: contains more than one \'AESD\' audio recording chunk\n", fileName);
                invalidFile = TRUE;
            }
            
            goto skipChunk;

        case ApplicationSpecificID:
            
            // Any number of application specific chunks are allowed.
            
            goto skipChunk;

        case CommentID:
            
            if (++commentChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'COMT\' comment chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            
            goto skipChunk;

        case NameID:
            
            if (++nameChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'NAME\' name chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case AuthorID:
            
            if (++authorChunkCount > 1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'AUTH\' author chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case CopyrightID:
            
            if (++copyrightChunkCount >1 ) {
                fprintf(stderr, "%s: invalid AIFF/AIFF-C file, contains more than one \'(c) \' copyright chunk, skipping file\n", fileName);
                invalidFile = TRUE;
            }
            goto skipChunk;

        case AnnotationID:
            
            // Any number of annotation chunks are allowed
            
            goto skipChunk;
            
            // All these  local chunks we don't care about and we just lseek() over them.
            
        skipChunk:
            
            if (lseek(fd, padOddSize(CFSwapInt32(chunkHeaderPtr->ckSize)), SEEK_CUR) == -1) {
                fprintf(stderr, "ERROR: %s lseek(%d, %zu, SEEK_CUR) returned -1\n", fileName, fd, padOddSize(CFSwapInt32(chunkHeaderPtr->ckSize)));
                invalidFile = TRUE;
            }
            
            return id;      // Danger: calling code should not assume any id return implies a chunk is loaded in memory.
            
        default:
            
            fprintf(stderr, "%s: unknown chunk type: %s\n", fileName, stringFromUInt32(chunkHeaderPtr->ckID));
            return 0;
            break;
            
    }
}

size_t padOddSize(size_t size) {
    if (size % 2) {
        return size + 1;      // odd size
    }
    else {
        return size;          // even size
    }
}


char * stringFromUInt32(UInt32 val) {
    char * string = calloc(1, 5);
    sprintf(string, "%c%c%c%c", val, val >> 8, val >> 16, val >> 24);
    return string;
}


char * cASCIIStringCopyFromCFString(CFStringRef cfString) {
        if (cfString == NULL) {
            return NULL;
      }
    CFIndex len = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingASCII) + 1;
    char * buf = (char *) malloc(maxSize);
    if (CFStringGetCString(cfString, buf, maxSize, kCFStringEncodingASCII)) {
        return buf;
      }
    
    free(buf);
    
    return NULL;
}


void usage(const char * ourNameString) {
    printf("\
%s [-vVh] [-s sampleRate] aiff_file1 ... aiff_filen\n\
Print AIFF or AIFF-C file(s) sample rate, optionally other information, and\n\
optionally reset the sample rate. The standard output consists of a line of\n\
the following tab separated values:\n\
                    filename\n\
                    sample rate\n\
Options:\n\
 -s sampleRate   Reset file(s) sample rate to integer value sampleRate.\n\
 -v              verbose output. Output consist of a line of following tab\n\
                 separated values:\n\
                    filename\n\
                    number of audio channels\n\
                    number of sample frames in the file\n\
                    number of bits per sample\n\
                    sample rate\n\
                    file type, AIFF or AIFC (AIFF Compressed)\n\
                    compression type string from the AIFF-C file\n\
 -V              Version. Display version of this program, copyright, and \n\
                 license information.\n\
 -h              help. Display this help message.\n\
\n\
 e.g. affix music.aiff \n\
      affix -v sound.AIFF \n\
      affix -vs 96000 sound2.aifc \n\
      affix -v -s 192000 sound3.aif \n\
      affix -v * (reports verbose information for all files matched by *) \n\
\n", ourNameString);
    exit(1);
}


void printVersion(const char * ourNameString) {
    // Print the version and copyright information from the Info.plist embedded in the executable
    
    CFBundleRef ourBundle;
    CFStringRef nsHumanReadableCopyright;
    CFStringRef cfBundleVersion;
    CFStringRef cfBundleShortVersionCFString;
    
    ourBundle = CFBundleGetMainBundle();
    
    if (ourBundle == NULL) {
        // CFBundleGetMainBundle() will return a bundle even of there is no embedded Info.plist
        fprintf(stderr, "ERROR: %s: CFBundleGetMainBundle() returned NULL\n", ourNameString);
        exit(-1);
        }
        
    printf("%s: ", ourNameString);
    
    if ((cfBundleShortVersionCFString =
            CFBundleGetValueForInfoDictionaryKey(ourBundle, CFSTR("CFBundleShortVersionString"))) != NULL ) {
        char * bundleShortVersionString = cASCIIStringCopyFromCFString(cfBundleShortVersionCFString);
        printf("Version %s ", bundleShortVersionString);
        free(bundleShortVersionString);
    }
    else {
        printf("<cannot find CFBundleShortVersionString in embedded Info.plist> ");
    }
    
    if ((cfBundleVersion =
                CFBundleGetValueForInfoDictionaryKey(ourBundle, CFSTR("CFBundleVersion"))) != NULL ) {
        char * bundleVersionString = cASCIIStringCopyFromCFString(cfBundleVersion);
        printf("(%s) ", bundleVersionString);
        free(bundleVersionString);
    }
    else {
        printf("<cannot find CFBundleShortVersionString in embedded Info.plist> ");
    }
    
    if ((nsHumanReadableCopyright =
            CFBundleGetValueForInfoDictionaryKey(ourBundle, CFSTR("NSHumanReadableCopyright"))) != NULL) {
        char * humanReadableCopyrightString = cASCIIStringCopyFromCFString(nsHumanReadableCopyright);
        printf("%s\n", humanReadableCopyrightString);
        free(humanReadableCopyrightString);
    }
    else {
        printf("<cannot find NSHumanReadableCopyright in embedded Info.plist>\n");
    }
    return;
}
