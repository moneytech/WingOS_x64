#include <klib/syscall.h>
#include <klib/process_message.h>
#include <klib/mem_util.h>
#include <klib/raw_graphic.h>
#include <stdio.h>
#include <klib/graphic_system.h>

#include <klib/mouse.h>
#include <klib/process_message.h>
#include <klib/mem_util.h>
#define MAX_WINDOW 1024


// [!] BEFORE READING THIS CODE
// [!] EVERYTHING HERE WILL BE DIVIDED IN MULTIPLE FILE FOR THE MOMENT IT IS LIKE THAT
// [!] I WILL CLEAN UP EVERYTHING WHEN I WILL BE ABLE JUST TO CLEAR A WINDOW FROM AN APPLICATION

struct raw_window_data{
    uint64_t wid;
    uint64_t pid;
    uint64_t px;
    uint64_t py;
    uint64_t width;
    uint64_t height;
    char* window_name;
    sys::pixel* window_front_buffer;
    sys::pixel* window_back_buffer;
    bool used;
};

sys::pixel* front_buffer;
sys::pixel* back_buffer;
uint64_t real_gbuffer_addr = 0x0;
uint64_t scr_width = 0;
uint64_t scr_height = 0;
uint64_t window_count = 0;
void draw_window(raw_window_data window, sys::pixel* buffer){
    const uint64_t win_width = window.width;
    const uint64_t win_height = window.height;
    for(uint64_t x = 0; x < win_width; x++){
        for(uint64_t y = 0; y < win_height; y++){
            const uint64_t pos_f = (x+window.px) + (y+window.py) *scr_width;
            const uint64_t pos_t = (x) + (y) * win_width;
            if(pos_f > scr_width * scr_height){
                return;
            }
            buffer[pos_f].pix = window.window_front_buffer[pos_t].pix;
        }
    }
}
raw_window_data* window_list;


uint64_t create_window(sys::graphic_system_service_protocol*request, uint64_t pid){
    printf("creating window for process %x", pid);
    for(int i = 0; i < MAX_WINDOW; i++){
        if(window_list[i].used == false){
            window_count++;
            window_list[i].used  = true;
            window_list[i].pid = pid;
            window_list[i].width = request->create_window_info.width;
            window_list[i].height = request->create_window_info.height;
            window_list[i].px = 10;
            window_list[i].py = 10;
            window_list[i].window_name = request->create_window_info.name;
            window_list[i].wid = i;
            window_list[i].window_front_buffer = (sys::pixel*)sys::service_malloc(request->create_window_info.width * request->create_window_info.height* sizeof (sys::pixel));
            window_list[i].window_back_buffer  = (sys::pixel*)sys::service_malloc(request->create_window_info.width * request->create_window_info.height* sizeof (sys::pixel));
            return i;
        }
    }

    return -1;
}


uint64_t can_use_window(uint64_t target_wid, uint64_t pid){
    if(target_wid> MAX_WINDOW) {
        printf("graphic : trying to get a window that doesn't exist (wid > MAX_WINDOW)");
        return -2;
    }else if( window_list[target_wid].used == false){
        return -1;
        printf("not valid window id, the window is doesn't exist ");
    }else if(window_list[target_wid].pid != pid ){
        printf("the current window isn't from the current process");
        return 0;
    }
    return 1;
}
uint64_t get_window_back_buffer(sys::graphic_system_service_protocol* request, uint64_t pid){
    if(!can_use_window(request->get_request.window_handler_code, pid)){
        return -2;
    }
    return (uint64_t)window_list[request->get_request.window_handler_code].window_back_buffer;

}
uint64_t window_swap_buffer(sys::graphic_system_service_protocol* request, uint64_t pid){
    if(!can_use_window(request->get_request.window_handler_code, pid)){
        return -2;
    }
    raw_window_data& target = window_list[request->get_request.window_handler_code];
    swap_buffer(target.window_front_buffer, target.window_back_buffer, target.width * target.height);
    return 1;
}
uint64_t get_window_position(sys::graphic_system_service_protocol* request, uint64_t pid){
    if(!can_use_window(request->get_request.window_handler_code, pid)){
        return -2;
    }
    raw_window_data& target = window_list[request->get_request.window_handler_code];
    sys::raw_pos pos = {0};
    pos.rpos.x = target.px;
    pos.rpos.y = target.py;

    return (uint64_t)pos.pos;
}

uint64_t interpret(sys::graphic_system_service_protocol* request, uint64_t pid){
    if(request->request_type == 0){
        printf("graphic error : request null type");
        return -2;
    }else if(request->request_type == sys::GRAPHIC_SYSTEM_REQUEST::CREATE_WINDOW){
        return create_window(request, pid);
    }else if(request->request_type == sys::GRAPHIC_SYSTEM_REQUEST::GET_WINDOW_BACK_BUFFER){
        return get_window_back_buffer(request, pid);
    }else if(request->request_type == sys::GRAPHIC_SYSTEM_REQUEST::SWAP_WINDOW_BUFFER){
        return window_swap_buffer(request, pid);
    }else if(request->request_type == sys::GRAPHIC_SYSTEM_REQUEST::GET_WINDOW_POSITION){
        return get_window_position(request, pid);
    }
    printf("graphic error : request non implemented type");
    return -2;
}


uint64_t m_width = 7;
uint64_t m_height = 8;
char main_cursor_mouse_buffer[] = {
    1,0,0,0,0,0,0,
    1,1,0,0,0,0,0,
    1,2,1,0,0,0,0,
    1,2,2,1,0,0,0,
    1,2,2,2,1,0,0,
    1,2,2,2,2,1,0,
    1,2,2,2,2,2,1,
    1,1,1,1,1,1,1
};



void draw_mouse(uint64_t x,uint64_t y){
    x++;
    y++;
    if(x >= (scr_width - m_width)){
        x = scr_width- m_width;
    }
    if(y >= scr_height- m_height){
        y = scr_height- m_height;
    }
    for(uint64_t ix = 0; ix < m_width; ix++){
        for(uint64_t iy = 0; iy < m_height; iy++){
            const uint64_t m_index = ix + iy * m_width;
            const uint64_t m_col = main_cursor_mouse_buffer[m_index];
              if(m_col == 0){
                continue;
            }
              const uint64_t m_toscreen_idx = (ix + x) + (iy + y) * scr_width;

            if(m_col == 1){
                back_buffer[m_toscreen_idx] = sys::pixel(255,255,255);
            }else{
                back_buffer[m_toscreen_idx] = sys::pixel(0,0,0);

            }
        }
    }
}
int32_t m_x = 0;
int32_t m_y = 0;
sys::process_message mouse_msg_x;
sys::process_message mouse_msg_y;

sys::ps2_device_request mouse_requestx = {0};
sys::ps2_device_request mouse_requesty = {0};
void update_mouse(){

    m_x = sys::get_mouse_x();
    m_y = sys::get_mouse_y();


}
int main(){
    window_count = 0;
    printf("main graphic system service loading... \n");

    real_gbuffer_addr = sys::get_graphic_buffer_addr();
    front_buffer = (sys::pixel*)real_gbuffer_addr;
    scr_width = sys::get_screen_width();
    scr_height = sys::get_screen_height();
    back_buffer = (sys::pixel*)sys::service_malloc(scr_width*scr_height*sizeof (uint32_t));
    window_list = (raw_window_data*)sys::service_malloc(MAX_WINDOW * sizeof (raw_window_data));
    for(int i = 0; i < MAX_WINDOW; i++){
        window_list[i].used = false;
    }
    printf("g buffer addr   : %x \n",real_gbuffer_addr);
    printf("g buffer width  : %x \n",scr_width);
    printf("g buffer height : %x \n",scr_height);
    uint32_t soff = 0;
    while (true) {
        // read all message
        update_mouse();
        while(true){
            sys::raw_process_message* msg = sys::service_read_current_queue();
            if(msg != 0x0){
                sys::graphic_system_service_protocol* pr = (sys::graphic_system_service_protocol*)msg->content_address;

                msg->response = interpret(pr,  msg->from_pid);
                msg->has_been_readed = true;
            }
            break;
        }

        soff++;
        sys::pixel r = sys::pixel(soff);
        raw_clear_buffer(back_buffer, scr_width * scr_height, soff);
        if(window_count != 0){
            for(int i = 0; i < window_count; i++){
                if(window_list[i].used == true){
        //            swap_buffer(window_list[i].window_front_buffer, window_list[i].window_back_buffer, window_list[i].width*window_list[i].height*sizeof (uint32_t));
                    draw_window(window_list[i],back_buffer);
                }
            }
        }
        draw_mouse(m_x,m_y);


        swap_buffer(front_buffer, back_buffer, scr_width*scr_height);
    }
    return 1;
}
