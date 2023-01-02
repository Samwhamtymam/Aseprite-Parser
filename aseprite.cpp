#include "aseprite.h"

// NOTE: builds with: g++ aseprite.cpp -o build -std=c++11 -lz

Aseprite::Aseprite(const char* filePath)
{
    FILE * file;
    long fileSize;
    char * buffer;
    size_t result;

    // Open file
    file = fopen(filePath, "rb");     // mode = "rb" means read (r) and binary file (b)
    if (file==NULL) {fputs("File error at aseprite",stderr); exit(1);}

    // File length
    fseek(file, 0, SEEK_END);         // find end
    fileSize = ftell(file);           // how many bytes between file start and us (we at end)
    fseek(file, 0, SEEK_SET);         // back to start

    // Allocate buffer
    buffer = (char*)malloc(sizeof(char)*fileSize);
    if (buffer == NULL) {fputs ("Memory error at aseprite",stderr); exit(2);}

    // Read into buffer
    result = fread(buffer, 1, fileSize, file);
    if (result!=fileSize) {fputs("Reading error at aseprite", stderr); exit(3);}

    // Copy relevent data to header struct
    BufferReader br = BufferReader(buffer);

    br.DWORD(&header.fileSize);
    br.WORD(&header.magicNumber);
    br.WORD(&header.frameCount);
    br.WORD(&header.width);
    br.WORD(&header.height);
    br.WORD(&header.colorDepth);
    br.SEEK(114);  // Jump to end of header data

    // Store frames
    frameArray.reserve(header.frameCount);
    for (int i=0; i<header.frameCount; i++)
    {
        // TODO: make work for more than 1 frame
        Frame frame;
        // Collect frame header data
        br.DWORD(&frame.header.size);
        br.WORD (&frame.header.magicNumber);
        br.WORD (&frame.header.chunkCountOld);
        br.WORD (&frame.header.frameDuration);
        br.SEEK (2);
        br.DWORD(&frame.header.chunkCountNew);

        // Loop through chunks
        int chunkNum = 0;
        if (frame.header.chunkCountNew!=0) {chunkNum = frame.header.chunkCountNew;}
        else if (frame.header.chunkCountOld!=0xFFFF) {chunkNum = frame.header.chunkCountOld;}
        int lastChunk = 0;
        for (int c=0; c<chunkNum; c++)
        {
            uint chunkStart = br.readCount();
            uint chunkSize;
            br.DWORD(&chunkSize);
            uint chunkEnd = chunkStart + chunkSize;

            uint16_t chunkType;
            br.WORD(&chunkType);

            // Chunks
            if (chunkType == Chunks::FrameTags)
            {
                uint16_t numTags;
                // chunk header
                br.WORD(&numTags);
                tagArray.reserve(numTags);
                br.SEEK(8);             // skip useless data

                for (int t=0; t<numTags; t++)
                {
                    Tag tag;
                    
                    // read into tag
                    br.WORD(&tag.from);
                    br.WORD(&tag.to);
                    uint8_t loopDir;
                    br.BYTE(&loopDir);
                    tag.loopDirection = static_cast<Tag::LoopDirections>(loopDir);
                    br.SEEK(8);

                    // Color (supposedly depreciated)
                    uint8_t chan[4];
                    for (int c=0; c<3; c++){br.BYTE(&chan[c]);}
                    chan[3] = 255;                              // alpha
                    tag.userData.color = (Color){.R=chan[0], .G=chan[1], .B=chan[2], .A=chan[3]};

                    br.SEEK(1);
                    br.STRING(&tag.name);

                    // add tag
                    tagArray.push_back(tag);
                    lastChunk = Chunks::FrameTags;
                }
                
                // check we are at the right spot
                if (br.readCount()!=chunkEnd){
                    printf("Tag chunk reading error, we are at: %u, but should be at: %u \n", br.readCount(), chunkEnd);
                    exit(1);
                }
            }
            else if (chunkType == Chunks::FrameLayer)
            {
                Layer layer;

                // read into layer
                uint16_t flags;
                br.WORD(&flags);
                layer.flags = static_cast<Layer::Flags>(flags);

                uint16_t type;
                br.WORD(&type);
                layer.type = static_cast<Layer::Types>(type);

                br.WORD(&layer.childLevel);
                br.SEEK(4);
                br.WORD(&layer.blendMode);

                uint8_t opacity;
                br.BYTE(&opacity);
                layer.opacity = (float)(opacity / 255.0f);

                br.SEEK(3);
                br.STRING(&layer.name);

                if (layer.type == 2) {br.SEEK(4); printf("%s", "WARNING: Tileset type aseprite layers not supported.");}

                // check we are at the right spot
                if (br.readCount()!=chunkEnd){
                    printf("Layer chunk reading error, we are at: %u, but should be at: %u \n", br.readCount(), chunkEnd);
                    exit(1);
                }

                // add layer
                layerArray.push_back(layer);
                lastChunk = Chunks::FrameLayer;
            }
            else if (chunkType == Chunks::FrameCel)
            {
                Cel cel;

                // read into cel
                uint16_t layerIndex;
                br.WORD(&layerIndex);
                cel.layer = layerArray[layerIndex];

                br.SHORT(&cel.x);
                br.SHORT(&cel.y);
                
                uint8_t opacity;
                br.BYTE(&opacity);
                cel.opacity = (float)(opacity / 255.0f);

                uint16_t type;
                br.WORD(&type);
                
                br.SEEK(7);
                
                if (type == 0 || type == 2)
                {
                    br.WORD(&cel.width);
                    br.WORD(&cel.height);
                    if (header.colorDepth == 32)
                    {
                        uint bytes = cel.width * cel.height * 4;
                        cel.pixels.reserve(cel.width * cel.height);
                        if (type == 0){
                            br.PIXEL(&cel.pixels, cel.width * cel.height);
                        }
                        else if (type == 2){
                            uint len = chunkEnd - br.readCount();
                            br.DECODE(&cel.pixels, len, bytes);
                        }
                    }
                    else{
                        printf("%s \n", "WARNING: Grayscale and Index modes are not supported!");
                    }
                }
                else{
                    printf("WARNING: Linked cels and/or Compressed Tilemap cels are not supported!");
                    br.SEEK(chunkEnd - br.readCount());
                }
                // check we are at the right spot
                if (br.readCount()!=chunkEnd){
                    printf("WARNING: Cel chunk reading error, we are at: %u, but should be at: %u \n", br.readCount(), chunkEnd);
                    exit(1);
                }

                // add cel
                frame.celArray.push_back(cel);
                lastChunk = Chunks::FrameCel;
            }
            else if (chunkType == Chunks::ChunkUserData){
                UserData userData;
                uint32_t flags;
                br.DWORD(&flags);
                if (flags & 1){
                    br.STRING(&userData.text);
                }
                if (flags * 2){
                    uint8_t chan[4];
                    for (int c=0; c<4; c++) {br.BYTE(&chan[c]);}
                    userData.color = (Color){.R=chan[0], .G=chan[1], .B=chan[2], .A=chan[3]};
                }

                switch(lastChunk){
                    case Chunks::FrameCel:
                        // Not implemented
                        break;
                    case Chunks::FrameLayer:
                        // Not implemented
                        break;
                    case Chunks::FrameTags:
                        // Not implemented
                        printf("Tag user data found \n");               // never seem to see userdata chunks
                        break;
                    default:
                        break;
                }

                // check we are at the right spot
                if (br.readCount()!=chunkEnd){
                    printf("WARNING: Cel chunk reading error, we are at: %u, but should be at: %u \n", br.readCount(), chunkEnd);
                    exit(1);
                }
                
            }
            else 
            {
                br.SEEK(chunkEnd - br.readCount());
            }
        }

        // Add frame to array
        frameArray.push_back(frame);

    }

    // Release
    fclose(file);
    free(buffer);
}



int main(int argc, char **argv)
{
    const char* asepritePath = "test2.aseprite";
    Aseprite ase = Aseprite(asepritePath);

}