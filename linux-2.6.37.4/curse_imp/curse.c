/*
 * This file contains the source code for the curse system call.
 *
 * [The functions used by the system call are sourced below it.]
 *
 */
#include <linux/syscalls.h>
#ifdef CONFIG_CURSES

#include <linux/types.h>		/*Sentinels prevent multiple inclusion.*/
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/namei.h>

#include <curse/curse.h>
#include <curse/curse_types.h>

#define CURSE_SYSTEM 0
#define CURSE_TARGETED 1
#define CURSE_REMOTE 2

//=====External declarations.
extern int max_curse_no;
extern struct curse_list_entry *curse_full_list;

//=====Various wrapper functions.
/*This function returns the bitmask for the specified normalized curse index.*/
inline uint64_t bitmask_from_no (int  a_c_id)
{
	return curse_list_pointer[a_c_id].curse_bit;
}

/*This macro expands to the requested field of the requested element of curse_list_pointer array.*/
#define CURSE_FIELD(el, field) (curse_list_pointer[(el)].field)

/*This function checks if current is allowed to change the state of the target proc.*/
static int check_permissions (pid_t target, int type)
{
	struct task_struct *foreign_task;
	const struct cred *foreign_c = NULL, *local_c = NULL;
	uint8_t local_curse_perms;
	uint8_t foreign_curse_perms;
	int ret = -EINVAL;
	unsigned long spinflags;

	spin_lock_irqsave(&((current->curse_data).protection), spinflags);
	local_curse_perms = current->curse_data.permissions;
	spin_unlock_irqrestore(&((current->curse_data).protection), spinflags);
	local_c = get_current_cred();

	switch(type) {
		case CURSE_SYSTEM:
			ret = -EPERM;
			if ((local_c->euid == 0) && (local_curse_perms & _SU_ACTIVE_PERM))
				ret = 1;
			goto out_with_local;
		case CURSE_REMOTE:
		case CURSE_TARGETED:
			ret = -ESRCH;		//FIXME: Sanity check.
			rcu_read_lock();
			foreign_task = find_task_by_vpid(target);
			rcu_read_unlock();
			if (!foreign_task)
				goto out;

			ret = -EINVAL;		//FIXME: Sanity check.
			foreign_c = get_task_cred(foreign_task);

			if (!foreign_c)
				goto out_with_local;
			/* am i root or sudo?? */
			/* do we belong to the same effective user?*/

			spin_lock_irqsave(&((foreign_task->curse_data).protection), spinflags);
			foreign_curse_perms = foreign_task->curse_data.permissions;
			spin_unlock_irqrestore(&((foreign_task->curse_data).protection), spinflags);

			ret = -EPERM;
			if (type == CURSE_TARGETED) {
				if (((local_c->euid == 0) && (local_curse_perms & _SU_ACTIVE_PERM) && (foreign_curse_perms & _SU_PASSIVE_PERM))	||	\
						(((local_c->euid == foreign_c->euid) || (local_c->euid == foreign_c->uid))								&&	\
						 (local_curse_perms & _USR_ACTIVE_PERM) && (foreign_curse_perms & _USR_PASSIVE_PERM)))
					ret = 1;
			} else {
				if ((local_c->euid == 0) || (local_c->euid == foreign_c->euid) || (local_c->euid == foreign_c->uid))
					ret = 1;
			}

	}
	put_cred(foreign_c);
out_with_local:
	put_cred(local_c);
out:
	return ret;
}

/*This function takes a userspace string, and returns: 0 with the inode number in inode_number, or error.*/
static int inode_from_user_path (char __user *path, unsigned long *inode_number)
{
	int ret = -ENOMEM;
	char *kernel_buffer;
	ssize_t len = (sizeof(path)+1);
	struct path tmp;
	umode_t in_mode;

	debug("Length is %d.\n", (int)len);
	if ((kernel_buffer = kzalloc(sizeof(char)*len, GFP_KERNEL)) == NULL)
		goto out;
	ret = -EFAULT;
	if (copy_from_user(kernel_buffer, path, len))
		goto out;
	debug("String is %s.\n", kernel_buffer);

	if ((ret = kern_path(/*transformed path*/ kernel_buffer, /*flags*/ LOOKUP_FOLLOW, &tmp)))
		goto out;
	debug("kern_path return is %d.\n", ret);

	(*inode_number) = tmp.dentry->d_inode->i_ino;
	in_mode = tmp.dentry->d_inode->i_mode;

	debug("inode number is %lu and mode is %d\n", (*inode_number), (int)in_mode);
	if (!(in_mode & S_IXUGO)) {
		ret = -EPERM;
		debug("not executable\n");
	}

	path_put(&tmp);

out:
	return ret;
}

//=====Source syscall sub-functions.
static int syscurse_list_all (char __user *buf)
{
	int ret = -EINVAL;
	size_t length;
	//FIXME: I will add them for support, even if they are unused.
/*
	static size_t offset=0;

	if (len <= 0)
		goto out;
*/
	//length = sizeof(curse_full_list);
	length = sizeof(struct curse_list_entry)*max_curse_no;
//	ret = ((length - offset) >= len) ? len : (length - offset);

	ret = 1;
//	debug("My master you ask me to copy %u bytes, i shall do my best...\n", (unsigned int) length);
	if (copy_to_user(buf, (const char *)&curse_full_list/*+offset*/, length)) {
		ret=-EFAULT;
		goto out;
	}
/*
	offset += ret;
	if (offset == length)
		offset=0;
*/
out:
	return ret;
}

static int syscurse_activate (int curse_no)
{
	int i, ret = -EPERM;

	i = curse_no;
	if ((ret = check_permissions(0, CURSE_SYSTEM)) != 1)
		goto out_ret;

	ret = 1;
	//Found a use for stub curse 0: activates the general curse system without activating any curse.
	if (bitmask_from_no(curse_no)) {								//Activation of an existing curse, activates the system too.
		if (!(CURSE_FIELD(i, status) & ACTIVATED)) {
			CURSE_FIELD(i, status) |= ACTIVATED;
		} else {
			ret = -EINVAL;
			goto out_ret;
		}
	}
	if (!CURSE_SYSTEM_Q)											//On invalid id, system activation.
		CURSE_SYSTEM_UP;

out_ret:
	return ret;
}

static int syscurse_deactivate (int curse_no)
{
	int i, ret = -EPERM;

	if ((ret = check_permissions(0, CURSE_SYSTEM)) != 1)
		goto out_ret;
	i = curse_no;

	ret = 1;
	if (bitmask_from_no(curse_no)) {								//Targeted deactivation is normal.
		if (CURSE_FIELD(i, status) & ACTIVATED) {
			CURSE_FIELD(i, status) &= ~ACTIVATED;
		} else {
			ret = -EINVAL;
			goto out_ret;
		}
	} else if (/*!bitmask_from_no(curse_no) && */ CURSE_SYSTEM_Q)	//Invalid target deactivates the system.
		CURSE_SYSTEM_DOWN;

	//TODO: Do we have to unhook (call close pointer) all the active curses here?	::	No, we simply deactivate. On activation, it will continue as was.

out_ret:
	return ret;
}

static int syscurse_check_curse_activity (int curse_no)
{
	int i, ret = -EINTR;

	if (!CURSE_SYSTEM_Q)
		goto out;

	i = curse_no;
	if (CURSE_FIELD(i, entry)->curse_id == 0xABADDE5C) {
		ret = -EINVAL;
		goto out;
	}
	if (CURSE_FIELD(i, status) & CASTED)
		ret = 1;
	else
		ret = 0;

out:
	return ret;
}

static int syscurse_check_tainted_process (int curse_no, pid_t target)
{
	int err = -EINVAL;
	uint64_t check_bit;
	unsigned long spinflags;
	struct task_struct *target_task;

	if (!(check_bit = bitmask_from_no(curse_no)) || (target <= 0))
		goto out;
	if (!CURSE_SYSTEM_Q)
		goto out;

	err = -ESRCH;
	rcu_read_lock();
	target_task = find_task_by_vpid(target);
	rcu_read_unlock();
	if (!target_task)
		goto out;

	err = -EINVAL;
	if (target <= 0)
		goto out;
	if ((err = check_permissions(target, CURSE_TARGETED)) != 1)
		goto out;
	err = 0;

	//Check if target has an active curse on it.	::	FIXME: Move it to one-liner? Is it better?
	spin_lock_irqsave(&((target_task->curse_data).protection), spinflags);
	if (target_task->curse_data.curse_field & check_bit)
		err = 1;
	else
		err = 0;
	spin_unlock_irqrestore(&((target_task->curse_data).protection), spinflags);

out:
	return err;
}

static int syscurse_ctrl (int curse_no, int ctrl, pid_t pid)
{
	int index, ret = -EINVAL;
	struct task_struct *target_task;
	struct task_curse_struct *cur_curse_field;
	unsigned long flags = 0;
	uint8_t ctrl_masks[] = {_USR_ACTIVE_PERM, _USR_PASSIVE_PERM, _SU_ACTIVE_PERM, _SU_PASSIVE_PERM};
	_Bool set_clr;
	int com_index;

	index = curse_no;

	spin_lock_irqsave(&CURSE_FIELD(index, flag_lock), flags);
	ret = 1;
	switch (ctrl) {		/*Inherritance (on curse_list_pointer array)*/
	case INH_ON		:
		SET_INHER(index);
		break;
	case INH_OFF	:
		CLR_INHER(index);
		break;
	default:
		ret = -1;
	}
	spin_unlock_irqrestore(&CURSE_FIELD(index, flag_lock), flags);

	if (ret == 1)
		goto out;

	rcu_read_lock();
	target_task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (!target_task)
		goto out;
	cur_curse_field = &(target_task->curse_data);

	ret = -EINVAL;
	if (pid <= 0)
		goto out;
	if ((ret = check_permissions(pid, CURSE_REMOTE)) != 1) {
		goto out;
	}

	if ((ctrl >= USR_ACTIVE_PERM_ON) && (ctrl <= SU_PASSIVE_PERM_ON)) {
		set_clr=0;
		com_index = (ctrl - USR_ACTIVE_PERM_ON);
	} else if ((ctrl >= USR_ACTIVE_PERM_OFF) && (ctrl <= SU_PASSIVE_PERM_OFF)) {
		set_clr=1;
		com_index = (ctrl - USR_ACTIVE_PERM_OFF);
	} else {
		set_clr=2;
	}

	spin_lock_irqsave(&(cur_curse_field->protection), flags);
	switch (set_clr) {		/*Permissions (on task_curse_struct struct)*/
		case 0	:
			SET_PERM((*cur_curse_field), ctrl_masks[com_index]);
			break;
		case 1	:
			CLR_PERM((*cur_curse_field), ctrl_masks[com_index]);
			break;
		default	:
			ret = -EINVAL;
	}
	spin_unlock_irqrestore(&(cur_curse_field->protection), flags);

out:
	return ret;
}

static int syscurse_cast (int curse_no, pid_t target)
{
	int err = -EINVAL;
	unsigned long spinflags;
	struct task_struct *target_task;
	int new_index;
	uint64_t new_mask;

	if (!CURSE_SYSTEM_Q)
		goto out;


	err = -ESRCH;
	rcu_read_lock();
	target_task = find_task_by_vpid(target);
	rcu_read_unlock();
	if (!target_task)
		goto out;

	err = -EINVAL;
	if (target <= 0 )
		goto out;
	if ((err = check_permissions(target, CURSE_TARGETED)) != 1)
		goto out;

	err = -EINVAL;
	new_index = curse_no;
	new_mask = CURSE_FIELD(new_index, curse_bit);

	if ((!new_mask) || (!(CURSE_FIELD(new_index, status) & ACTIVATED)))
		goto out;

	spin_lock_irqsave(&((target_task->curse_data).protection), spinflags);
	if (!(target_task->curse_data.curse_field & new_mask)) {
		target_task->curse_data.curse_field |= new_mask;
		atomic_inc(&CURSE_FIELD(new_index, ref_count));
		if (GET_INHER(new_index))
			target_task->curse_data.inherritance |= new_mask;
		else
			target_task->curse_data.inherritance &= (~new_mask);
		CURSE_FIELD(new_index, status) |= CASTED;
		err = 1;
	}
	spin_unlock_irqrestore(&((target_task->curse_data).protection), spinflags);
	CURSE_FIELD(new_index, functions)->fun_init(target_task);	//Call init after cast.

out:
	return err;
}

static int syscurse_lift (int curse_no, pid_t target)
{
	int err = -EINVAL;
	unsigned long spinflags;
	struct task_struct *target_task;
	uint64_t curse_mask;
	int index;

	if (!CURSE_SYSTEM_Q)
		goto out;

	index = curse_no;
	err = -ESRCH;
	rcu_read_lock();
	target_task = find_task_by_vpid(target);
	rcu_read_unlock();
	if (!target_task)
		goto out;

	err = -EINVAL;
	if (target <= 0)
		goto out;
	if ((err = check_permissions(target, CURSE_TARGETED)) != 1)
		goto out;

	err = -EINVAL;
	if (!(curse_mask = CURSE_FIELD(index, curse_bit)))
		goto out;

	spin_lock_irqsave(&((target_task->curse_data).protection), spinflags);
	if (target_task->curse_data.curse_field & curse_mask) {
		target_task->curse_data.curse_field &= (~curse_mask);		//Just to be safe (^= toggles, not clears).
		atomic_dec(&CURSE_FIELD(index, ref_count));
		target_task->curse_data.inherritance &= (~curse_mask);
		if (atomic_read(&CURSE_FIELD(index, ref_count)) == 0)		//Revert curse status to ACTIVATED if ref 0ed-out.	: Could be atomic_dec_and_set.
			CURSE_FIELD(index, status) &= ~CASTED;
		err = 1;
	}
	spin_unlock_irqrestore(&((target_task->curse_data).protection), spinflags);

	CURSE_FIELD(index, functions)->fun_destroy(target_task);	//Call destroy after lift.

out:
	return err;
}

static int syscurse_show_rules (void)
{
	return 0;
}

static int syscurse_add_rule (int curse, char __user *path)
{
	int ret = -EINVAL;
	unsigned long in_num;

	//Find inode
	//Check if executable
	if ((ret = inode_from_user_path(path, &in_num)))
		goto out;

	//Check permissions
	//Check if it is already in saved
	//Else do it

out:
	return ret;
}

static int syscurse_rem_rule (int curse, char *path)
{
	//Find inode
	//Check if it is in saved
	//Check permissions
	//Else do it
	return 0;
}

//=====Syscall kernel source.
/*This is the system call source base function.*/
SYSCALL_DEFINE5 (curse, unsigned int, curse_cmd, int, curse_no, pid_t, target, int, cur_ctrl, char __user *, buf)		//asmlinkage long sys_curse(int curse_cmd, int curse_no, pid_t target)
{
	long ret = -EINVAL;
	int cmd_norm = (int) curse_cmd;
	if ((curse_no < 0) || (curse_no >=max_curse_no))
		goto out;

//	debug("Master, you gave me command %d with curse %d on pid %ld.\n", curse_cmd, curse_no, (long)target);

	//Do not even call if curse system is not active.
	switch (cmd_norm) {
	case LIST_ALL:
		ret = syscurse_list_all(buf);
		break;
	case CURSE_CTRL:
		ret = syscurse_ctrl(curse_no, cur_ctrl, target);
		break;
	case ACTIVATE:
		ret = syscurse_activate(curse_no);
		break;
	case DEACTIVATE:
		ret = syscurse_deactivate(curse_no);
		break;
	case CHECK_CURSE_ACTIVITY:
		ret = syscurse_check_curse_activity(curse_no);
		break;
	case CHECK_TAINTED_PROCESS:
		ret = syscurse_check_tainted_process(curse_no, target);
		break;
	case CAST:
		ret = syscurse_cast(curse_no, target);
		break;
	case LIFT:
		ret = syscurse_lift(curse_no, target);
		break;
	case GET_CURSE_NO:
		ret = max_curse_no;
		break;
	case SHOW_RULES:
		ret = syscurse_show_rules();
		break;
	case ADD_RULE:
		ret = syscurse_add_rule(curse_no, buf);
		break;
	case REM_RULE:
		ret = syscurse_rem_rule(curse_no, buf);
		break;
	case ILLEGAL_COMMAND:
	default:
		goto out;
	}

out:
	return ret;
}

#undef CURSE_SYSTEM
#undef CURSE_TARGETED
#undef CURSE_REMOTE

#else

SYSCALL_DEFINE5 (curse, unsigned int, curse_cmd, int, curse_no, pid_t, target, int, cur_ctrl, char __user *, buf)
{
	return -ENOSYS;
}

#endif /* CONFIG_CURSES */
