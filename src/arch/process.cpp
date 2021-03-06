#include <arch/64bit.h>
#include <arch/gdt.h>
#include <arch/lock.h>
#include <arch/mem/liballoc.h>
#include <arch/process.h>
#include <arch/smp.h>
#include <arch/sse.h>
#include <com.h>
#include <device/apic.h>
#include <device/local_data.h>
#include <kernel.h>
#include <loggging.h>
#include <utility.h>
#pragma GCC optimize("O0")
extern "C" void irq0_first_jump();
extern "C" void reload_cr3();
process kernel_process;
bool cpu_wait = false;
char temp_esp[8192];
process *nxt;
bool process_locked = true;
bool process_loaded = false;
void lock_process() { process_locked = true; }
void unlock_process() { process_locked = false; }
lock_type task_lock = {0};
extern "C" void start_task()
{
    lock((&task_lock));
}
extern "C" void end_task()
{
    unlock((&task_lock));
}
void main_process_1()
{
    printf("process 1 \n");
    asm("sti");
    while (true)
    {
        asm("int 32");
    }
}

void main_process_2()
{
    printf("process 2 \n");
    asm("sti");
    while (true)
    {

        asm("int 32");
    }
}

void process_smp_basic()
{
    asm("sti");

    while (true)
    {
        //   log("smp", LOG_INFO) << "hello [1] from cpu : " << apic::the()->get_current_processor_id();
        //   unlock_process();
        asm("int 32");
    }
}
void process_smp_basic_2()
{
    asm("sti");

    while (true)
    {
        //  lock_process();
        //  log("smp", LOG_INFO) << "hello [2] from cpu : " << apic::the()->get_current_processor_id();
        //  unlock_process();
        asm("int 32");
    }
}
extern "C" void irq0_first_jump(void);
void init_multi_process(func start)
{
    log("proc", LOG_DEBUG) << "loading multi processing";
    printf("loading process \n");
    process_array = (process *)((uint64_t)malloc(sizeof(process) * MAX_PROCESS + 4096 /* for security */));
    get_current_data()->current_process = nullptr;
    printf("loading process 0 \n");
    for (int i = 0; i < MAX_PROCESS; i++)
    {
        process_array[i].pid = i;
        process_array[i].current_process_state = process_state::PROCESS_AVAILABLE;
        for (int j = 0; j < PROCESS_STACK_SIZE; j++)
        {
            process_array[i].stack[j] = 0;
        }
    }

    log("proc", LOG_INFO) << "loading process 0";
    init_process(main_process_1, true, "testing1", false);

    log("proc", LOG_INFO) << "loading process 1";
    init_process(main_process_2, true, "testing2", false);

    log("proc", LOG_INFO) << "loading process 2";
    init_process(start, true, "kernel process", false);
    log("proc", LOG_INFO) << "loading smp process for cpus :" << smp::the()->processor_count;
    for (int i = 0; i <= smp::the()->processor_count; i++)
    {

        log("proc", LOG_INFO) << "loading process for cpu : " << i;
        init_process(process_smp_basic, true, "smp1", false, i);
        init_process(process_smp_basic_2, true, "smp2", false, i);
    }

    process_loaded = true;

    asm volatile("sti");
    unlock_process();
    while (true)
    {
    }
}
lock_type process_creator_lock = {0};
uint64_t last_process = 0;
uint8_t proc_last_selected_cpu = 0;
process *init_process(func entry_point, bool start_direct, const char *name, bool user, int cpu_target)
{
    lock((&process_creator_lock));
    for (uint64_t i = last_process; i < MAX_PROCESS; i++)
    {
        if (process_loaded == true && i == 0)
        {
            continue;
        }
        if (process_array[i].current_process_state ==
            process_state::PROCESS_AVAILABLE)
        {
            log("proc", LOG_INFO) << "adding process" << i << "entry : " << (uint64_t)entry_point << "name : " << name;
            if (start_direct == true)
            {
                process_array[i].current_process_state = process_state::PROCESS_WAITING;
            }
            else
            {
                process_array[i].current_process_state = process_state::PROCESS_NOT_STARTED;
            }
            int iname = 0;
            for (iname = 0; iname < 128 && name[iname] != 0; iname++)
            {
                process_array[i].process_name[iname] = name[iname];
            }
            process_array[i].global_process_memory = (uint8_t *)get_mem_addr((uint64_t)pmm_alloc(1));
            process_array[i].global_process_memory_length = 4096;
            memzero(process_array[i].global_process_memory, process_array[i].global_process_memory_length);
            process_array[i].last_message_used = 0;
            process_array[i].process_name[iname] = 0;
            process_array[i].entry_point = (uint64_t)entry_point;
            for (int j = 0; j < PROCESS_STACK_SIZE; j++)
            {
                process_array[i].stack[j] = 0;
            }
            process_array[i].rsp =
                ((uint64_t)process_array[i].stack) + PROCESS_STACK_SIZE;
            for (uint64_t j = 0; j < MAX_PROCESS_MESSAGE_QUEUE; j++)
            {
                process_array[i].msg_list[j].entry_free_to_use = true;
                process_array[i].msg_list[j].message_id = j;
            }
            if (cpu_target == -1)
            {

                process_array[i].processor_target = apic::the()->get_current_processor_id();
            }
            else
            {
                if (cpu_target == -2)
                {
                    proc_last_selected_cpu++;
                    if (proc_last_selected_cpu > smp::the()->processor_count)
                    {
                        proc_last_selected_cpu = 0;
                    }
                    cpu_target = proc_last_selected_cpu;
                }
                process_array[i].processor_target = cpu_target;
            }
            uint64_t *rsp = (uint64_t *)process_array[i].rsp;
            InterruptStackFrame *ISF = (InterruptStackFrame *)(rsp - (sizeof(InterruptStackFrame)));
            memset(ISF, 0, sizeof(InterruptStackFrame));
            ISF->rip = (uint64_t)entry_point;
            ISF->ss = SLTR_KERNEL_DATA;
            ISF->cs = SLTR_KERNEL_CODE;
            ISF->rflags = 0x286;
            ISF->rsp = (uint64_t)ISF;
            if (user)
            {

                process_array[i].page_directory = (uint64_t)new_vmm_page_dir();
            }
            else
            {
                process_array[i].page_directory = (uint64_t)get_current_data()->page_table;
            }
            process_array[i].rsp = (uint64_t)ISF;
            process_array[i].is_ORS = false;
            if (get_current_data()->current_process == 0x0)
            {
                get_current_data()->current_process = &process_array[i];
            }

            unlock((&process_creator_lock));
            last_process = i + 1;
            return &process_array[i];
        }
    }
    if (last_process == 0)
    {
        log("proc", LOG_ERROR) << "init_process : no free process found";
        unlock((&process_creator_lock));
        return nullptr;
    }
    else
    {
        last_process = 0;
        return init_process(entry_point, start_direct, name, user, cpu_target);
    }
}

extern "C" uint64_t switch_context(InterruptStackFrame *current_Isf, process *next)
{

    if (process_locked)
    {
        return (uint64_t)current_Isf; // early return
    }
    // log("proc", LOG_INFO) << "next process : " << next->process_name << "cpu : " << next->processor_target;
    if (next == NULL)
    {
        if (cpu_wait)
        {
            cpu_wait = false;
        }
        return (uint64_t)current_Isf; // early return
    }
    if (get_current_data()->current_process == NULL)
    {
        // current_Isf = (InterruptStackFrame *)next->rsp;
        // memcpy(current_Isf, (void *)next->rsp, sizeof(InterruptStackFrame));
        next->current_process_state = process_state::PROCESS_RUNNING;

        get_current_data()->current_process = next;
        load_sse_context(get_current_data()->current_process->sse_context);
        task_update_switch(next);
        if (cpu_wait)
        {
            cpu_wait = false;
        }
        return next->rsp;
    }
    else
    {
        get_current_data()->current_process->current_process_state = PROCESS_WAITING;
        get_current_data()->current_process->rsp = (uint64_t)current_Isf;
        save_sse_context(get_current_data()->current_process->sse_context);
        //memcpy(current_Isf, (void *)next->rsp, sizeof(InterruptStackFrame));
        //  current_Isf->rsp = next->rsp;
        // current_Isf = (InterruptStackFrame *)next->rsp;
        next->current_process_state = process_state::PROCESS_RUNNING;
        get_current_data()->current_process = next;
        load_sse_context(get_current_data()->current_process->sse_context);
        task_update_switch(next);
        if (cpu_wait)
        {
            cpu_wait = false;
        }
        return next->rsp;
    }
}

extern "C" process *get_next_process(uint64_t current_id)
{
    if (process_locked)
    {
        return get_current_data()->current_process;
    }

    uint64_t dad = apic::the()->get_current_processor_id();
    for (uint64_t i = current_id + 1; i < MAX_PROCESS; i++)
    {

        if ((process_array[i].current_process_state ==
             process_state::PROCESS_WAITING) &&
            i != 0 && process_array[i].processor_target == dad)
        {

            if (process_array[i].is_ORS == true && process_array[i].should_be_active == false)
            {
                continue;
            }
            return &process_array[i];
        }
    }
    for (uint64_t i = 0; i < MAX_PROCESS; i++)
    { // we do a loop
        if ((process_array[i].current_process_state ==
             process_state::PROCESS_WAITING) &&
            i != 0 && process_array[i].processor_target == dad)
        {
            if (process_array[i].is_ORS == true && process_array[i].should_be_active == false)
            {
                continue;
            }
            return &process_array[i];
        }
    }

    log("proc", LOG_ERROR) << "no process found";
    log("proc", LOG_ERROR) << "from cpu : " << apic::the()->get_current_processor_id();
    log("proc", LOG_ERROR) << "maybe from : " << get_current_data()->current_process->process_name;
    dump_process();
    return get_current_data()->current_process;
}
uint16_t counter = 0;
extern "C" uint64_t irq_0_process_handler(InterruptStackFrame *isf)
{

    if (process_locked)
    {
        //log("proc", LOG_INFO) << "process locked";
        return (uint64_t)isf;
    }

    if (apic::the()->get_current_processor_id() == 0)
    {
        /*    counter++;
        if (counter > 500)
        {
            counter = 0;

            dump_process();
        }*/
        for (int i = 0; i <= smp::the()->processor_count; i++)
        {
            if (i != apic::the()->get_current_processor_id())
            {
                apic::the()->send_int(i, 100);
            }
        }
    }
    if (get_current_data()->current_process == NULL)
    {
        process *i = get_next_process(0);
        if (i == 0)
        {
            log("proc", LOG_INFO) << "get next process returned 0";
            return (uint64_t)isf;
        }
        return switch_context(isf, i);
    }
    else
    {
        process *i = get_next_process(get_current_data()->current_process->pid);
        if (i == nullptr)
        {
            log("proc", LOG_INFO) << "get next process returned nullptr";
            log("proc", LOG_INFO) << "in cpu" << apic::the()->get_current_processor_id();
            return (uint64_t)isf;
        }
        return switch_context(isf, i);
    }
}

extern "C" void task_update_switch(process *next)
{

    tss_set_rsp0((uint64_t)next->stack + PROCESS_STACK_SIZE);

    get_current_data()->page_table = (uint64_t *)next->page_directory;
    update_paging();
}

void add_thread_map(process *p, uint64_t from, uint64_t to, uint64_t length)
{

    for (uint64_t i = 0; i < length; i++)
    {

        map_page((main_page_table *)p->page_directory, from + i * PAGE_SIZE, to + i * PAGE_SIZE, PAGE_TABLE_FLAGS);
    }
}
uint64_t get_pid_from_process_name(const char *name)
{
    for (int i = 0; i < MAX_PROCESS; i++)
    {
        if (process_array[i].current_process_state == PROCESS_WAITING || process_array[i].current_process_state == PROCESS_RUNNING)
        {

            if (strcmp(name, process_array[i].process_name) == 0)
            {
                return process_array[i].pid;
            }
        }
    }
    return -1;
}
process_message *send_message(uint64_t data_addr, uint64_t data_length, const char *to_process)
{

    for (int i = 0; i < MAX_PROCESS; i++)
    {
        if (process_array[i].current_process_state == PROCESS_WAITING || process_array[i].current_process_state == PROCESS_RUNNING)
        {

            if (strcmp(to_process, process_array[i].process_name) == 0)
            {
                for (int j = process_array[i].last_message_used; j < MAX_PROCESS_MESSAGE_QUEUE; j++)
                {
                    if (process_array[i].msg_list[j].entry_free_to_use == true)
                    {
                        process_array[i].last_message_used = j;
                        process_message *todo = &process_array[i].msg_list[j];
                        todo->entry_free_to_use = false;
                        todo->has_been_readed = false;
                        todo->content_length = data_length;
                        uint64_t memory_kernel_copy = (uint64_t)malloc(data_length);
                        memcpy((void *)memory_kernel_copy, (void *)data_addr, data_length);
                        todo->content_address = memory_kernel_copy;
                        todo->from_pid = get_current_data()->current_process->pid;
                        todo->to_pid = process_array[i].pid;
                        todo->response = -1;
                        process_message *copy = (process_message *)malloc(sizeof(process_message));
                        *copy = *todo;
                        copy->content_address = -1;
                        process_array[i].should_be_active = true;
                        return copy;
                    }
                } // every left message has been checked
                for (int j = 0; j < MAX_PROCESS_MESSAGE_QUEUE; j++)
                {
                    if (process_array[i].msg_list[j].entry_free_to_use == true)
                    {
                        process_array[i].last_message_used = 0;
                        process_message *todo = &process_array[i].msg_list[j];
                        todo->entry_free_to_use = false;
                        todo->has_been_readed = false;
                        todo->content_length = data_length;
                        uint64_t memory_kernel_copy = (uint64_t)malloc(data_length);
                        memcpy((void *)memory_kernel_copy, (void *)data_addr, data_length);
                        todo->content_address = memory_kernel_copy;
                        todo->from_pid = get_current_data()->current_process->pid;
                        todo->to_pid = process_array[i].pid;
                        todo->response = -1;
                        process_message *copy = (process_message *)malloc(sizeof(process_message));
                        *copy = *todo;
                        copy->content_address = -1;
                        process_array[i].should_be_active = true;
                        return copy;
                    }
                }
            }
        }
    }
    log("process", LOG_ERROR) << "trying to send a message to : " << to_process << "and not founded it :(";

    return nullptr;
}
void dump_process()
{
    lock_process();
    for (int i = 0; i < MAX_PROCESS; i++)
    {
        if (process_array[i].current_process_state != PROCESS_AVAILABLE)
        {

            log("proc", LOG_DEBUG) << "info for process : " << i;
            log("proc", LOG_INFO) << "process name     : " << process_array[i].process_name;
            log("proc", LOG_INFO) << "process state    : " << process_array[i].current_process_state;
            log("proc", LOG_INFO) << "process cpu      : " << process_array[i].processor_target;
        }
    }
    unlock_process();
}
process_message *read_message()
{
    for (uint64_t i = 0; i < MAX_PROCESS_MESSAGE_QUEUE; i++)
    {
        if (get_current_data()->current_process->msg_list[i].entry_free_to_use == false)
        {
            if (get_current_data()->current_process->msg_list[i].has_been_readed == false)
            {
                return &get_current_data()->current_process->msg_list[i];
            }
        }
    }
    return 0x0;
}
uint64_t message_response(process_message *message_id)
{

    if (message_id->to_pid > MAX_PROCESS)
    {
        log("process", LOG_ERROR) << "not valid pid in message response" << message_id->to_pid;
        return -1;
    }
    if (message_id->from_pid != get_current_data()->current_process->pid)
    {
        log("process", LOG_ERROR) << "not valid process from" << message_id->from_pid;
        return -1;
    }
    if (process_array[message_id->to_pid].current_process_state != PROCESS_WAITING && process_array[message_id->to_pid].current_process_state != PROCESS_RUNNING)
    {
        log("process", LOG_ERROR) << "trying to send a message to a not started pid" << message_id->to_pid;
        return -1;
    }

    process_message *msg = &process_array[message_id->to_pid].msg_list[message_id->message_id];
    if (msg->has_been_readed == false)
    {
        //log("process", LOG_ERROR) << "message has not been readed" << process_array[message_id->to_pid].process_name;
        return -2; // return -2 = wait again
    }
    else if (msg->entry_free_to_use == true)
    {
        log("process", LOG_ERROR) << "message has already been readed";
        return -3;
    }
    else
    {
        msg->entry_free_to_use = true;
        free(message_id);
        free((void *)msg->content_address);
        return msg->response;
    }
    return -1;
}
void set_on_request_service(bool is_ORS)
{
    if (is_ORS == true)
    {
        log("process", LOG_INFO) << "setting process : " << get_current_data()->current_process->process_name << " (on request service mode)";
    }
    get_current_data()->current_process->is_ORS = is_ORS;
    get_current_data()->current_process->should_be_active = false;
}
void on_request_service_update()
{
    get_current_data()->current_process->should_be_active = false;
}
uint64_t get_process_global_data_copy(uint64_t offset, const char *process_name)
{
    uint64_t pid = get_pid_from_process_name(process_name);
    if (pid == -1)
    {
        log("process", LOG_ERROR) << "get global data copy, trying to get a non existant process : " << process_name;
        return -1;
    }
    if (process_array[pid].global_process_memory_length < offset + sizeof(uint64_t))
    {
        log("process", LOG_ERROR) << "getting out of range process data";
        return -1;
    }
    else
    {
        uint8_t *data_from = process_array[pid].global_process_memory;

        return *((uint64_t *)(data_from + offset));
    }
}
void *get_current_process_global_data(uint64_t offset, uint64_t length)
{
    if (get_current_data()->current_process->global_process_memory_length < offset + length)
    {
        log("process", LOG_ERROR) << "getting out of range process data";
        return nullptr;
    }
    else
    {
        return get_current_data()->current_process->global_process_memory + offset;
    }
}
