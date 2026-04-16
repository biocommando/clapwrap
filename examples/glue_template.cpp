#include "vst_bridge.h"

// -------------- VSTPlugin wrapper function stubs  --------------

#define WPLUG ((TestVST*)vst->effect)

bool VSTPlugin_init(VSTPlugin *vst)
{
    return true;
}
void VSTPlugin_activate(VSTPlugin *vst, double samplerate)
{
}
void VSTPlugin_process(VSTPlugin *vst, float **in, float **out, int frames)
{
}
void VSTPlugin_set_param(VSTPlugin *vst, int index, float value)
{
}
float VSTPlugin_get_param(VSTPlugin *vst, int index)
{
    return 0;
}
int VSTPlugin_get_param_count(VSTPlugin *vst)
{
    return 0;
}
void VSTPlugin_process_note_event(VSTPlugin *vst, int key, int velocity, int type)
{
}
void *VSTPlugin_get_chunk(VSTPlugin *vst, size_t *size)
{
    return nullptr;
}
bool VSTPlugin_set_chunk(VSTPlugin *vst, const void *data, size_t size)
{
    return true;
}
void VSTPlugin_destroy(VSTPlugin *vst)
{
}
void VSTPlugin_get_param_name(VSTPlugin *vst, int index, char *label)
{
}
void VSTPlugin_get_param_display(VSTPlugin *vst, int index, char *label)
{
}
void *VSTPlugin_new_plugin(VSTPlugin *vst)
{
    return nullptr;
}
bool VSTPlugin_has_ui(VSTPlugin *vst)
{
    return false;
}
void *VSTPlugin_create_editor(VSTPlugin *vst)
{
    return nullptr;
}
const char *get_vst_plugin_unique_id()
{
    return "com.myname.plugin";
}
const char *get_vst_plugin_name()
{
    return "plugin name";
}
const char *get_vst_plugin_vendor()
{
    return "my name";
}
const char *get_vst_plugin_version()
{
    return "0.1.0";
}
bool vst_is_synth()
{
    return false;
}
