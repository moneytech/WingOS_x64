#pragma once
#include <stdint.h>
#include <klib/graphic_system.h>
#include <kgui/widget.h>
namespace gui {
    class window{
        const char* window_name;
        sys::graphic_context graphic_context;
        uint64_t width;
        uint64_t height;
        widget_list lst;
        uint64_t current_tick;
        // widget* widget_list; for later ;)
    public:
        // create a simple window
        // if you don't want a update_function give just a nullptr,
        // it will just skip the update
        // todo: add a after_draw_update() and a before_draw_update()
        window(const char* name, uint64_t window_width, uint64_t window_height);
        void add_widget(widget* wdget);

        // get mouse position inside the window
        int32_t get_mouse_pos_relative_x();
        int32_t get_mouse_pos_relative_y();

        uint64_t start();
    };
}
