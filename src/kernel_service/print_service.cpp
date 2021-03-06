#include <arch/lock.h>
#include <arch/process.h>
#include <com.h>
#include <kernel_service/print_service.h>

lock_type locker = {0};
void print_service()
{
    log("console_out", LOG_INFO) << "loaded print service";
    set_on_request_service(true);
    while (true)
    {
        process_message *msg = read_message();
        if (msg == 0x0)
        {
        }
        else
        {
            lock((&locker));
            log("console_out", LOG_INFO); // this is very insecure :(
            com_write_strn((char *)msg->content_address, msg->content_length);
            msg->has_been_readed = true;
            msg->response = 10; // 10 for yes
            on_request_service_update();

            unlock((&locker));
        }
    }
}
