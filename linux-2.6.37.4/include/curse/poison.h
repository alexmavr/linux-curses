#ifdef CONFIG_CURSES
#ifdef CONFIG_POISON
#ifndef _POISON_CURSE
#define _POISON_CURSE

#define POISON_DURATION 20
void poison_inject (uint64_t);
void poison_init (struct task_struct *);
void poison_destroy (struct task_struct *);

#endif	/* CONFIG_POISON */
#endif	/* _POISON_CURSE */
#endif	/* CONFIG_CURSES */
