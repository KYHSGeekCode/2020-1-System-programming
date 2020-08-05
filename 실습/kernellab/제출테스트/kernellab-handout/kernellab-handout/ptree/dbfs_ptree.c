#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

pid_t  pid=0;
pid_t oldPid = 0;
char * buffer = NULL;
int buflen=0;
static ssize_t write_pid_to_input(struct file *fp, 
                                char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
//	printk("ptree: write_pid_to input called length %zu position %lld with pid %ld\n", length, *position, (long)pid);
	if(pid != oldPid) {
		oldPid = pid;
		if(buffer) {
			kfree(buffer);
		}
		buffer = NULL; // Invalidate cache
	}
    pid_t input_pid = pid;
    //sscanf(user_buffer, "%u", &input_pid);
    struct pid * pid_struct = find_get_pid(input_pid);
	curr = pid_task(pid_struct, PIDTYPE_PID); // Find task_struct using input_pid. Hint: pid_task
	if(!curr) {
		printk("Curr is null\n");
		return 0;
	}
	if(!buffer) {
//			printk("ptree: buffer is null\n");
			int needed = 0;
			struct task_struct * iter = curr;
			int count =0;
			while(iter) {
				count++;	
				if(iter != iter->parent && iter->pid != 1) {
					iter = iter->parent;
				} else {
					break;
				}
			}
//			printk("ptree: count=%d\n", count);
			int *lens = kmalloc(count*sizeof(int), GFP_KERNEL);
			int lenindex = 0;
			iter = curr;
			while(iter) {
				char tcomm[sizeof(iter->comm)];
				get_task_comm(tcomm, iter);
				pid_t thepid = iter->pid;
				lens[lenindex] = snprintf(NULL,0,"%s (%ld)\n", tcomm, (long)thepid);
				needed += lens[lenindex];
				lenindex++;
//				printk("ptree: first: %s (%ld), needed = %d \n", tcomm,(long) thepid, needed);
				if(iter != iter->parent && iter->pid != 1) {
					iter = iter->parent;
				} else {
					break;
				}
			}
			needed++;
//			printk("Needed: %d\n", needed);
			buffer = kmalloc(needed, GFP_KERNEL);
			if(!buffer) {
				printk("Failed to allocate space");
				return -1;
			}
			char *offsetBuf = buffer + needed-1;
			iter = curr;
			lenindex = 0;
			while(iter) {
				char tcomm[sizeof(iter->comm)];
				get_task_comm(tcomm, iter);
				pid_t thepid = iter->pid;
				//char tmp = offsetBuf[0];
				offsetBuf -= lens[lenindex];
				char * tempStr = kmalloc(lens[lenindex]+1, GFP_KERNEL);
				snprintf(tempStr, lens[lenindex]+1, "%s (%ld)\n", tcomm, (long)thepid);
				memcpy(offsetBuf, tempStr, lens[lenindex]);
				kfree(tempStr); 
				//snprintf(offsetBuf, lens[lenindex]+1, "%s (%ld)\n", tcomm, (long)thepid);
				//offsetBuf[lens[lenindex]] = tmp;
				lenindex++; 
				if(iter != iter->parent && iter->pid != 1) {
					iter = iter->parent;
				} else {
					break;
				}
			}
			kfree(lens);
			buffer[needed-1] = '\0';
			*position = 0;
			buflen = needed;
	}
	int left  = buflen - *position;
	int tocopy = left < length? left: length;
	int notcopied = copy_to_user(user_buffer, buffer + *position, tocopy);
	int copied = tocopy-notcopied;
	*position+=copied;
//	printk("ptree: left %d tocopy %d notcopied %d copied %d position %lld\n", left,tocopy,notcopied,copied, *position);
    // Tracing process tree from input_pid to init(1) process
    // Make Output Format string: process_command (process_id)
    return copied;
}

/*
static ssize_t read_op(struct file * fp,
			const char __user *user_buffer,
			size_t length,
			loff_t *position)
{
		
	;
	return 0;
}
*/

static const struct file_operations dbfs_fops = {
        .read = write_pid_to_input,
	//	.read = read_op,
};

static int __init dbfs_module_init(void)
{
    // Implement init module code

    dir = debugfs_create_dir("ptree", NULL);
        
    if (!dir) {
        printk("Cannot create ptree dir\n");
        return -1;
    }

    inputdir = debugfs_create_u32("input", 0666, dir, &pid);
	
    if (!inputdir) {
        printk("Cannot create input file\n");
        return -1;
    }
    ptreedir = debugfs_create_file("ptree", 0444 , dir , NULL, &dbfs_fops); // Find suitable debugfs API
	
    if (!ptreedir) {
        printk("Cannot create ptree file\n");
        return -1;
    }
	
	printk("dbfs_ptree module initialize done\n");

    return 0;
}

static void __exit dbfs_module_exit(void)
{
    // Implement exit module code
	if(buffer) {
		kfree(buffer);
		buffer = NULL;
	}
	debugfs_remove_recursive(dir);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
