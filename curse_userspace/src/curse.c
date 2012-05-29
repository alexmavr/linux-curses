/* -.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
* File Name : curse.c
* Creation Date : 28-05-2012
* Last Modified : %+
* Created By : Greg Liras <gregliras@gmail.com>
_._._._._._._._._._._._._._._._._._._._._.*/

#ifndef _LIB_CURSE_USER
#define _LIB_CURSE_USER

#include <curse/curse.h>
#include <curse/curse_list.h>
#include <curse/curse_types.h>

#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>

#ifdef __i386__
#define __NR_curse 341
#else
#define __NR_curse 303
#endif

/*Static shared data.*/
static sem_t list_sema;

/*Init-Fin handlers.*/
static void curse_init_handle {
	if (!sem_init(&list_sema, 1/*0 is for thread-shared semas*/, 1)) {
		//...Error.
	}
	//...Other initializings
	
}
static void curse_fin_handle {
	if (!sem_destroy(&list_sema)) {
		//...Error.
	}
	//...Other
}

/*Wrapper for encapsulated access to the list. Static and NULL for the first time (protected by a semaphore), if allocated and initialized, it must not need semaphore access.*/
struct curse_list_entry *get_list (void) {
	static struct curse_list_entry *buffered_list = NULL;
	static size_t curse_no;

	if (buffered_list!=NULL) {
		if (!sem_wait(&list_sema)) {	/*Take sema.*/
			if (buffered_list==NULL) {
				/*Call to get max_curse_no*/
				/*Allocate (MAX_CURSE_NO+1)*sizeof(struct curse_list_entry)*/
				//buffered_list = (struct curse_list_entry *)calloc((MAX_CURSE_NO+1), sizeof(struct curse_list_entry));
				/*Call syscall and get list.*/
			} else if (!sem_post(&list_sema)) {	 /*Release sema.*/
				//...Error out.
			}
		} else {
			return NULL;
		}
		return buffered_list;
	}
}

/*Wrapper for returning the index of a curse by searching with a name.*/
int index_from_name (const char *id) {
	/*Search static buffered list (if not null) for occurence. That is until MAX_CURSE_NO.*/

	return id[0];
}

long curse (int command, const char *id, pid_t target) {
	curse = index_from_name (id);
	return syscall(__NR_curse, command, curse, target);
}

#endif	/* _LIB_CURSE_NO */
