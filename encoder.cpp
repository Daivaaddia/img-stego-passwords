#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <zlib.h>
#include <string>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include "encoder.h"

#define PNG_MAGIC 0x0a1a0a0d474e5089

typedef struct ChunkIHDR {
    uint32_t width;
    uint32_t height;
    uint8_t colourWidth;
    uint8_t colourType;
    uint8_t compressionMethod;
    uint8_t filterMethod;
    uint8_t enlacementMethod;
} ChunkIHDR;

std::vector<uint8_t> steganographer(int mode, char *inputFile, unsigned char *message, int msgLen, char *outputFile);
bool isFilePng(std::ifstream& img);
void parseIHDR(std::ifstream& img, ChunkIHDR *chunk);
int findIDAT(std::ifstream& img, uint32_t *sizeIDAT);
std::vector<uint8_t> readIDATChunk(std::ifstream& img, size_t len);
std::vector<uint8_t> decompressIDATChunk(std::vector<uint8_t> compressedData, int maxOutputLen);
std::vector<uint8_t> compressIDATChunk(std::vector<uint8_t> decompressedData);
void processFilter(std::vector<uint8_t>& data, int scanlineLen, int bytesPerPixel);
void processFilterSub(std::vector<uint8_t>& data, int startPos, int len, int bytesPerPixel);
void embedMessage(std::vector<uint8_t>& data, unsigned char *message, int msgLen, int scanlineLen);
void refilter(std::vector<uint8_t>& data, int scanlineLen, int bytesPerPixel);
void refilterSub(std::vector<uint8_t>& data, int startPos, int len, int bytesPerPixel);
void createPNG(std::vector<uint8_t> compressedData, char *originalFileName, std::ifstream& img, int IDATDataStartPos, uint32_t originalIDATChunkSize, int maxOutputLen, char *outputFileString);
void readRestIDATs(std::vector<uint8_t>& compressedData, std::ifstream& img);
std::vector<uint8_t> decodeMessage(std::vector<uint8_t>& decompressedData, int scanlineLen);

void encodePlaintext(char *inputFile, unsigned char *message, int msgLen, char *outputFile) {
    steganographer(ENCODE, inputFile, message, msgLen, outputFile);
}

std::string decodePlaintext(char *inputFile) {
    std::vector<uint8_t> outputVec = steganographer(DECODE, inputFile, NULL, 0, NULL);
    std::string outputStr(outputVec.begin(), outputVec.end());
    return outputStr;
}

void handleEVPErrors(void) {
    ERR_print_errors_fp(stderr);
    abort();
}

void encodeAES(char *inputFile, unsigned char *message, int msgLen, char *outputFile, char *outputKeyFile) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int ciphertext_len;
    std::string ciphertext;
    unsigned char key[32]; // 256 bits
    unsigned char iv[16];

    if (!RAND_bytes(key, sizeof(key))) {
        std::cout << "Error generating random key\n";
        exit(1);
    }

    if (!RAND_bytes(iv, sizeof(iv))) {
        std::cout << "Error generating random IV\n";
        exit(1);
    }
    
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        handleEVPErrors();
    }

    if(EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1)
        handleEVPErrors();

    if(EVP_EncryptUpdate(ctx, (unsigned char *) ciphertext.data(), &len, message, msgLen) != 1) {
        handleEVPErrors();
    }
        
    ciphertext_len = len;
    
    if(EVP_EncryptFinal_ex(ctx, (unsigned char *) ciphertext.data() + len, &len) != 1) {     
        handleEVPErrors();
    }

    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    
    unsigned char keyMessage[48];
    memcpy(keyMessage, key, sizeof(key));
    memcpy(keyMessage + sizeof(key), iv, sizeof(iv));

    steganographer(ENCODE, inputFile, (unsigned char *) ciphertext.data(), ciphertext_len, outputFile);
    steganographer(ENCODE, inputFile, keyMessage, 48, outputKeyFile);
}

std::string decodeAES(char *inputFile, char *inputKeyFile) {
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    std::vector<uint8_t> ciphertext = steganographer(DECODE, inputFile, NULL, 0, NULL);
    std::vector<uint8_t> keyMessageVector = steganographer(DECODE, inputKeyFile, NULL, 0, NULL);
    std::vector<uint8_t> plaintext(ciphertext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));

    unsigned char key[32]; // 256 bits
    unsigned char iv[16];
    memcpy(key, keyMessageVector.data(), sizeof(key));
    memcpy(iv, keyMessageVector.data() + sizeof(key), sizeof(iv));

    if(!(ctx = EVP_CIPHER_CTX_new())) {
        handleEVPErrors();
    }

    if(EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        handleEVPErrors();
    }
        
    if(EVP_DecryptUpdate(ctx, (unsigned char *) plaintext.data(), &len, (unsigned char *) ciphertext.data(), ciphertext.size()) != 1) {
        handleEVPErrors();
    }

    plaintext_len = len;
    
    if(EVP_DecryptFinal_ex(ctx, (unsigned char *) plaintext.data() + len, &len) != 1) {
        handleEVPErrors();
    }

    plaintext_len += len;
    plaintext.resize(plaintext_len);

    EVP_CIPHER_CTX_free(ctx);

    std::string plaintextStr(plaintext.begin(), plaintext.end());
    return plaintextStr;
}

std::vector<uint8_t> steganographer(int mode, char *inputFile, unsigned char *message, int msgLen, char *outputFile) {
    std::ifstream img(inputFile, std::ios::binary);
    if (!isFilePng(img)) {
        std::cout << "File is not a PNG\n";
        exit(0);
    }

    ChunkIHDR chunkIHDR;
    parseIHDR(img, &chunkIHDR);

    if (chunkIHDR.colourWidth != 8 || chunkIHDR.colourType != 6) {
        std::cout << "Please select other PNG image!\n";
        img.close();
        exit(0);
    }
    // THE 4 IS HARDCODED - PART OF COLOURTYPE 6
    int bytesPerPixel = chunkIHDR.colourWidth * 4 / 8;

    uint32_t sizeIDAT;
    if (!findIDAT(img, &sizeIDAT)) {
        std::cout << "IDAT Chunk not found\n";
        img.close();
        exit(0);
    }

    size_t IDATDataStartPos = img.tellg();
    std::vector<uint8_t> compressedData = readIDATChunk(img, sizeIDAT);

    readRestIDATs(compressedData, img);

    int maxOutputLen = (chunkIHDR.height * chunkIHDR.width * 4) + chunkIHDR.height;
    std::vector<uint8_t> decompressedData = decompressIDATChunk(compressedData, maxOutputLen);

    // + 1 for filter byte
    // THE 4 IS HARDCODED - PART OF COLOURTYPE 6
    int scanlineLen = (chunkIHDR.width * 4) + 1;

    processFilter(decompressedData, scanlineLen, bytesPerPixel);

    if (mode == DECODE) {
        //DECODE
        std::vector<uint8_t> output = decodeMessage(decompressedData, scanlineLen);
        img.close();
        return output;
    }

    embedMessage(decompressedData, message, msgLen, scanlineLen);
    refilter(decompressedData, scanlineLen, bytesPerPixel);

    std::vector<uint8_t> recompressedData = compressIDATChunk(decompressedData);
    createPNG(recompressedData, inputFile, img, IDATDataStartPos, sizeIDAT, maxOutputLen, outputFile);

    img.close();
    return {};
}

bool isFilePng(std::ifstream& img) {
    uint64_t buffer;
    img.read(reinterpret_cast<char *>(&buffer), 8);

    return buffer == PNG_MAGIC;
}

void parseIHDR(std::ifstream& img, ChunkIHDR *chunk) {
    uint32_t chunkSize;
    img.read(reinterpret_cast<char *>(&chunkSize), 4);
    chunkSize = __builtin_bswap32(chunkSize);

    if (chunkSize != 13) {
        std::cout << "IHDR Chunk not of expected size: " << chunkSize << '\n';
        exit(0);
    }
    // Jump over IHDR
    img.seekg(4, std::ios::cur);

    uint32_t width;
    uint32_t height;
    img.read(reinterpret_cast<char *>(&width), 4);
    img.read(reinterpret_cast<char *>(&height), 4);
    chunk->width = __builtin_bswap32(width);
    chunk->height = __builtin_bswap32(height);

    img.read(reinterpret_cast<char *>(&(chunk->colourWidth)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->colourType)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->compressionMethod)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->filterMethod)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->enlacementMethod)), 1);
}

int findIDAT(std::ifstream& img, uint32_t *sizeIDAT) {
    char byte;
    while(img.get(byte)) {
        if (byte == 'I') {
            char buffer[4] = {0};
            img.read(buffer, 3);
            
            if (strcmp(buffer, "DAT") == 0) {
                img.seekg(-8, std::ios::cur);
                img.read(reinterpret_cast<char *>(sizeIDAT), 4);
                *sizeIDAT = __builtin_bswap32(*sizeIDAT);
                img.seekg(4, std::ios::cur); 
                return 1;
            }
            // Go back 3 bytes in case we get consecutive I
            img.seekg(-3, std::ios::cur); 
        }
    }

    // IDAT not found
    return 0;
}

std::vector<uint8_t> readIDATChunk(std::ifstream& img, size_t len) {
    std::vector<uint8_t> compressedData(len);
    img.read(reinterpret_cast<char *>(compressedData.data()), len);
    return compressedData;
}

std::vector<uint8_t> decompressIDATChunk(std::vector<uint8_t> compressedData, int maxOutputLen) {
    std::vector<uint8_t> decompressedData;

    z_stream inflateStream;
    memset(&inflateStream, 0, sizeof(z_stream));

    // Bytef is just a typedef for an uint8_t
    inflateStream.avail_in = compressedData.size();
    inflateStream.next_in = compressedData.data();

    int ret = inflateInit(&inflateStream) ;
    if (ret != Z_OK) {
        std::cout << "Error with inflateInit: " << ret << '\n';
        exit(0);
    }

    int bufferLen = maxOutputLen;
    uint8_t *buffer = (uint8_t *) malloc(bufferLen);

    inflateStream.avail_out = bufferLen;
    inflateStream.next_out = buffer;

    ret = inflate(&inflateStream, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        printf("Error: inflate returned %d\n", ret);
        inflateEnd(&inflateStream);
        exit(0);
    }

    if (inflateStream.avail_in != 0) {
        printf("Error: inflate did not consume all input\n");
        inflateEnd(&inflateStream);
        exit(0);
    }

    decompressedData.insert(decompressedData.end(), buffer, buffer + (bufferLen - inflateStream.avail_out));

    inflateEnd(&inflateStream);
    free(buffer);
    return decompressedData;
}

std::vector<uint8_t> compressIDATChunk(std::vector<uint8_t> decompressedData) {
    std::vector<uint8_t> compressedData;

    z_stream deflateStream;
    memset(&deflateStream, 0, sizeof(z_stream));

    // Bytef is just a typedef for an uint8_t
    deflateStream.avail_in = decompressedData.size();
    deflateStream.next_in = decompressedData.data();

    int ret = deflateInit(&deflateStream, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        std::cout << "Error with deflateInit: " << ret << '\n';
        exit(0);
    }

    int bufferLen = decompressedData.size();
    uint8_t *buffer = (uint8_t *) malloc(bufferLen);

    do {
        deflateStream.avail_out = bufferLen;
        deflateStream.next_out = buffer;

        ret = deflate(&deflateStream, Z_FINISH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            printf("Error: deflate returned %d\n", ret);
            free(buffer);
            deflateEnd(&deflateStream);
            exit(0);
        }

        compressedData.insert(compressedData.end(), buffer, buffer + (bufferLen - deflateStream.avail_out));
    } while (deflateStream.avail_out == 0);

    deflateEnd(&deflateStream);
    free(buffer);
    return compressedData;
}

void processFilter(std::vector<uint8_t>& data, int scanlineLen, int bytesPerPixel) {
    for (int i = 0; i < data.size(); i += scanlineLen) {
        switch (data[i]) {
            case 0:
                break;
            case 1:
                processFilterSub(data, i + 1, scanlineLen, bytesPerPixel);
                break;
            default:
                std::cout << "Unimplemented filter type: " << (int) data[i] << '\n';
                exit(0);
        }
    }
}

void processFilterSub(std::vector<uint8_t>& data, int startPos, int len, int bytesPerPixel) {
    // start on 2nd image pixel
    for (int i = startPos; i < startPos + len - 1; i++) {
        if (i < startPos + bytesPerPixel) {
            continue;
        }

        uint8_t prevPixel = data[i - bytesPerPixel];
        data[i] += prevPixel;
    }
}

void refilter(std::vector<uint8_t>& data, int scanlineLen, int bytesPerPixel) {
    for (int i = 0; i < data.size(); i += scanlineLen) {
        switch (data[i]) {
            case 0:
                break;
            case 1:
                refilterSub(data, i + 1, scanlineLen, bytesPerPixel);
                break;
            default:
                std::cout << "Unimplemented filter type: " << (int) data[i] << '\n';
                exit(0);
        }
    }
}

void refilterSub(std::vector<uint8_t>& data, int startPos, int len, int bytesPerPixel) {
    uint8_t *orig = (uint8_t *)malloc(data.size());
    memcpy(orig, data.data(), data.size());
    
    // start on 2nd image pixel
    for (int i = startPos; i < startPos + len - 1; i++) {
        if (i < startPos + bytesPerPixel) {
            continue;
        }

        uint8_t prevPixel = orig[i - bytesPerPixel];
        data[i] -= prevPixel;
    }

    free(orig);
}

void embedMessage(std::vector<uint8_t>& data, unsigned char *message, int msgLen, int scanlineLen) {
    // TODO: Correctly calculate this
    if (data.size() <= msgLen * 8) {
        std::cout << "Message is too long!\n";
        exit(0);
    }

    std::vector<uint8_t> messageBits;

    if (msgLen > 255) {
        std::cout << "Message is too long!\n";
        exit(0);
    }

    for (int i = 7; i >= 0; i--) {
        messageBits.push_back((msgLen >> i) & 1);
    }

    for (int i = 0; i < msgLen; i++) {
        uint8_t byte = message[i];
        for (int i = 7; i >= 0; i--) {
            messageBits.push_back((byte >> i) & 1);
        }
    }

    for (int dataIndex = 1, bitsIndex = 0; bitsIndex < messageBits.size(); dataIndex++, bitsIndex++) {
        // skip filter bytes
        if (dataIndex % scanlineLen == 0) {
            continue;
        }
        // 11111110
        data[dataIndex] = (data[dataIndex] & 0xFE) | messageBits[bitsIndex];
    }
}

void createPNG(std::vector<uint8_t> compressedData, char *originalFileName, std::ifstream& img, int IDATDataStartPos, uint32_t originalIDATChunkSize, int maxOutputLen, char *outputFileString) {
    // b indicates binary 
    FILE *original = fopen(originalFileName, "rb");
    FILE *output = fopen(outputFileString, "wb");

    // Go back to right before length bytes
    int headerSize = IDATDataStartPos - 8;

    uint8_t *buffer = (uint8_t *)malloc(headerSize);
    fread(buffer, sizeof(uint8_t), headerSize, original);
    fwrite(buffer, sizeof(uint8_t), headerSize, output);

    uint32_t length = compressedData.size();
    uint32_t lengthBigEndian = __builtin_bswap32(length);
    fwrite(&lengthBigEndian, sizeof(uint32_t), 1, output);

    std::vector<uint8_t> vectorIDAT = {'I', 'D', 'A', 'T'};

    uint32_t crc = 0;

    for (uint8_t c : vectorIDAT) {
        fwrite(&c, sizeof(uint8_t), 1, output);
        crc = crc32(crc, &c, 1);
    }   
    
    for (uint8_t c : compressedData) {
        fwrite(&c, sizeof(uint8_t), 1, output);
        crc = crc32(crc, &c, 1);
    }

    uint32_t crcBigEndian = __builtin_bswap32(crc);
    fwrite(&crcBigEndian, sizeof(uint32_t), 1, output);

    // headerSize + length + type + data + crc + length difference
    int currFileIndex = headerSize + 4 + 4 + length + 4 + (originalIDATChunkSize - length);

    //processRestOfFile(img, output, &currFileIndex, maxOutputLen);
    img.seekg(currFileIndex, std::ios::beg);

    char byte;
    while (img.get(byte)) {
        fwrite(&byte, sizeof(char), 1, output);
    }
}

void readRestIDATs(std::vector<uint8_t>& compressedData, std::ifstream& img) {
    uint32_t sizeIDAT;
    std::vector<uint8_t> newCompressedData;

    while(findIDAT(img, &sizeIDAT)) {
        newCompressedData = readIDATChunk(img, sizeIDAT);
        compressedData.insert(compressedData.end(), newCompressedData.begin(), newCompressedData.end());
    }
}

std::vector<uint8_t> decodeMessage(std::vector<uint8_t>& decompressedData, int scanlineLen) {
    uint8_t messageLenByte = 0;

    for (int i = 1; i <= 8; i++) {
        uint8_t byte = decompressedData[i];
        messageLenByte |= (byte & 1);

        if (i < 8) {
            messageLenByte <<= 1;
        }
    }

    int messageLen = static_cast<int>(messageLenByte);
    std::vector<uint8_t> messageVec;

    for (int i = 1; i <= messageLen; i++) {
        uint8_t letter = 0;
        for (int j = 1; j <= 8; j++) {
            uint8_t byte = decompressedData[j + (8 * i)];
            letter |= (byte & 1);

            if (j < 8) {
                letter <<= 1;
            }
        }
        messageVec.push_back(letter);
    }

    return messageVec;
}