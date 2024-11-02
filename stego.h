#include <string>
#ifndef ENCODER_H
#define ENCODER_H


void encodePlaintext(char *inputFile, unsigned char *message, int msgLen, char *outputFile);
std::string decodePlaintext(char *inputFile);
void encodeAES(char *inputFile, unsigned char *message, int msgLen, char *outputFile, char *inputKeyFile, char *outputKeyFile);
std::string decodeAES(char *inputFile, char *inputKeyFile);

const int ENCODE = 0;
const int DECODE = 1;

const int PLAINTEXT_MODE = 0;
const int AES_MODE = 1;

#endif