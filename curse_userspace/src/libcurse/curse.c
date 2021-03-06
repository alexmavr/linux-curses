#ifndef _LIB_CURSE_USER
#define _LIB_CURSE_USER

#include <curse/curse.h>
#include <curse/curse_list.h>
#include <curse/curse_types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>

#ifdef __i386__
#define __NR_curse 341
#else
#define __NR_curse 303
#endif

/*Static shared data.*/
sem_t list_sema;

/*Init-Fin handlers.*/
__attribute__((constructor)) void  curse_init_handle()
{
//	printf("Initializer on library called.\n");
	if (sem_init(&list_sema, 1 /*0 is for thread-shared semas*/ , 1) != 0) {
		perror("Semaphore initialization error");
	}
	//...Other initializations
}
__attribute__((destructor)) void  curse_fin_handle()
{
//	printf("Destructor on library called.\n");
	if (sem_destroy(&list_sema)) {
		perror("Semaphore destroy error");
	}
}

/*Wrapper for encapsulated access to the list. Static and NULL for the first time (protected by semaphore), if allocated and initialized, it must not need semaphore access.*/
struct curse_list_entry *get_list (long maxCurseNum) {
	static struct curse_list_entry *buffered_list = NULL;

	if (buffered_list == NULL) {
		if (!sem_wait(&list_sema)) {	/*Take sema.*/
			if (buffered_list == NULL) {
				/*Allocate (MAX_CURSE_NO+1)*sizeof(struct curse_list_entry)*/
				if ((buffered_list = (struct curse_list_entry *)calloc((maxCurseNum), sizeof(struct curse_list_entry))) == 0) {
					perror("Allocation failed");
					return NULL;
				}
				syscall(__NR_curse, LIST_ALL, 0, 0, 0, buffered_list);	/*Call syscall and get list.*/
			}
			if (sem_post(&list_sema)) {	 /*Release sema.*/
				perror("Semaphore release failed");
			}
		} else {
			perror("Semaphore acquisition failed");
			return NULL;
		}
	}
	return buffered_list;
}

inline long get_maxCurseNum()
{
	static long maxCurseNum = -1;
	if (maxCurseNum == -1)
		maxCurseNum = syscall(__NR_curse, GET_CURSE_NO, 0, 0, 0, 0);
	return maxCurseNum;
}

/*Wrapper for returning the index of a curse by searching with a name.*/
int index_from_name (const char *id)
{
	/*Search static buffered list (if not null) for occurence. That is until MAX_CURSE_NO.*/
	int i = 0;
	const struct curse_list_entry *list;

	//Get the max number only on first call of this function, save unneeded syscalls.
	long maxCurseNum = get_maxCurseNum();
	list = get_list(maxCurseNum);
	if (list != NULL) {
		for (i = 0; i < maxCurseNum; ++i) {
			if (strcmp(list[i].curse_name, id) == 0) {
				return i;
			}
		}
		perror("Curse not found");
		return -1;
	} else {
		perror("Curse list is empty");
		return -2;
	}
}


long curse(int command, const char* name, pid_t target, int ctrl, char* userbuf)
{
	int theCurse;
	struct curse_list_entry *list;
	long maxCurseNum;

	switch (command) {
	case LIST_ALL:
		maxCurseNum = get_maxCurseNum();
		list = get_list(maxCurseNum);
		userbuf = memcpy(userbuf, list, maxCurseNum*sizeof(struct curse_list_entry));
		return 1;
	case GET_CURSE_NO:
		return get_maxCurseNum();
	case SHOW_RULES:
	case ADD_RULE:
	case REM_RULE:
		theCurse = 0;
		break;
	default:
		theCurse = index_from_name(name);
		break;
	}

	if (theCurse < 0) {
		return -1;
	}

	return syscall(__NR_curse, command, theCurse, target, ctrl, userbuf);
}

#endif	/* _LIB_CURSE_NO */
