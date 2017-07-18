//
// Created by lgb on 17-1-18.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/user.h>
#include <asm/ptrace.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <elf.h>
#include <android/log.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>

/*#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/threads.h>*/



#if defined(__i386__)
#define pt_regs         user_regs_struct
#elif defined(__aarch64__)
#define pt_regs         user_pt_regs
#define uregs	regs
#define ARM_pc	pc
#define ARM_sp	sp
#define ARM_cpsr	pstate
#define ARM_lr		regs[30]
#define ARM_r0		regs[0]
#define PTRACE_GETREGS PTRACE_GETREGSET
#define PTRACE_SETREGS PTRACE_SETREGSET
#endif

#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define  LOG_TAG "INJECT"
#define  LOGD(fmt, args...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, fmt, ##args)
#define DEBUG_PRINT(format,args...) \
    LOGD(format, ##args)
#else
#define DEBUG_PRINT(format,args...)
#endif

#define CPSR_T_MASK     ( 1u << 5 )

#if defined(__aarch64__)
const char *libc_path = "/system/lib64/libc.so";
const char *linker_path = "/system/bin/linker64";
#else
const char *libc_path = "/system/lib/libc.so";
const char *linker_path = "/system/bin/linker";
#endif

//namespace android {

/*
ATTATCH，指定目标进程，开始调试；

GETREGS，获取目标进程的寄存器，保存现场；

SETREGS，修改PC等相关寄存器，使其指向mmap；

POPETEXT，把so path写入mmap申请的地址空间；

SETRESG，修改PC等相关寄存器，使其指向dlopen，调用dlopen，获取sohandle；

SETRESG，修改PC等相关寄存器，使其指向dlsym， functionaddr = dlsym(sohandle,"functionname");
SETRESG，修改PC等相关寄存器，使其指向functionaddr，调用function；

SETRESG，修改PC等相关寄存器，使其指向dlclose，调用dlclose(sohandle);

SETREGS，恢复现场；

DETACH，解除调试，使其恢复；
*/


//根据name找到pid
int find_pid_of(const char *process_name)
{
    int id;
    pid_t pid = -1;
    DIR* dir;
    FILE *fp;
    char filename[32];
    char cmdline[256];

    struct dirent * entry;

    if (process_name == NULL)
        return -1;

    dir = opendir("/proc");
    if (dir == NULL)
        return -1;

    while((entry = readdir(dir)) != NULL) {
        id = atoi(entry->d_name);
        if (id != 0) {
            sprintf(filename, "/proc/%d/cmdline", id);
            fp = fopen(filename, "r");
            if (fp) {
                fgets(cmdline, sizeof(cmdline), fp);
                fclose(fp);

                if (strcmp(process_name, cmdline) == 0) {
                    /* process found */
                    pid = id;
                    break;
                }
            }
        }
    }

    closedir(dir);
    return pid;
}

int ptrace_attach(pid_t pid)
{
    if (ptrace(PTRACE_ATTACH, pid, NULL, 0) < 0) {
        perror("ptrace_attach");
        DEBUG_PRINT("[-] ptrace_attach -1\n");
        return -1;
    }

    int status = 0;
    waitpid(pid, &status , WUNTRACED);

    return 0;
}

int ptrace_getregs(pid_t pid, struct pt_regs * regs)
{
#if defined (__aarch64__)
		int regset = NT_PRSTATUS;
		struct iovec ioVec;

		ioVec.iov_base = regs;
		ioVec.iov_len = sizeof(*regs);
    if (ptrace(PTRACE_GETREGSET, pid, (void*)(intptr_t)regset, &ioVec) < 0) {
        perror("ptrace_getregs: Can not get register values");
        DEBUG_PRINT("    [+] ptrace_getregs: error\n");
        printf(" io %llx, %d", ioVec.iov_base, ioVec.iov_len);
        return -1;
    }

    return 0;
#else
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0) {
        perror("ptrace_getregs: Can not get register values");
        DEBUG_PRINT("    [+] ptrace_getregs: error\n");
        return -1;
    }

    return 0;
#endif
}

//显然，这里面核心的就是get_module_base函数：
/*
此函数的功能就是通过遍历/proc/pid/maps文件，来找到目的module_name的内存映射起始地址。
由于内存地址的表达方式是startAddrxxxxxxx-endAddrxxxxxxx的，所以会在后面使用strtok(line,"-")来分割字符串
如果pid = -1,表示获取本地进程的某个模块的地址，
否则就是pid进程的某个模块的地址。
*/
void* get_module_base(pid_t pid, const char* module_name)
{
    FILE *fp;
    long addr = 0;
    char *pch;
    char filename[32];
    char line[1024];

    if (pid < 0) {
        /* self process */
        snprintf(filename, sizeof(filename), "/proc/self/maps", pid);
    } else {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }

    fp = fopen(filename, "r");
    DEBUG_PRINT("    [+] get_module_base %d, %s, %s\n", pid, filename, module_name);
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            //DEBUG_PRINT("    [+] get_module_base line= %s\n", line);
            if (strstr(line, module_name)) {
				//分解字符串为一组字符串。line为要分解的字符串，"-"为分隔符字符串。
                pch = strtok( line, "-" );
				//将参数pch字符串根据参数base(表示进制)来转换成无符号的长整型数
                addr = strtoull( pch, NULL, 16 );

                if (addr == 0x8000)
                    addr = 0;

                break;
            }
        }

        fclose(fp) ;
    }

    return (void *)addr;
}

/*
该函数为一个封装函数，通过调用get_module_base函数来获取目的进程的某个模块的起始地址，然后通过公式计算出指定函数在目的进程的起始地址。
*/
void* get_remote_addr(pid_t target_pid, const char* module_name, void* local_addr)
{
    void* local_handle, *remote_handle;

	//获取本地某个模块的起始地址
    local_handle = get_module_base(-1, module_name);
    //获取远程pid的某个模块的起始地址
    remote_handle = get_module_base(target_pid, module_name);
    /*if (remote_handle == 0 ) {      //有些应用只加载32位的so
        long temp;
        if (strcmp(module_name, "/system/lib64/libc.so") == 0) {
            local_handle = get_module_base(348, "/system/lib/libc.so");
            remote_handle = get_module_base(target_pid, "/system/lib/libc.so");

            //temp = (long) remote_handle;
            //DEBUG_PRINT("    [+] original: remote[%d]\n", temp);
        } else if (strcmp(module_name, "/system/bin/linker64") == 0) {
            local_handle = get_module_base(348, "/system/bin/linker");
            remote_handle = get_module_base(target_pid, "/system/bin/linker");

            //temp = (long) remote_handle;
            //DEBUG_PRINT("    [+] original: remote[%d]\n", temp);
        }
    }*/

    DEBUG_PRINT("    [+] get_remote_addr: local[%llx], remote[%llx]\n", local_handle, remote_handle);
    //由于module_name函数地址相对于动态库的装载地址是固定不变的，即:local_handle - local_addr = remote_handle - remote_addr
    //所以目标进程的module_name的起始地址 remote_addr = local_addr - local_handle + remote_handle
    void * ret_addr = (void *)((uintptr_t)local_addr - (uintptr_t)local_handle + (uintptr_t)remote_handle);

#if defined(__i386__)
    if (!strcmp(module_name, libc_path)) {
        ret_addr += 2;
    }
#endif
    return ret_addr;
}


/*
Func : 将size字节的data数据写入到pid进程的dest地址处
@param dest: 目的进程的栈地址
@param data: 需要写入的数据的起始地址
@param size: 需要写入的数据的大小，以字节为单位
*/
int ptrace_writedata(pid_t pid, uint8_t *dest, uint8_t *data, const size_t size)
{
    long i, j, remain;
    uint8_t *laddr;
    size_t bytes_width = sizeof(long);

	//很巧妙的联合体，这样就可以方便的以字节为单位写入4字节数据，再以long为单位ptrace_poketext到栈中
    union u {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / bytes_width;
    remain = size % bytes_width;

    laddr = data;

	//先以4字节为单位进行数据写入

    for (i = 0; i < j; i ++) {
        memcpy(d.chars, laddr, bytes_width);
        /*往内存地址中写入一个字节。pid表示被跟踪的子进程，内存地址由addr给出，data为所要写入的数*/
        int result = ptrace(PTRACE_POKETEXT, pid, dest, d.val);
        DEBUG_PRINT("    [+] ptrace_writedata() result=%d\n", result);
        dest  += bytes_width;
        laddr += bytes_width;
    }

    if (remain > 0) {
		//为了最大程度的保持原栈的数据，先读取dest的long数据，然后只更改其中的前remain字节，再写回
        d.val = ptrace(PTRACE_PEEKTEXT, pid, dest, 0);
        for (i = 0; i < remain; i ++) {
            d.chars[i] = *laddr ++;
        }

        ptrace(PTRACE_POKETEXT, pid, dest, d.val);
    }

    return 0;
}

int ptrace_continue(pid_t pid)
{
    if (ptrace(PTRACE_CONT, pid, NULL, 0) < 0) {
        DEBUG_PRINT("    [+] ptrace_continue() error\n");
        perror("ptrace_cont");
        return -1;
    }
    DEBUG_PRINT("    [+] ptrace_continue()\n");
    return 0;
}


int ptrace_setregs(pid_t pid, struct pt_regs * regs)
{
#if defined (__aarch64__)
		int regset = NT_PRSTATUS;
		struct iovec ioVec;

		ioVec.iov_base = regs;
		ioVec.iov_len = sizeof(*regs);
    if (ptrace(PTRACE_SETREGSET, pid, (void*)(intptr_t)regset, &ioVec) < 0) {
        perror("ptrace_setregs: Can not get register values");
        return -1;
    }

    return 0;
#else
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0) {
        perror("ptrace_setregs: Can not set register values");
        return -1;
    }

    return 0;
#endif
}

/*
功能总结：
1，将要执行的指令写入寄存器中，指令长度大于4个long的话，需要将剩余的指令通过ptrace_writedata函数写入栈中；
2，使用ptrace_continue函数运行目的进程，直到目的进程返回状态值0xb7f（对该值的分析见后面红字）；
3，函数执行完之后，目标进程挂起，使用ptrace_getregs函数获取当前的所有寄存器值，方便后面使用ptrace_retval函数获取函数的返回值。
*/
#if defined(__arm__) || defined(__aarch64__)
int ptrace_call(pid_t pid, uintptr_t addr, long *params, int num_params, struct pt_regs* regs)
{
    int i;
#if defined(__arm__)
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 1\n");
    int num_param_registers = 4;
#elif defined(__aarch64__)
    DEBUG_PRINT("    [+] __aarch64__ ptrace_getregs: 1\n");
    int num_param_registers = 8;
#endif

    for (i = 0; i < num_params && i < num_param_registers; i ++) {
        regs->uregs[i] = params[i];
    }
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 2 \n");
    //
    // push remained params onto stack
    //
    if (i < num_params) {
        regs->ARM_sp -= (num_params - i) * sizeof(long) ;//(intptr_t) (void*)
        //ptrace_writedata(pid_t pid, uint8_t *dest, uint8_t *data, const size_t size)
        ptrace_writedata(pid, (uint8_t*)regs->ARM_sp, (uint8_t*)&params[i], (num_params - i) * sizeof(long));
    }
    //将PC寄存器值设为目标函数的地址
    regs->ARM_pc = addr;
	//进行指令集判断
    if (regs->ARM_pc & 1) {
        /* thumb */
        regs->ARM_pc &= (~1u);
		// #define CPSR_T_MASK  ( 1u << 5 )  CPSR为程序状态寄存器
        regs->ARM_cpsr |= CPSR_T_MASK;
    } else {
        /* arm */
        regs->ARM_cpsr &= ~CPSR_T_MASK;
    }
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 3\n");
    //设置子程序的返回地址为空，以便函数执行完后，返回到null地址，产生SIGSEGV错误，详细作用见后面的红字分析
    regs->ARM_lr = 0;

	/*
    *Ptrace_setregs就是将修改后的regs写入寄存器中，然后调用ptrace_continue来执行我们指定的代码
    */
    if (ptrace_setregs(pid, regs) == -1
            || ptrace_continue(pid) == -1) {
        printf("error\n");
        return -1;
    }

    int stat = 0;
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 4\n");
    waitpid(pid, &stat, WUNTRACED);
	/* WUNTRACED告诉waitpid，如果子进程进入暂停状态，那么就立即返回。如果是被ptrace的子进程，那么即使不提供WUNTRACED参数，也会在子进程进入暂停状态的时候立即返回。
	对于使用ptrace_cont运行的子进程，它会在3种情况下进入暂停状态：①下一次系统调用；②子进程退出；③子进程的执行发生错误。
	这里的0xb7f就表示子进程进入了暂停状态，且发送的错误信号为11(SIGSEGV)，它表示试图访问未分配给自己的内存, 或试图往没有写权限的内存地址写数据。
	那么什么时候会发生这种错误呢？显然，当子进程执行完注入的函数后，由于我们在前面设置了regs->ARM_lr = 0，它就会返回到0地址处继续执行，这样就会产生SIGSEGV了！*/

	//这个循环是否必须我还不确定。因为目前每次ptrace_call调用必定会返回0xb7f，不过在这也算是增加容错性吧~

	//通过看ndk的源码sys/wait.h以及man waitpid可以知道这个0xb7f的具体作用。首先说一下stat的值：高2字节用于表示导致子进程的退出或暂停状态信号值，
	//低2字节表示子进程是退出(0x0)还是暂停(0x7f)状态。0xb7f就表示子进程为暂停状态，导致它暂停的信号量为11即sigsegv错误。
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 5\n");
	while (stat != 0xb7f) {
        if (ptrace_continue(pid) == -1) {
            printf("error\n");
            DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 5  ptrace_continue() error\n");
            return -1;
        }
        DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 5  waitpid()\n");
        waitpid(pid, &stat, WUNTRACED);
    }
    DEBUG_PRINT("    [+] __arm__ ptrace_getregs: 6\n");
    return 0;
}

#elif defined(__i386__)
long ptrace_call(pid_t pid, uintptr_t addr, long *params, int num_params, struct user_regs_struct * regs)
{   DEBUG_PRINT("    [+] ptrace_getregs: 1\n");
    regs->esp -= (num_params) * sizeof(long) ;
    ptrace_writedata(pid, (void *)regs->esp, (uint8_t *)params, (num_params) * sizeof(long));
    DEBUG_PRINT("    [+] ptrace_getregs: 2\n");
    long tmp_addr = 0x00;
    regs->esp -= sizeof(long);
    ptrace_writedata(pid, regs->esp, (char *)&tmp_addr, sizeof(tmp_addr));
    DEBUG_PRINT("    [+] ptrace_getregs: 3\n");
    regs->eip = addr;
    if (ptrace_setregs(pid, regs) == -1
            || ptrace_continue( pid) == -1) {
        printf("error\n");
        return -1;
    }
    DEBUG_PRINT("    [+] ptrace_getregs: 4\n");
    int stat = 0;
    waitpid(pid, &stat, WUNTRACED);
    while (stat != 0xb7f) {
        if (ptrace_continue(pid) == -1) {
            printf("error\n");
            return -1;
        }
        waitpid(pid, &stat, WUNTRACED);
    }
    DEBUG_PRINT("    [+] ptrace_getregs: 5\n");
    return 0;
}
#else
#error "Not supported"
#endif

uint64_t ptrace_retval(struct pt_regs * regs)
{
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_r0;
#elif defined(__i386__)
    return regs->eax;
#else
#error "Not supported"
#endif
}

uint64_t ptrace_ip(struct pt_regs * regs)
{
#if defined(__arm__) || defined(__aarch64__)
    return regs->ARM_pc;
#elif defined(__i386__)
    return regs->eip;
#else
#error "Not supported"
#endif
}

//总结一下ptrace_call_wrapper，它的完成两个功能：
//一是调用ptrace_call函数来执行指定函数，执行完后将子进程挂起；
//二是调用ptrace_getregs函数获取所有寄存器的值，主要是为了获取r0即函数的返回值。
int ptrace_call_wrapper(pid_t target_pid, const char * func_name, void * func_addr, long * parameters, int param_num, struct pt_regs * regs)
{
    DEBUG_PRINT("    [+] Calling %s in target process...\n", func_name);
    if (ptrace_call(target_pid, (uintptr_t)func_addr, parameters, param_num, regs) == -1)
        return -1;
    DEBUG_PRINT("    [+] Calling %s in target process. ptrace_call() success.\n", func_name);
    if (ptrace_getregs(target_pid, regs) == -1)
        return -1;
    DEBUG_PRINT("    [+] Target process returned from %s, return value=%llx, pc=%llx \n",
            func_name, ptrace_retval(regs), ptrace_ip(regs));
    return 0;
}

int ptrace_readdata(pid_t pid,  uint8_t *src, uint8_t *buf, size_t size)
{
    long i, j, remain;
    uint8_t *laddr;
    size_t bytes_width = sizeof(long);

    union u {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / bytes_width;
    remain = size % bytes_width;

    laddr = buf;

    for (i = 0; i < j; i ++) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, bytes_width);
        src += bytes_width;
        laddr += bytes_width;
    }

    if (remain > 0) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, src, 0);
        memcpy(laddr, d.chars, remain);
    }

    return 0;
}

int ptrace_detach(pid_t pid)
{
    if (ptrace(PTRACE_DETACH, pid, NULL, 0) < 0) {
        perror("ptrace_detach");
        DEBUG_PRINT("    [+] ptrace_detach: error\n");
        return -1;
    }

    return 0;
}

//远程注入
int inject_remote_process(pid_t target_pid, const char *library_path, const char *function_name, const char *param, size_t param_size)
{
    DEBUG_PRINT("[〇] starting inject_remote_process: %d  %s\n", target_pid, param);

    int ret = -1;
    void *mmap_addr, *dlopen_addr, *dlsym_addr, *dlclose_addr, *dlerror_addr;
    void *local_handle, *remote_handle, *dlhandle;
    uint8_t *map_base = 0;
    uint8_t *dlopen_param1_ptr, *dlsym_param2_ptr, *saved_r0_pc_ptr, *inject_param_ptr, *remote_code_ptr, *local_code_ptr;

    struct pt_regs regs, original_regs;
    long parameters[10];

    //①ATTATCH，指定目标进程，开始调试
    if (ptrace_attach(target_pid) == -1)
        goto exit;
    DEBUG_PRINT("[①] ptrace_attach() 指定目标进程，开始调试.\n");

    //②GETREGS，获取目标进程的寄存器，保存现场
    if (ptrace_getregs(target_pid, &regs) == -1)
        goto exit;

    memcpy(&original_regs, &regs, sizeof(regs));    // save original registers
    DEBUG_PRINT("[②] ptrace_getregs() 获取目标进程的寄存器，保存现场\n");

	//③通过get_remote_addr函数获取目的进程的mmap函数的地址，以便为libxxx.so分配内存
	/*需要对(void*)mmap进行说明：这是取得inject本身进程的mmap函数的地址，由于mmap函数在libc.so库中，
	 *为了将libxxx.so加载到目的进程中，就需要使用目的进程的mmap函数，所以需要查找到libc.so库在目的进程的起始地址。
	 */
    mmap_addr = get_remote_addr(target_pid, libc_path, (void *)mmap);
    DEBUG_PRINT("[③] get_remote_addr() 获取目的进程的mmap函数的地址mmap_addr=%llx\n", mmap_addr);

	/* call mmap (null, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC,
	                         MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	匿名申请一块0x4000大小的内存
	*/
    parameters[0] = 0;  // addr
    parameters[1] = 0x4000; // size
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC;  // prot
    parameters[3] = MAP_ANONYMOUS | MAP_PRIVATE; // flags
    parameters[4] = 0; //fd
    parameters[5] = 0; //offset

    if (ptrace_call_wrapper(target_pid, "mmap", mmap_addr, parameters, 6, &regs) == -1)
        goto exit;
    DEBUG_PRINT("[④] ptrace_call_wrapper() -> mmap  success\n");

	//⑤从寄存器中获取mmap函数的返回值，即申请的内存首地址： (intptr_t)
    map_base =(uint8_t*) ptrace_retval(&regs);
    DEBUG_PRINT("[⑤] ptrace_retval() 从寄存器中获取mmap函数的返回值\n");

	//⑥依次获取linker中dlopen、dlsym、dlclose、dlerror函数的地址:
    dlopen_addr = get_remote_addr( target_pid, linker_path, (void *)dlopen );
    dlsym_addr = get_remote_addr( target_pid, linker_path, (void *)dlsym );
    dlclose_addr = get_remote_addr( target_pid, linker_path, (void *)dlclose );
    dlerror_addr = get_remote_addr( target_pid, linker_path, (void *)dlerror );

    DEBUG_PRINT("[⑥] get_remote_addr: dlopen=%llx, dlsym=%llx, dlclose=%llx, dlerror=%llx\n",
            dlopen_addr, dlsym_addr, dlclose_addr, dlerror_addr);

	//⑦调用dlopen函数：
	/*
	1将要注入的so名写入前面mmap出来的内存
	2写入dlopen代码
	3执行dlopen("libxxx.so", RTLD_NOW | RTLD_GLOBAL)
	    RTLD_NOW： 需要在dlopen返回前，解析出所有未定义符号，如果解析不出来，在dlopen会返回NULL
	    RTLD_GLOBAL：动态库中定义的符号可被其后打开的其它库解析。
	4取得dlopen的返回值，存放在sohandle变量中
	*/
    ptrace_writedata(target_pid, map_base, (uint8_t*)library_path, strlen(library_path) + 1);
    DEBUG_PRINT("[⑦-0] ptrace_retval() 从寄存器中获取mmap函数的返回值\n");

    parameters[0] = (long)map_base;
    parameters[1] = RTLD_NOW| RTLD_GLOBAL;

    if (ptrace_call_wrapper(target_pid, "dlopen", dlopen_addr, parameters, 2, &regs) == -1)
        goto exit;
    DEBUG_PRINT("[⑦-1] ptrace_call_wrapper() -> dlopen\n");

    // 在目标进程内调用 dlopen函数加载要注入的 so ，这一步成功后，so已经被注入目标进程的地址空间内了
    void* sohandle = (void*)ptrace_retval(&regs);
    DEBUG_PRINT("[⑦-2] ptrace_retval()  动态库已经被注入目标进程的地址空间内\n");

    if(!sohandle) {
        if (ptrace_call_wrapper(target_pid, "dlerror", dlerror_addr, 0, 0, &regs) == -1)
            goto exit;
        DEBUG_PRINT("[⑦-3] ptrace_call_wrapper() -> dlerror   success\n");

        uint8_t* errret = (uint8_t*)ptrace_retval(&regs);
        uint8_t errbuf[100];
        ptrace_readdata(target_pid, errret, errbuf, 100);
  	}

    //⑧调用dlsym函数
	/*
	等同于hook_entry_addr = (void *)dlsym(sohandle, "hook_entry");
	*/
    #define FUNCTION_NAME_ADDR_OFFSET       0x100
    ptrace_writedata(target_pid, map_base + FUNCTION_NAME_ADDR_OFFSET, (uint8_t*)function_name, strlen(function_name) + 1);
    parameters[0] = (long)sohandle;
    parameters[1] = (long)(map_base + FUNCTION_NAME_ADDR_OFFSET);

    if (ptrace_call_wrapper(target_pid, "dlsym", dlsym_addr, parameters, 2, &regs) == -1)
        goto exit;
    DEBUG_PRINT("[⑧-0] ptrace_call_wrapper() -> dlsym     success\n");

    void* hook_entry_addr = (void*)ptrace_retval(&regs);
    DEBUG_PRINT("[⑧-1] ptrace_retval() hook_entry_addr=%p\n", hook_entry_addr);

	//⑨调用hook_entry函数：
    #define FUNCTION_PARAM_ADDR_OFFSET      0x200
    ptrace_writedata(target_pid, map_base + FUNCTION_PARAM_ADDR_OFFSET, (uint8_t*)param, strlen(param) + 1);
    parameters[0] = (long)(map_base + FUNCTION_PARAM_ADDR_OFFSET);

    if (ptrace_call_wrapper(target_pid, "hook_entry", hook_entry_addr, parameters, 1, &regs) == -1)
        goto exit;
    DEBUG_PRINT("[⑨] ptrace_call_wrapper() -> hook_entry 调用hook_entry函数     success\n");

    parameters[0] = (long)sohandle;
	//⑩调用dlclose关闭lib:
    if (ptrace_call_wrapper(target_pid, "dlclose", (void *)dlclose, parameters, 1, &regs) == -1)
        goto exit;
    DEBUG_PRINT("[⑨] ptrace_call_wrapper() -> dlclose 调用dlclose关闭lib     success\n");

	//⑪恢复现场并退出ptrace:
    ptrace_setregs(target_pid, &original_regs);
    ptrace_detach(target_pid);
    DEBUG_PRINT("[⑪] 恢复现场并退出ptrace\n");
    ret = 0;

exit:
    return ret;
}



int main(int argc, char *argv[]) {

    //cmd:  /data/InjectUtil com.example.lgb.main /data/libhello.so hook_entry /data/data/com.example.lgb.main/cache
    if(argc != 5) {
        printf("the params' error and need 5 params.\n");
        printf("example: /data/InjectUtil com.example.test /data/libmycalculator.so hook_entry /data/data/com.example.test/cache\n");
        return -2;
    }

    printf("params 2: %s\n", argv[1]);
    pid_t target_pid;
    int ret;
    target_pid = find_pid_of(argv[1]);
    if (-1 == target_pid) {
        printf("Can't find the process\n");
        ret = -100;
    } else {
        ret = inject_remote_process(target_pid, argv[2], argv[3],  argv[4], strlen(argv[4]));
    }
    printf("inject_remote_process result is %d\n", ret);
    return ret;
}

//};






