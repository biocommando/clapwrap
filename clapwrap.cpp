#include <clap/clap.h>
#include <clap/ext/state.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/audio-ports-activation.h>
#include <clap/ext/note-ports.h>
#include <clap/plugin-features.h>
#include <vector>
#include <cstring>
#include <string>
#include <stdarg.h>

#include "vst_bridge.h"

const char *_log_ctx_enabled[] = {
    "clap_plugin",
    "clap_gui",
    "note_events",
    "load_state",
#ifdef PLUGIN_DEBUG_LOG
    "plugin",
#endif
    nullptr
};

void clap_debug_logger(const char *ctx, const char *fmt, ...)
{
#if 0
	static int counters[32], init = 0;
    if (!init) memset(counters, 0, sizeof(counters));
    init = 1;
    int enabled = 0, counter_i = 0;
    for (int i = 0; !enabled && _log_ctx_enabled[i] && i < 32; i++)
    {
        counter_i = i;
        if (!strcmp(_log_ctx_enabled[i], ctx))
            enabled = 1;
    }
    if (!enabled || counters[counter_i] >= 1000)
        return;

    char fname[256];
    sprintf(fname, "D:\\code\\c\\debug-%s.log", ctx);
	FILE *f = fopen(fname, counters[counter_i] ? "a" : "w");
	++counters[counter_i];
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    fputc('\n', f);
    if (counters[counter_i] >= 1000)
    {
        fprintf(f, "--- log line limit reached ---\n");
    }
    fclose(f);
#endif
}

struct ParamInfo
{
    clap_id id;
    int vst_index;
};

struct MyClapPlugin
{
    clap_plugin_t clap;

    const clap_host_t *host;

    VSTPlugin *vst;

    double sample_rate;
    uint32_t block_size;

    std::vector<ParamInfo> params;
    void *editor;

};

static const void *register_gui_wrapper(const clap_plugin_t* plugin, const char *id);

static char _plugin_path[1024] = "";
void set_plugin_path(const char *path)
{
    if (strlen(path) >= sizeof(_plugin_path))
        return;
    strcpy(_plugin_path, path);
}

const char *get_plugin_path()
{
    return _plugin_path;
}

static bool my_init(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "my_init");
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    if (p->vst)
        return true;

    p->vst = create_vst_plugin();
    if (!p->vst) return false;

    int count = p->vst->get_param_count(p->vst);

    p->params.resize(count);

    for (int i = 0; i < count; ++i) {
        p->params[i].id = i;        // simple mapping
        p->params[i].vst_index = i;
    }

    return p->vst->init(p->vst);
}

static void my_destroy(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "my_destroy");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    if (p->vst) {
        p->vst->destroy(p->vst);
        delete p->vst;
    }

    delete p;
}

static bool my_activate(const clap_plugin_t* plugin,
                        double sample_rate,
                        uint32_t min_frames,
                        uint32_t max_frames) {
    clap_debug_logger("clap_plugin", "my_activate");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    p->sample_rate = sample_rate;
    p->block_size = max_frames;

    p->vst->activate(p->vst, sample_rate);
    return true;
}

static void my_deactivate(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "my_deactivate");
    // nothing yet
}

static clap_process_status my_process(const clap_plugin_t* plugin,
                                      const clap_process_t* process) {
    clap_debug_logger("clap_plugin", "my_process");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    float** inputs  = process->audio_inputs_count ? process->audio_inputs[0].data32 : nullptr;
    float** outputs = process->audio_outputs[0].data32;

    int frames = process->frames_count;

    if (process->in_events) {
        uint32_t n = process->in_events->size(process->in_events);

        for (uint32_t i = 0; i < n; ++i) {
            const clap_event_header_t* ev =
                process->in_events->get(process->in_events, i);

            switch (ev->type) {

            case CLAP_EVENT_PARAM_VALUE: {
                auto* pe = (const clap_event_param_value_t*)ev;

                if (pe->param_id < p->params.size()) {
                    int vst_index = p->params[pe->param_id].vst_index;
                    p->vst->set_param(p->vst, vst_index, pe->value);
                }
                break;
            }

            case CLAP_EVENT_NOTE_ON: {
                auto* ne = (const clap_event_note_t*)ev;

                uint8_t note = ne->key;
                uint8_t vel  = (uint8_t)(ne->velocity * 127.0f);

                p->vst->process_note_event(p->vst, note, vel, 1);
                break;
            }

            case CLAP_EVENT_NOTE_OFF: {
                auto* ne = (const clap_event_note_t*)ev;

                uint8_t note = ne->key;

                p->vst->process_note_event(p->vst, note, 0, 0);
                break;
            }

            case CLAP_EVENT_MIDI: {
                auto* me = (const clap_event_midi_t*)ev;
                clap_debug_logger("note_events", "CLAP_EVENT_MIDI %d %d %d", (int)me->data[0], (int)me->data[1], (int)me->data[2]);
                bool note_on = (me->data[0] & 0xF0) == 0b10010000;
                bool note_off = (me->data[0] & 0xF0) == 0b10000000;
                if (note_on || note_off)
                    p->vst->process_note_event(p->vst, me->data[1], me->data[2], note_on ? 1 : 0);
            }

            default:
                break;
            }
        }
    }

    p->vst->process(p->vst, inputs, outputs, frames);

    return CLAP_PROCESS_CONTINUE;
}

static bool my_start_processing(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "my_start_processing");
    return true;
}

static void my_stop_processing(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "my_stop_processing");
}

static uint32_t params_count(const clap_plugin_t* plugin) {
    clap_debug_logger("clap_plugin", "params_count");
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    return p->params.size();
}

static bool params_get_info(const clap_plugin_t* plugin,
                            uint32_t index,
                            clap_param_info_t* info) {
    clap_debug_logger("clap_plugin", "params_get_info");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    if (index >= p->params.size()) return false;

    auto& param = p->params[index];

    memset(info, 0, sizeof(*info));
    info->id = param.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;

    p->vst->get_param_name(p->vst, param.vst_index, info->name);
    snprintf(info->module, sizeof(info->module), "VST");

    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 0.5;

    return true;
}

static bool params_get_value(const clap_plugin_t* plugin,
                             clap_id id,
                             double* value) {
    clap_debug_logger("clap_plugin", "params_get_value");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    if (id >= p->params.size()) return false;

    int vst_index = p->params[id].vst_index;
    *value = p->vst->get_param(p->vst, vst_index);

    return true;
}

static bool params_value_to_text(const clap_plugin_t* plugin,
                                 clap_id id,
                                 double value,
                                 char* out,
                                 uint32_t size) {
    clap_debug_logger("clap_plugin", "params_value_to_text");
    if (size <= 8)
        return false;
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    int vst_index = p->params[id].vst_index;
    p->vst->get_param_display(p->vst, vst_index, out);
    //snprintf(out, size, "%.3f", value);
    return true;
}

static bool params_text_to_value(const clap_plugin_t*,
                                 clap_id,
                                 const char* text,
                                 double* value) {
    clap_debug_logger("clap_plugin", "params_text_to_value");
    *value = atof(text);
    return true;
}


static bool state_save(const clap_plugin_t* plugin,
                       const clap_ostream_t* stream) {
    clap_debug_logger("clap_plugin", "state_save");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    size_t size = 0;
    void* data = p->vst->get_chunk(p->vst, &size);
    clap_debug_logger("clap_plugin", data ? "data" : "null");
    clap_debug_logger("clap_plugin", size == 0 ? "0" : "has size");

    if (!data || size == 0)
        return false;

    int64_t written = stream->write(stream, data, size);
    clap_debug_logger("clap_plugin", written == (int64_t)size ? "ok write" : "fail");

    return written == (int64_t)size;
}

static bool state_load(const clap_plugin_t* plugin,
                       const clap_istream_t* stream) {
    clap_debug_logger("clap_plugin", "state_load");
    auto* p = (MyClapPlugin*)plugin->plugin_data;

    // Read entire stream into memory
    std::vector<uint8_t> buffer;

    uint8_t temp[4096];

    while (true) {
        int64_t r = stream->read(stream, temp, sizeof(temp));
        clap_debug_logger("load_state", "num bytes read %ld", r);
        if (r <= 0) break;

        buffer.insert(buffer.end(), temp, temp + r);
    }
    clap_debug_logger("load_state", "buf sz %u", (unsigned)buffer.size());
    if (buffer.empty())
        return false;

    clap_debug_logger("load_state", "set to plugin");
    for (int i = 0; i < buffer.size(); i++)
        clap_debug_logger("load_state", "%u", ((unsigned)buffer[i])&0xff);

    return p->vst->set_chunk(p->vst, buffer.data(), buffer.size()) == 0;
}

static uint32_t audio_ports_count(const clap_plugin_t* plugin,
                                  bool is_input) {
    clap_debug_logger("clap_plugin", "audio_ports_count");
    if (is_input && vst_is_synth()) return 0;
    return 1; // stereo in/out
}

static bool audio_ports_get(const clap_plugin_t* plugin,
                            uint32_t index,
                            bool is_input,
                            clap_audio_port_info_t* info) {
    clap_debug_logger("clap_plugin", "audio_ports_get");
    if (index > 0) return false;

    info->id = 0;

    strcpy(info->name, is_input ? "Input" : "Output");

    info->channel_count = 2;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;

    char debg[1000];
    sprintf(debg, "clap_audio_port_info_t<%u: %u %u %s %u>", info->id, info->channel_count, info->flags, info->port_type, info->in_place_pair);
    clap_debug_logger("clap_plugin", debg);

    return true;
}


static uint32_t note_ports_count(const clap_plugin_t*, bool is_input) {
    clap_debug_logger("clap_plugin", "note_ports_count");
    return is_input && vst_is_synth() ? 1 : 0;
}

static bool note_ports_get(const clap_plugin_t*,
                           uint32_t index,
                           bool is_input,
                           clap_note_port_info_t* info) {
    clap_debug_logger("clap_plugin", "note_ports_get");
    if (!vst_is_synth()) return false;
    if (!is_input || index > 0) return false;

    info->id = 0;
    snprintf(info->name, sizeof(info->name), "Note Input");

    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_MIDI;

    return true;
}

static const clap_plugin_params_t params_ext = {
    params_count,
    params_get_info,
    params_get_value,
    params_value_to_text,
    params_text_to_value,
    nullptr, // flush handled in process
};

static const clap_plugin_state_t state_ext = {
    state_save,
    state_load
};

static const clap_plugin_audio_ports_t audio_ports_ext = {
    audio_ports_count,
    audio_ports_get
};

static const clap_plugin_note_ports_t note_ports_ext = {
    note_ports_count,
    note_ports_get
};

#ifndef VST_GUI
static const void *register_gui_wrapper(const clap_plugin_t* plugin, const char *id)
{
    return nullptr;
}
#endif

static const void* my_get_extension(const clap_plugin_t* plugin,
                                    const char* id) {
    std::string s_id = id;
    clap_debug_logger("clap_plugin", ("my_get_extension " + s_id).c_str());
    if (s_id == CLAP_EXT_PARAMS)
        return &params_ext;

    if (s_id == CLAP_EXT_STATE)
        return &state_ext;

    if (s_id == CLAP_EXT_AUDIO_PORTS)
        return &audio_ports_ext;

    if (s_id == CLAP_EXT_NOTE_PORTS)
        return &note_ports_ext;
    
    auto gui = register_gui_wrapper(plugin, id);
    if (gui)
        return gui;

    return nullptr;
}

clap_plugin_descriptor_t _descr;
bool _descr_init = false;
static const char *_empty_string = "";

static const char* _synth_features[] = {
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_STEREO,
    CLAP_PLUGIN_FEATURE_SYNTHESIZER,
    nullptr
};

static const char* _fx_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    CLAP_PLUGIN_FEATURE_MIXING,
    nullptr
};

static const clap_plugin_descriptor_t *get_descriptor_from_vst()
{
    clap_debug_logger("clap_plugin", "get_descriptor_from_vst");
    if (!_descr_init)
    {
        _descr_init = true;
        memset(&_descr, 0, sizeof(_descr));
        _descr.clap_version = CLAP_VERSION;
        _descr.id = get_vst_plugin_unique_id();
        _descr.name = get_vst_plugin_name();
        _descr.vendor = get_vst_plugin_vendor();
        _descr.version = get_vst_plugin_version();
        _descr.manual_url = _empty_string;
        _descr.support_url = _empty_string;
        _descr.url = _empty_string;
        _descr.description = _empty_string;
        _descr.features = vst_is_synth() ? _synth_features : _fx_features;
    }
    return &_descr;
}

static const clap_plugin_t *create_plugin(const clap_plugin_factory_t *factory,
                                          const clap_host_t *host,
                                          const char *plugin_id)
{
    clap_debug_logger("clap_plugin", "create_plugin");
    if (std::string{plugin_id} != get_vst_plugin_unique_id())
        return nullptr;

    MyClapPlugin *p = new MyClapPlugin();
    memset(p, 0, sizeof(*p));

    p->host = host;

    p->clap.desc = get_descriptor_from_vst();
    p->clap.plugin_data = p;

    p->clap.init = my_init;
    p->clap.destroy = my_destroy;
    p->clap.activate = my_activate;
    p->clap.deactivate = my_deactivate;
    p->clap.process = my_process;
    p->clap.start_processing = my_start_processing;
    p->clap.stop_processing = my_stop_processing;

    p->clap.get_extension = my_get_extension;

    return &p->clap;
}

/// entry

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t *)
{
    return 1;
}

static const clap_plugin_descriptor_t *factory_get_plugin_descriptor(
    const clap_plugin_factory_t *, uint32_t index)
{
    return index == 0 ? get_descriptor_from_vst() : nullptr;
}

static const clap_plugin_factory_t plugin_factory = {
    factory_get_plugin_count,
    factory_get_plugin_descriptor,
    create_plugin};

const clap_plugin_factory_t *get_clap_plugin_factory()
{
    return &plugin_factory;
}

static const void* get_factory(const char* factory_id) {
    if (std::string{factory_id} == CLAP_PLUGIN_FACTORY_ID)
        return get_clap_plugin_factory();
    return nullptr;
}

static bool entry_init(const char* path) {
    set_plugin_path(path);
    return true;
}

static void entry_deinit(void) {
}

extern "C" {

CLAP_EXPORT extern const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    entry_init,
    entry_deinit,
    get_factory
};

}

/// Implicit glue code mapping

#ifdef AUTO_GLUE

extern bool VSTPlugin_init(VSTPlugin *vst);
extern void VSTPlugin_activate(VSTPlugin *vst, double samplerate);
extern void VSTPlugin_process(VSTPlugin *vst, float **in, float **out, int frames);
extern void VSTPlugin_set_param(VSTPlugin *vst, int index, float value);
extern float VSTPlugin_get_param(VSTPlugin *vst, int index);
extern int VSTPlugin_get_param_count(VSTPlugin *vst);
extern void VSTPlugin_process_note_event(VSTPlugin *vst, int key, int velocity, int type);
extern void *VSTPlugin_get_chunk(VSTPlugin *vst, size_t *size);
extern bool VSTPlugin_set_chunk(VSTPlugin *vst, const void *data, size_t size);
extern void VSTPlugin_destroy(VSTPlugin *vst);
extern void VSTPlugin_get_param_name(VSTPlugin *vst, int index, char *label);
extern void VSTPlugin_get_param_display(VSTPlugin *vst, int index, char *label);
extern void *VSTPlugin_new_plugin(VSTPlugin *vst);
extern bool VSTPlugin_has_ui(VSTPlugin *);
extern void *VSTPlugin_create_editor(VSTPlugin *);

VSTPlugin *create_vst_plugin()
{
    auto vst = new VSTPlugin();
    vst->effect = VSTPlugin_new_plugin(vst);
    vst->init = VSTPlugin_init;
    vst->activate = VSTPlugin_activate;
    vst->process = VSTPlugin_process;
    vst->set_param = VSTPlugin_set_param;
    vst->get_param = VSTPlugin_get_param;
    vst->get_param_count = VSTPlugin_get_param_count;
    vst->process_note_event = VSTPlugin_process_note_event;
    vst->get_chunk = VSTPlugin_get_chunk;
    vst->set_chunk = VSTPlugin_set_chunk;
    vst->destroy = VSTPlugin_destroy;
    vst->get_param_name = VSTPlugin_get_param_name;
    vst->get_param_display = VSTPlugin_get_param_display;
    vst->has_ui = VSTPlugin_has_ui;
    vst->create_editor = VSTPlugin_create_editor;
    return vst;
}
#endif

/// GUI

#ifdef VST_GUI

#include <clap/ext/gui.h>
#include "VSTGui/aeffguieditor.h"
#include "VSTGui/timer.h"

#include <windows.h>

void *hInstance; // Required by VSTGui

static AEffGUIEditorFst* get_editor(const clap_plugin_t *plugin)
{
    clap_debug_logger("clap_gui", "get_editor");
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    clap_debug_logger("clap_gui", p->editor ? "has editor" : "null");
    return (AEffGUIEditorFst*)p->editor;
}

static bool is_api_supported(const clap_plugin_t *plugin, const char *api, bool is_floating)
{
    clap_debug_logger("clap_gui", "is_api_supported");
    return std::string{api} == CLAP_WINDOW_API_WIN32 && !is_floating;
}


static bool get_preferred_api(const clap_plugin_t *plugin,
                        const char **api,
                        bool *is_floating)
{
    clap_debug_logger("clap_gui", "get_preferred_api");
    *api = CLAP_WINDOW_API_WIN32;
    *is_floating = false;
    return true;
}

void time_interval_elapsed(void *plugin)
{
    ((AEffGUIEditorFst*)((MyClapPlugin*)plugin)->editor)->idle();
}

static bool create(const clap_plugin_t *plugin, const char *api, bool is_floating)
{
    clap_debug_logger("clap_gui", "create");
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    p->editor = p->vst->create_editor(p->vst);
    hInstance = GetModuleHandle(nullptr);
    timer_start(33, p);
    clap_debug_logger("clap_gui", "create/out");
    return true;
}

static void destroy(const clap_plugin_t *plugin)
{
    clap_debug_logger("clap_gui", "destroy");
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    timer_stop(p);
    if (p->editor)
    {
        get_editor(plugin)->close();
        delete get_editor(plugin);
    }
}

static bool set_scale(const clap_plugin_t *plugin, double scale)
{
    clap_debug_logger("clap_gui", "set_scale");
    return false;
}

static bool get_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)
{
    clap_debug_logger("clap_gui", "get_size");
    ERect *r = nullptr;
    get_editor(plugin)->getRect(&r);
    if (r)
    {
        *width = r->right - r->left;
        *height = r->bottom - r->top;
        return true;
    }
    return false;
}

static bool can_resize(const clap_plugin_t *plugin)
{
    clap_debug_logger("clap_gui", "can_resize");
    return false;
}

static bool get_resize_hints(const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints)
{
    clap_debug_logger("clap_gui", "get_resize_hints");
    return false;
}

static bool adjust_size(const clap_plugin_t *plugin, uint32_t *width, uint32_t *height)
{
    clap_debug_logger("clap_gui", "adjust_size");
    return false;
}

static bool set_size(const clap_plugin_t *plugin, uint32_t width, uint32_t height)
{
    clap_debug_logger("clap_gui", "set_size");
    return false;
}

static bool set_parent(const clap_plugin_t *plugin, const clap_window_t *window)
{
    clap_debug_logger("clap_gui", "set_parent");
    get_editor(plugin)->open(window->win32);
    return true;
}

static bool set_transient(const clap_plugin_t *plugin, const clap_window_t *window)
{
    clap_debug_logger("clap_gui", "set_transient");
    return false;
}

static void suggest_title(const clap_plugin_t *plugin, const char *title)
{
    clap_debug_logger("clap_gui", "suggest_title");
}

static bool show(const clap_plugin_t *plugin)
{
    clap_debug_logger("clap_gui", "show");
    get_editor(plugin)->getFrame()->setVisible(true);
    return true;
}

static bool hide(const clap_plugin_t *plugin)
{
    clap_debug_logger("clap_gui", "hide");
    get_editor(plugin)->getFrame()->setVisible(false);
    return true;
}

static const clap_plugin_gui_t vstgui_wrapping_gui_ext = {
    is_api_supported,
    get_preferred_api,
    create,
    destroy,
    set_scale,
    get_size,
    can_resize,
    get_resize_hints, 
    adjust_size,
    set_size,
    set_parent,
    set_transient,
    suggest_title,
    show,
    hide,
};

static const void *register_gui_wrapper(const clap_plugin_t* plugin, const char *id)
{
    auto* p = (MyClapPlugin*)plugin->plugin_data;
    if (std::string{id} == CLAP_EXT_GUI && p->vst->has_ui(p->vst))
        return &vstgui_wrapping_gui_ext;
    return nullptr;
}


#endif