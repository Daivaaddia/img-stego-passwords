#include "encoder.h"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    int mode = std::stoi(argv[1]);
    int encodingOption = std::stoi(argv[2]);
    char *inputFile = argv[3];

    if (mode == ENCODE) {
        std::string message = argv[4];
        int msgLen = message.length();
        char *outputFile = argv[5];

        if (encodingOption == PLAINTEXT_MODE) {
            encodePlaintext(inputFile, (unsigned char *) message.data(), msgLen, outputFile);
        } else if (encodingOption == AES_MODE) {
            char *outputKeyFile = argv[6];
            encodeAES(inputFile, (unsigned char *) message.data(), msgLen, outputFile, outputKeyFile);
        }
    } else if (mode == DECODE) {
        char *inputKeyFile = argv[4];
        std::string output;
        if (encodingOption == PLAINTEXT_MODE) {
            output = decodePlaintext(inputFile);
        } else if (encodingOption == AES_MODE) {
            output = decodeAES(inputFile, inputKeyFile);
        }
        
        std::cout << output << '\n';
    }
}