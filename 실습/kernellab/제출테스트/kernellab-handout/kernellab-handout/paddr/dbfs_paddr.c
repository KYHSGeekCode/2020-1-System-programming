#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

struct packet {
	pid_t pid;
	unsigned long vaddr;
	unsigned long paddr;
};


static ssize_t read_output(struct file *fp,
						   char __user *user_buffer,
						   size_t length,
						   loff_t *position)
{
	struct packet thePacket;
	if(copy_from_user(&thePacket,   user_buffer, length))
		return -1;
//	printk("%d %lx %lx\n", (int)thePacket.pid,  thePacket.vaddr, thePacket.paddr);
	struct pid * pid_struct = find_get_pid(thePacket.pid);
	task  = pid_task(pid_struct, PIDTYPE_PID); // Find task_struct using input_pid. Hint: pid_task
	struct mm_struct * mm = task -> mm;
	pgd_t * firstPgd = mm->pgd;
//	printk("pgd_t*: %p %lx\n", firstPgd, *firstPgd);
	pgd_t * thePgd = pgd_offset(mm, thePacket.vaddr);
	p4d_t * theP4d = p4d_offset(thePgd, thePacket.vaddr);
	pud_t * thePud = pud_offset(theP4d, thePacket.vaddr);
	pmd_t * thePmd = pmd_offset(thePud, thePacket.vaddr);
	pte_t * thePte = pte_offset_kernel(thePmd, thePacket.vaddr);
	unsigned long offset = thePacket.vaddr & ~PAGE_MASK;
	unsigned long physicalAddress = (pte_val(*thePte) & PAGE_MASK) | offset;
	unsigned long fourtyEightBitMask = ((1UL << 49) - 1);
//	printk("%lx\n", fourtyEightBitMask);
	physicalAddress &= fourtyEightBitMask;
	thePacket.paddr = physicalAddress;
//	printk("physical: %lx\n", physicalAddress);
	copy_to_user(user_buffer, &thePacket, sizeof(thePacket));
	return 0;
	
}

static const struct file_operations dbfs_fops = {
	.read = read_output,// Mapping file operations with your functions
};

static int __init dbfs_module_init(void)
{
	// Implement init module


	dir = debugfs_create_dir("paddr", NULL);

	if (!dir) {
		printk("Cannot create paddr dir\n");
		return -1;
	}

	// Fill in the arguments below
	output = debugfs_create_file("output", 0777 , dir , NULL, &dbfs_fops );
	
	if(!output) {
		printk("Cannot create output file\n");
		return -1;
	}

	printk("dbfs_paddr module initialize done\n");

	return 0;
}

static void __exit dbfs_module_exit(void)
{
	// Implement exit module
	debugfs_remove_recursive(dir);
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
