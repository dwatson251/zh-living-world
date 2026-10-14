// LZH-Light ("NOX") codec is not part of the released EA source; these entry points report failure.
#include "LZHCompress/NoxCompress.h"
Bool DecompressFile(char *, char *) { return FALSE; }
Bool CompressFile(char *, char *) { return FALSE; }
Bool CompressPacket(char *, char *) { return FALSE; }
Bool DecompressPacket(char *, char *) { return FALSE; }
UnsignedInt CalcNewSize(UnsignedInt rawSize) { return rawSize; }
Bool DecompressMemory(void *, Int, void *, Int &) { return FALSE; }
Bool CompressMemory(void *, Int, void *, Int &) { return FALSE; }
