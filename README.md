# clapwrap

This project aims to act as a minimal wrapper that makes old VST2+VSTGUI tech stack plugins compatible with the CLAP plugin standard.

The main thing you need to implement are the following functions (declared in `vst_bridge.h`):
```c
VSTPlugin* create_vst_plugin();

const char *get_vst_plugin_unique_id();
const char *get_vst_plugin_name();
const char *get_vst_plugin_vendor();
const char *get_vst_plugin_version();
bool vst_is_synth();
```

Use `-DAUTO_GLUE` to use a pre-defined `create_vst_plugin()` implementation that maps each `VSTPlugin` function pointer to a function called `VSTPlugin_<field name>`. Stubs for the functions can be found in `examples/glue_template.cpp`.

## GUI support

The wrapper supports VSTGUI through a wrapper that is located in `VSTGui/` directory. To compile the `clapwrap` itself in VSTGUI supporting mode, use compile option `-DVST_GUI`.

Currently supports only Windows platform.

## Building

Building a plugin that uses this wrapper can be done using a command line like this (on Windows):
```batch
g++ -c ../clapwrap/VSTGui/*.cpp -DWIN32 -trigraphs -DWINDOWS
g++ -shared -fPIC -o  ^
    fuzzofdoom.clap -I../clap/include -I../clapwrap -I../../clapwrap/VSTGui *.cpp ^
    ../clapwrap/clapwrap.cpp ^
    vstcontrols.o vstgui.o aeffguieditor.o timer.o ^
    -lole32 -lkernel32 -lgdi32 -lgdiplus -luuid -luser32 -lshell32 ^
    -DUNICODE -DWIN32 -DWINDOWS -DVST_GUI -DAUTO_GLUE
```
