#include <iostream>
#include <bitset>
#include <string>

#include <filesystem>
namespace fs = std::filesystem;

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "toml.hpp"

// implement portmidi here
#include "portmidi.h"


int play_sound_by_key_id(ma_engine *engine, toml::table config, int key_id, int velocity);

int main(int argc, char *argv[])
{
    Pm_Initialize();

    auto config = toml::parse_file("config.toml");

    bool adaptiveVolume = config["settings"]["adaptive-volume"].value_or(true);


    // get number of devices
    int numDevices = Pm_CountDevices();
    std::cout << "Number of devices: " << numDevices << std::endl;

    // get device info
    const PmDeviceInfo *deviceInfo;
    for (int i = 0; i < numDevices; i++)
    {
        deviceInfo = Pm_GetDeviceInfo(i);
        std::cout << "Device " << i << ": " << deviceInfo->name << " - " << (deviceInfo->output == 1 ? "Sortie" : "Entrée") << std::endl;
    }


    // initialize miniaudio variables
    ma_result result;
    ma_engine engine;
    ma_engine_config engineConfig;
  

    printf("Initializing audio engine...\n");

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.");
        return -1;
    }

    printf("Audio engine initialized.\n");
    float volume = config["settings"]["volume"].value_or(100);
    ma_engine_set_volume(&engine, volume/100);

        
    // open device 3
    PmStream *midiStream;
    int deviceID = config["settings"]["midi-device-id"].value_or(3);
    Pm_OpenInput(&midiStream, deviceID, NULL, 100, NULL, NULL);

    // read midi
    PmEvent buffer;

    int lastTimestamp = 0;

    while (true)
    {
        Pm_Read(midiStream, &buffer, 1);
        if(buffer.timestamp != lastTimestamp)
        {
            lastTimestamp = buffer.timestamp;
            //std::cout << "Status: " << buffer.timestamp << ": " << buffer.message << std::endl;

            // get status
            int status = Pm_MessageStatus(buffer.message);
            bool keyDown = (status & 0xF0) == 0x90;

            std::cout << std::endl << "Status: " << std::bitset<8>(status) << " " << (keyDown ? "Down" : "Up") << std::endl;

            // get channel
            int channel = status & 0x0F;
            std::cout << "Channel: " << channel << std::endl;

            // get data1
            int data1 = Pm_MessageData1(buffer.message);
            // get data2
            int data2 = Pm_MessageData2(buffer.message);

            std::cout << "Data1: " << data1 << " data2: " << data2 << std::endl;

            // get list of currently playing sounds
            

            if(data1 == 1 && (status == 0b10110000)) {
                std::cout << "Pitch bend" << std::endl;
                ma_engine_set_volume(&engine, data2 / 127.0f);
            }

            if(status == 0b11100000 && data1 != 64) {
                if(data1 >= 127/2) {
                    std::cout << "Reset" << std::endl;
                    ma_engine_stop(&engine); 
                } else {
                    std::cout << "Go" << std::endl;
                    ma_engine_start(&engine);
                }
            }

            printf(engine.nodeGraph.isReading ? "Started" : "Stopped");


            if(keyDown) {
                play_sound_by_key_id(&engine, config, data1, adaptiveVolume ? data2 : 127);
                std::cout << "Currently playing sounds: " << (engine.pInlinedSoundHead != NULL ? "Oui" : "Non") << std::endl;
                /* if(engine.pInlinedSoundHead != NULL) {
                    std::cout << "owns data source: " << (engine.pInlinedSoundHead->sound.ownsDataSource ? "Oui" : "Non") << std::endl;
                    std::cout << "next: " << (engine.pInlinedSoundHead->pNext != NULL ? "Oui" : "Non") << std::endl;
                    if(engine.pInlinedSoundHead->pNext != NULL)
                        std::cout << "next: " << (engine.pInlinedSoundHead->pNext->pNext != NULL ? "Oui" : "Non") << std::endl;
                        
                    std::cout << "prev: " << (engine.pInlinedSoundHead->pPrev != NULL ? "Oui" : "Non") << std::endl;

                } */
            }
        }
    }


    ma_engine_uninit(&engine);    
    Pm_Terminate();
    return 0;
}

#define c89atomic_memory_order_seq_cst                          __ATOMIC_SEQ_CST
#define c89atomic_exchange_32(dst, src)                                 c89atomic_exchange_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_32(dst, src)                                c89atomic_fetch_sub_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_32(dst, src)                                c89atomic_fetch_add_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_explicit_32(dst, src, order)        __atomic_fetch_add(dst, src, order)
#define c89atomic_fetch_sub_explicit_32(dst, src, order)        __atomic_fetch_sub(dst, src, order)
#define c89atomic_exchange_explicit_32(dst, src, order)         __atomic_exchange_n(dst, src, order)


int play_sound_by_key_id(ma_engine *engine, toml::table config, int key_id, int velocity)
{
    float pVolume = velocity / 150.0f; // max 127 mais on divise par 150 pour avoir un volume moins fort


    std::string prefix = "./resources/";


    std::string pFilePath = prefix+ config["sounds"][std::to_string(key_id)].value_or("-1");

    std::string noFile = prefix+"-1";

    if(pFilePath == noFile) {
        printf("No sound for key %d\n", key_id);
        return 1;
    }    

    std::cout << "Playing sound " << pFilePath << " at volume " << pVolume << std::endl;

    ma_result result = MA_SUCCESS;
    ma_sound_inlined* pSound = NULL;
    ma_sound_inlined* pNextSound = NULL;
    ma_node* pNode = NULL;
    ma_uint32 nodeInputBusIndex = 0;

    if (engine == NULL) {
        return MA_INVALID_ARGS;
    }

    /* Attach to the endpoint node if nothing is specicied. */
    pNode = ma_node_graph_get_endpoint(&engine->nodeGraph);
    nodeInputBusIndex = 0;
    
    ma_spinlock_lock(&engine->inlinedSoundLock);
    {
        ma_uint32 soundFlags = 0;

        for (pNextSound = engine->pInlinedSoundHead; pNextSound != NULL; pNextSound = pNextSound->pNext) {
            if (ma_sound_at_end(&pNextSound->sound)) {
                /*
                The sound is at the end which means it's available for recycling. All we need to do
                is uninitialize it and reinitialize it. All we're doing is recycling memory.
                */
                pSound = pNextSound;
                c89atomic_fetch_sub_32(&engine->inlinedSoundCount, 1);
                break;
            }
        }

        if (pSound != NULL) {
            /*
            We actually want to detach the sound from the list here. The reason is because we want the sound
            to be in a consistent state at the non-recycled case to simplify the logic below.
            */
            if (engine->pInlinedSoundHead == pSound) {
                engine->pInlinedSoundHead =  pSound->pNext;
            }

            if (pSound->pPrev != NULL) {
                pSound->pPrev->pNext = pSound->pNext;
            }
            if (pSound->pNext != NULL) {
                pSound->pNext->pPrev = pSound->pPrev;
            }

            /* Now the previous sound needs to be uninitialized. */
            ma_sound_uninit(&pNextSound->sound);
        } else {
            /* No sound available for recycling. Allocate one now. */
            pSound = (ma_sound_inlined*)ma_malloc(sizeof(*pSound), &engine->allocationCallbacks);
        }

        if (pSound != NULL) {   /* Safety check for the allocation above. */
            /*
            At this point we should have memory allocated for the inlined sound. We just need
            to initialize it like a normal sound now.
            */
            soundFlags |= MA_SOUND_FLAG_ASYNC;                 /* For inlined sounds we don't want to be sitting around waiting for stuff to load so force an async load. */
            soundFlags |= MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT; /* We want specific control over where the sound is attached in the graph. We'll attach it manually just before playing the sound. */
            soundFlags |= MA_SOUND_FLAG_NO_PITCH;              /* Pitching isn't usable with inlined sounds, so disable it to save on speed. */
            soundFlags |= MA_SOUND_FLAG_NO_SPATIALIZATION;     /* Not currently doing spatialization with inlined sounds, but this might actually change later. For now disable spatialization. Will be removed if we ever add support for spatialization here. */

            result = ma_sound_init_from_file(engine, pFilePath.c_str(), soundFlags, NULL, NULL, &pSound->sound);
            if (result == MA_SUCCESS) {
                /* Now attach the sound to the graph. */
                result = ma_node_attach_output_bus(pSound, 0, pNode, nodeInputBusIndex);
                if (result == MA_SUCCESS) {
                    
                    /* Set the volume. */
                    ma_sound_set_volume(&pSound->sound, pVolume);
                    
                    /* At this point the sound should be loaded and we can go ahead and add it to the list. The new item becomes the new head. */
                    pSound->pNext = engine->pInlinedSoundHead;
                    pSound->pPrev = NULL;

                    engine->pInlinedSoundHead = pSound;    /* <-- This is what attaches the sound to the list. */
                    if (pSound->pNext != NULL) {
                        pSound->pNext->pPrev = pSound;
                    }
                } else {
                    ma_free(pSound, &engine->allocationCallbacks);
                }
            } else {
                ma_free(pSound, &engine->allocationCallbacks);
            }
        } else {
            result = MA_OUT_OF_MEMORY;
        }
    }
    ma_spinlock_unlock(&engine->inlinedSoundLock);

    if (result != MA_SUCCESS) {
        return result;
    }

    /* Finally we can start playing the sound. */
    result = ma_sound_start(&pSound->sound);
    if (result != MA_SUCCESS) {
        /* Failed to start the sound. We need to mark it for recycling and return an error. */
        c89atomic_exchange_32(&pSound->sound.atEnd, MA_TRUE);
        return result;
    }

    c89atomic_fetch_add_32(&engine->inlinedSoundCount, 1);
    return result;
}