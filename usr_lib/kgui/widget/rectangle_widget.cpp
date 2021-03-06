#include <kgui/widget/rectangle_widget.h>
#include <kgui/widget.h>

namespace gui {
    rectangle_widget::rectangle_widget() :col(sys::pixel(0,0,0,0)){
    }
    rectangle_widget::rectangle_widget(uint64_t x, uint64_t y, uint64_t width, uint64_t heigth, sys::pixel color) : col(color){
        widget_x = x;
        widget_y = y;
        widget_width = width;
        widget_height = heigth;
    }
    void rectangle_widget::update_widget() {
    };
    void rectangle_widget::draw_widget(sys::graphic_context& context) {
        context.draw_rectangle(widget_x, widget_y, widget_width, widget_height, col);
    };
    void rectangle_widget::init_widget(void* new_parent){

    };
}
