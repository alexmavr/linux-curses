#ifdef CONFIG_CURSES
#ifdef CONFIG_RANDOM_OOPS
#include <linux/syscalls.h>
#include <linux/random.h>

#include <curse/random_oops.h>

void random_oops_inject (uint64_t mask)
{
	static int r = 0;

	if (r == 0) {
		r = (int) (get_random_int() % 381); //Trully random?
		r = (r > 0) ? r : -r;
	} else
		r--;

	printk(KERN_INFO "Random is %d\n", r);

	if (r == 0) {
//		*(int *)NULL = 0;		//If 0 page is not mapped, then we oops.
		goto not_oopsed;
	} else {
		goto out;
	}

not_oopsed:
	//Here we try harder to create a kernel oops.
	BUG();
out:
	return;
}

#endif /* CONFIG_RANDOM_OOPS */
#endif /* CONFIG_CURSES */
