#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
#define MAX_THREADS 20
struct thread_msg {
    int pid;
    char content[256];
    bool occupied;
};
static struct thread_msg msg_pool[MAX_THREADS];
static DEFINE_MUTEX(my_pool_lock);

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset){
    int i, slot = -1;
    int current_pid = current->pid;
    mutex_lock(&my_pool_lock);
    for (i = 0; i < MAX_THREADS; i++) {
        if (msg_pool[i].occupied && msg_pool[i].pid == current_pid) {
            slot = i;
            break;
        }
        if (slot == -1 && !msg_pool[i].occupied) {
            slot = i;
        }
    }
    

    
    if (slot != -1) {
        msg_pool[slot].pid = current_pid;
        msg_pool[slot].occupied = true;
        // prevent overflow
        size_t copy_len = (buffer_len > 255) ? 255 : buffer_len;
        //bring data from user space to kernal
        if (copy_from_user(msg_pool[slot].content, ubuf, copy_len)) {
            mutex_unlock(&my_pool_lock);
            return -EFAULT;
        }
        msg_pool[slot].content[copy_len] = '\0';
    }

    mutex_unlock(&my_pool_lock);
    
    return buffer_len;
    /****************/
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset){
    static char local_buf[BUFSIZE];
    int len = 0;
    int i, slot = -1;
    int current_pid = current->pid;

    if (*offset > 0) return 0;

    mutex_lock(&my_pool_lock); 
    /* Look for messages associated with the reading process/thread */
    for (i = 0; i < MAX_THREADS; i++) {
        if (msg_pool[i].occupied && msg_pool[i].pid == current_pid) {
            slot = i;
            break;
        }
    }
    
    /* Format the output with runtime data */
    unsigned long long runtime = current->utime; /* CPU time in nanoseconds */
    len += snprintf(local_buf + len, BUFSIZE - len, 
                "--- Kernel Thread Monitor ---\n"
                "TGID (Process): %d | PID (Thread): %d\n"
                "CPU Runtime: %llu ns | Priority: %d\n",
                current->tgid, current->pid, runtime, (int)current->prio); 

    if (slot != -1) {
        len += snprintf(local_buf + len, BUFSIZE - len, "Latest Log: %s\n", msg_pool[slot].content);
        msg_pool[slot].occupied = false; /* Clear after reading */
    } else {
        len += snprintf(local_buf + len, BUFSIZE - len, "Status: No specific logs for this PID.\n");
    }
    mutex_unlock(&my_pool_lock);

    if (copy_to_user(ubuf, local_buf, len)) return -EFAULT; 
    *offset += len;
    return len;
}

static struct proc_ops Myops = {
    .proc_read = Myread,
    .proc_write = Mywrite,
};

static int My_Kernel_Init(void){
    proc_create(procfs_name, 0644, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");
