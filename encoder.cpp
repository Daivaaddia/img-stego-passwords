#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cstdint>
#include <zlib.h>

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

bool isFilePng(std::ifstream& img);
void parseIHDR(std::ifstream& img, ChunkIHDR *chunk);
void findIDAT(std::ifstream& img, uint32_t *sizeIDAT);

std::vector<unsigned char> readIDATChunk(std::ifstream& img, size_t len);
std::vector<unsigned char> decompressIDATChunk(std::vector<unsigned char> compressedData);
std::vector<unsigned char> compressIDATChunk(std::vector<unsigned char> decompressedData);

void createPNG(std::vector<unsigned char> compressedData, char *originalFileName, std::ifstream& img, int IDATDataStartPos);

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Usage: <picture filepath> <message>";
        exit(0);
    }

    std::ifstream img(argv[1], std::ios::binary);
    if (!isFilePng(img)) {
        std::cout << "File is not a PNG\n";
        exit(0);
    }

    ChunkIHDR chunkIHDR;
    parseIHDR(img, &chunkIHDR);

    if (chunkIHDR.colourWidth != 8 || chunkIHDR.colourType != 6) {
        std::cout << "Please select other PNG image!\n";
        exit(0);
    }

    uint32_t sizeIDAT;
    findIDAT(img, &sizeIDAT);

    size_t IDATDataStartPos = img.tellg();
    // std::cout << "Position index of 1 byte after T of IDAT: " << img.tellg() << '\n';

    std::vector<unsigned char> compressedData= readIDATChunk(img, sizeIDAT);
    std::vector<unsigned char> decompressedData = decompressIDATChunk(compressedData);

    // + 1 for filter byte
    int scanlineLen = (chunkIHDR.width * 4) + 1;

    std::vector<unsigned char> recompressedData = compressIDATChunk(decompressedData);

    // for (unsigned char c : recompressedData) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    // }

    // for (unsigned char c : decompressedData) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c)) << " ";
    // }


    // Consider finding max size

    createPNG(compressedData, argv[1], img, IDATDataStartPos);

    img.close();
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

void findIDAT(std::ifstream& img, uint32_t *sizeIDAT) {
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
                return;
            }
            // Go back 3 bytes in case we get consecutive I
            img.seekg(-3, std::ios::cur); 
        }
    }

    // IDAT not found
    std::cout << "IDAT Chunk not found\n";
    exit(0);
}

std::vector<unsigned char> readIDATChunk(std::ifstream& img, size_t len) {
    std::vector<unsigned char> compressedData(len);
    img.read(reinterpret_cast<char *>(compressedData.data()), len);
    return compressedData;
}

std::vector<unsigned char> decompressIDATChunk(std::vector<unsigned char> compressedData) {
    std::vector<unsigned char> decompressedData;

    z_stream inflateStream;
    memset(&inflateStream, 0, sizeof(z_stream));

    // Bytef is just a typedef for an uint8_t
    inflateStream.avail_in = compressedData.size();
    inflateStream.next_in = (Bytef *) compressedData.data();

    int ret = inflateInit(&inflateStream) ;
    if (ret != Z_OK) {
        std::cout << "Error with inflateInit: " << ret << '\n';
        exit(0);
    }

    int bufferLen = compressedData.size();
    unsigned char *buffer = (unsigned char *) malloc(bufferLen);

    do {
        inflateStream.avail_out = bufferLen;
        inflateStream.next_out = buffer;

        ret = inflate(&inflateStream, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            printf("Error: inflate returned %d\n", ret);
            free(buffer);
            inflateEnd(&inflateStream);
            exit(0);
        }

        decompressedData.insert(decompressedData.end(), buffer, buffer + (bufferLen - inflateStream.avail_out));
    } while (inflateStream.avail_out == 0);

    inflateEnd(&inflateStream);
    free(buffer);
    return decompressedData;
}

std::vector<unsigned char> compressIDATChunk(std::vector<unsigned char> decompressedData) {
    std::vector<unsigned char> compressedData;

    z_stream deflateStream;
    memset(&deflateStream, 0, sizeof(z_stream));

    // Bytef is just a typedef for an uint8_t
    deflateStream.avail_in = decompressedData.size();
    deflateStream.next_in = (Bytef *) decompressedData.data();

    int ret = deflateInit(&deflateStream, -1);
    if (ret != Z_OK) {
        std::cout << "Error with deflateInit: " << ret << '\n';
        exit(0);
    }

    int bufferLen = decompressedData.size();
    unsigned char *buffer = (unsigned char *) malloc(bufferLen);

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

// std::vector<unsigned char> processFilterSub(std::vector<unsigned char> data, int scanlineLen) {

// }

void createPNG(std::vector<unsigned char> compressedData, char *originalFileName, std::ifstream& img, int IDATDataStartPos) {
    // b indicates binary 
    FILE *original = fopen(originalFileName, "rb");
    FILE *output = fopen("output.png", "wb");

    int headerSize = IDATDataStartPos - 8;

    unsigned char *buffer = (unsigned char *)malloc(headerSize);
    fread(buffer, sizeof(unsigned char), headerSize, original);
    fwrite(buffer, sizeof(unsigned char), headerSize, output);

    img.seekg(headerSize, std::ios::beg);

    uint32_t chunkSize;
    img.read(reinterpret_cast<char *>(&chunkSize), 4);
    chunkSize = __builtin_bswap32(chunkSize);

    std::cout << chunkSize << '\n';
}