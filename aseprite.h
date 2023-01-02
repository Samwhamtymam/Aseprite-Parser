#pragma once
#include <stdio.h>
#include <iostream>
#include <vector>
#include <zlib.h>

// NOTE: Aseprite parser class written using the file specs: https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md#header,
// and referencing NoelFB's C# aseprite parser: https://gist.github.com/NoelFB/778d190e5d17f1b86ebf39325346fcc5.

struct Color
{
    uint8_t R;
    uint8_t G;
    uint8_t B;
    uint8_t A;

    void print(){
        printf("{R,G,B,A} = {%u, %u, %u, %u}. \n", R, G, B, A);
    }
};

class BufferReader
{
    uint _readCounter;
    char* _buffer;
public:
    BufferReader(char* __buffer){
        _buffer = __buffer;
        _readCounter = 0;
    }

    // Counter getter
    uint readCount() {return _readCounter;}

    // Funcs clean up and match aseprite docs
    void BYTE(uint8_t* __dest){
        std::memcpy(__dest, _buffer + _readCounter, 1);
        _readCounter += 1;
    }
    void SHORT(int16_t* __dest){
        std::memcpy(__dest, _buffer + _readCounter, 2);
        _readCounter += 2;
    }
    void DWORD(uint32_t* __dest){
        std::memcpy(__dest, _buffer + _readCounter, 4);
        _readCounter += 4;
    }
    void WORD(uint16_t* __dest){
        std::memcpy(__dest, _buffer + _readCounter, 2);
        _readCounter += 2;
    }
    void STRING(std::vector<char>* __str){
        uint16_t len;
        WORD(&len);
        for (int c=0; c<len; c++){
            uint8_t int_char;
            BYTE(&int_char);
            __str->push_back((char)int_char);
        }
        __str->push_back('\0');
    }
    void PIXEL(std::vector<Color>* __dest, int __len){
        for (int p=0; p<__len; p++){
            Color color;
            uint8_t chan[4];
            for (int c=0; c<4; c++) {BYTE(&chan[c]);}
            color = (Color){.R=chan[0], .G=chan[1], .B=chan[2], .A=chan[3]};
            __dest->push_back(color);
        }
    }
    void _U_CHAR_ARR(unsigned char __start[], int __len){
        for (int c=0; c<__len; c++){
            std::memcpy(&(__start[c]), _buffer + _readCounter, 1);  // Might be wrong
            _readCounter += 1;
        }
    }
    void SEEK(uint __jump){
        _readCounter += __jump;
    }
    void DECODE(std::vector<Color>* __dest, int __in_len, int __out_len){
        // __out_len is number of pixels * 4 bytes per pixel and __in_len is length of compressed data.
        // Copied from https://gist.github.com/arq5x/5315739 and http://zlib.net/zlib_how.html
        unsigned char in[__in_len];
        unsigned char out[__out_len];

        _U_CHAR_ARR(in, __in_len);

        // zlib struct
        z_stream infstream;
        infstream.zalloc = Z_NULL;
        infstream.zfree = Z_NULL;
        infstream.opaque = Z_NULL;

        infstream.avail_in = __in_len;
        infstream.next_in = (Bytef*)in;
        infstream.avail_out = __out_len;
        infstream.next_out = (Bytef*)out;

        // actual decompression
        inflateInit(&infstream);
        inflate(&infstream, Z_NO_FLUSH);
        inflateEnd(&infstream);

        for (int p=0; p<__out_len / 4; p++){
            Color color;
            uint8_t chan[4];
            for (int c=0; c<4; c++){chan[c] = (uint8_t)out[4*p+c];}
            color = (Color){.R=chan[0], .G=chan[1], .B=chan[2], .A=chan[3]};
            __dest->push_back(color);
        }
    }
};

struct UserData
{
    std::vector<char> text;
    Color color;
};

struct Tag
{
    enum LoopDirections
    {
        Forward = 0,
        Reverse = 1,
        PingPong = 2
    };

    std::vector<char> name;
    LoopDirections loopDirection;
    uint16_t from;
    uint16_t to;
    UserData userData;
};

class Layer
{
    // Tileset type no supported
public:
    enum Flags
    {
        Visible = 1,
        Editable = 2,
        LockMovement = 4,
        Background = 8,
        PreferLinkedCels = 16,
        Collapsed = 32,
        Reference = 64
    };

    enum Types
    {
        Normal = 0,
        Group = 1,
        Tilemap = 2
    };
    Flags flags;
    Types type;
    uint16_t childLevel;
    uint16_t blendMode;
    float opacity;
    std::vector<char> name;

    UserData userData;
};

class Cel
{
public:
    Layer layer;
    std::vector<Color> pixels;

    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    float opacity;

    UserData userData;
};

class Frame
{
public:
    // Header struct
    struct Header
    {
        uint32_t size;  // In bytes
        uint16_t magicNumber;
        uint16_t chunkCountOld;
        uint16_t frameDuration;
        // 2 Bytes of unused data that is set to zero goes here
        uint32_t chunkCountNew;
    };

    Header header;

    std::vector<Cel> celArray;

    Frame(){}

    ~Frame(){}

};

class Aseprite
{
public:
    // Header struct
    struct Header
    {
        uint32_t fileSize;
        uint16_t magicNumber;   //TODO: add a check to make sure this is (0xA5E0)
        uint16_t frameCount;
        uint16_t width;
        uint16_t height;
        uint16_t colorDepth;
        // there are 114 bytes left in the header that we dont use
    };

    Header header;
    std::vector<Frame> frameArray;
    std::vector<Layer> layerArray;
    std::vector<Tag> tagArray;

    enum Chunks
    {
        OldPaletteA = 0x0004,
        OldPaletteB = 0x0011,
        FrameLayer = 0x2004,
        FrameCel = 0x2005,
        CelExtra = 0x2006,
        ColorProfile = 0x2007,
        Mask = 0x2016,
        Path = 0x2017,
        FrameTags = 0x2018,
        Palette = 0x2019,
        ChunkUserData = 0x2020,
        Slice = 0x2022
    };

    Aseprite (const char* filePath);
};