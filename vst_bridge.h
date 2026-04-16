// vst_bridge.h
#pragma once
#include <cstdlib>

struct VSTPlugin {
    void* effect; // your FST AEffect or equivalent

    bool (*init)(VSTPlugin*);
    void (*activate)(VSTPlugin*, double samplerate);
    void (*process)(VSTPlugin*, float** in, float** out, int frames);
    void (*set_param)(VSTPlugin*, int index, float value);
    float (*get_param)(VSTPlugin*, int index);
    void (*get_param_name)(VSTPlugin*, int index, char *label);
    void (*get_param_display)(VSTPlugin*, int index, char *label);
    int (*get_param_count)(VSTPlugin*);

    void (*process_note_event)(VSTPlugin*, int key, int velocity, int type);

    void* (*get_chunk)(VSTPlugin*, size_t* size);
    bool  (*set_chunk)(VSTPlugin*, const void* data, size_t size);

    bool (*has_ui)(VSTPlugin*);
    void* (*create_editor)(VSTPlugin*);

    void (*destroy)(VSTPlugin*);
};

const char *get_plugin_path();

// You implement this using FST
VSTPlugin* create_vst_plugin();

const char *get_vst_plugin_unique_id();
const char *get_vst_plugin_name();
const char *get_vst_plugin_vendor();
const char *get_vst_plugin_version();
bool vst_is_synth();
