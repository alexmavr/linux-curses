//EXPERIMENTAL

#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/types.h>		/*Sentinels prevent multiple inclusion.*/
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/rcupdate.h>
#include <asm/atomic.h>

#include <curse/curse_list.h>
#include <curse/curse_types.h>
#include <curse/curse.h>			//Now it is only needed for the macros.

//=====Global data.
/*Pointer to the implemented curse array (loaded at init of syscall).*/
struct syscurse *curse_list_pointer=(struct syscurse *)NULL;
/*Proc node pointer.*/
struct proc_dir_entry *dir_node=(struct proc_dir_entry *)NULL, *output_node=(struct proc_dir_entry *)NULL;

//=====Kernel functions.
/*This is the injection wrapper, which must be in kernel space. This basically is an inline or define directive that checks if curses are activated and if the current process has a curse before calling the proper curse function.*/
inline void curse_k_wrapper (void) {
	struct task_struct *cur;

	if (CURSE_SYSTEM_Q)
		goto out;

	cur = current;
	//call the curse handler if there is a curse
	//if is used for opt, might integrate the handler here
	//ideas?
	if (cur->curse_data.curse_field) {
		int i=1;
		uint64_t c_m=0x0001, c_f = cur->curse_data.curse_field;
		printk(KERN_INFO "Gotta do sth now, whaaat?\n");
		
		//... This is where check and curse take place.
		while ((c_f & c_m) || (c_f)) {		//While the current is active, or there are remaining fields:
			fun_array[i].fun_inject();
			c_f >>= 1;
			i++;
		}
	}

out:
	return;
}

/*This function initializes all needed resources (only) once, during system init.*/
void curse_init (void) {
	int j;
	curse_id_t t;

	//1. Initialize curse lookup table.
	curse_list_pointer=(struct syscurse *)kzalloc((MAX_CURSE_NO+1)*sizeof(struct syscurse), GFP_KERNEL);
	for (j=1, t=0x01; j<MAX_CURSE_NO; j++, t<<=1) {		//ERROR HERE: j starts from 1.	//URGENT: TODO: FIXME: TODO: !!!! :: FIXED:: Check if no_fs_cache works, or revert and check again.
		curse_list_pointer[j].entry=(struct curse_list_entry *)&curse_full_list[j];
		curse_list_pointer[j].curse_bit=t;
		atomic_set(&(curse_list_pointer[j].ref_count), 0);
		curse_list_pointer[j].permissions=(_S_M | _G_M | _U_M);
		SET_INHER(curse_list_pointer[j]);
		curse_list_pointer[j].status=IMPLEMENTED;
	}
	curse_list_pointer[0].status=INVALID_CURSE;
	curse_list_pointer[0].curse_bit=0x0;
	atomic_set(&(curse_list_pointer[0].ref_count), 0);
	curse_list_pointer[0].entry=(struct curse_list_entry *)&curse_full_list[0];

	//2. Initialize active status boolean.	::	Could default on an initial status here (based on build options).
	CURSE_SYSTEM_DOWN;

	//3. Populate entries in /proc filesystem.
	if (!(dir_node = proc_mkdir(PROC_DIR_NAME, NULL)))
		goto out;
	if (!(output_node = create_proc_read_entry(PROC_OUT_NODE_NAME, (S_IRUSR|S_IRGRP|S_IROTH), dir_node, syscurse_list_all, curse_list_pointer)))
		goto out_dirred;

	//FIXME: Is there anything else to be done here?

	goto out;
//out_nodded:
	remove_proc_entry(PROC_OUT_NODE_NAME, dir_node);
out_dirred:
	remove_proc_entry(PROC_DIR_NAME, NULL);
out:
	return;		//Stub: there might be others below.
}