#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cstdint>

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
void findIDAT(std::ifstream& img);

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

    findIDAT(img);


    // Consider finding max size


    char byte;

    while (img.get(byte)) {
        //std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(byte)) << " ";
    }

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

    img.read(reinterpret_cast<char *>(&(chunk->width)), 4);
    img.read(reinterpret_cast<char *>(&(chunk->height)), 4);
    img.read(reinterpret_cast<char *>(&(chunk->colourWidth)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->colourType)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->compressionMethod)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->filterMethod)), 1);
    img.read(reinterpret_cast<char *>(&(chunk->enlacementMethod)), 1);
}

void findIDAT(std::ifstream& img) {
    char byte;
    while(img.get(byte)) {
        if (byte == 'I') {
            char buffer[4] = {0};
            img.read(buffer, 3);
            
            if (strcmp(buffer, "DAT") == 0) {
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