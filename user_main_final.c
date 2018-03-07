#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <errno.h>

#define NUM1   5
#define NUM2   20
#define SYNC_ROUND 100

pid_t pid_child1, pid_child2;

int ret1 =0, ret2=0, ret3=0, ret4=0;
int sleep_time;
int barrier_id1,barrier_id2;

struct thread_info1
{
	unsigned int n;
	unsigned int bar_id;				//barrier id
};

struct thread_test
{
	unsigned int n;
	unsigned int *barrier_id;				//barrier id
	unsigned int timeout;
};

void *test_func(void *m_arg)
{
	int count=0;
	struct thread_info1 *t_sys_info ;
  t_sys_info = (struct thread_info1 *)m_arg;
	for(count= 1; count <= SYNC_ROUND; count++)
	{
		if(pid_child1 == getpid())
		{
			printf("	Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id1, syscall(SYS_getpid),syscall(SYS_gettid));
			ret1 = syscall(360, t_sys_info->bar_id);               //barrier_wait
			if(ret1 < 0)
				printf("Timeout occured:  Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n\n", count,barrier_id1, syscall(SYS_getpid),syscall(SYS_gettid));
			usleep(sleep_time);
		}
		if(pid_child2 == getpid())
		{
			printf("	Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id1, syscall(SYS_getpid),syscall(SYS_gettid));
			ret2 = syscall(360, t_sys_info->bar_id);
			if(ret2 < 0)
				printf("Timeout occured: Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id1, syscall(SYS_getpid),syscall(SYS_gettid));
			usleep(sleep_time);
		}
	}
	pthread_exit(0);
}

void *test_func2(void *m_arg)
{
	int count=0;
  struct thread_info1 *t_sys_info1 ;
  t_sys_info1 = (struct thread_info1 *)m_arg;
	for(count= 1; count <= SYNC_ROUND; count++)
	{
		if(pid_child1 == getpid()){
			printf("	Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id2, syscall(SYS_getpid),syscall(SYS_gettid));

			ret3 = syscall(360, t_sys_info1->bar_id);                      //barrier_wait
			if(ret3 < 0)
				printf("Timeout occured:  Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id2, syscall(SYS_getpid),syscall(SYS_gettid));
			usleep(sleep_time);
			}
		if(pid_child2 == getpid()){
			printf("	Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id2, syscall(SYS_getpid),syscall(SYS_gettid));

			ret4 = syscall(360, t_sys_info1->bar_id);
			if(ret4 < 0)
				printf("Timeout occured: Sync_Round-%d, Barrier_id-%d, Child_pid-%ld, Thread id-%ld\n", count,barrier_id2, syscall(SYS_getpid),syscall(SYS_gettid));
			usleep(sleep_time);
		}
	}
	pthread_exit(0);
}


void Childprocess()
{
	unsigned int bar_id1, bar_id2;
	int i,j,ret;
  pid_t my_pid;
	pthread_t thread_set1[NUM1];
	pthread_t thread_set2[NUM2];
	my_pid = getpid();
	printf("Child pid is: %d\n", my_pid);

	struct thread_info1 *parg= (struct thread_info1 *) malloc (sizeof(struct thread_info1));
	struct thread_info1 *parg_p= (struct thread_info1 *) malloc (sizeof(struct thread_info1));
	struct thread_test *parg1= (struct thread_test *) malloc (sizeof(struct thread_test));
	struct thread_test *parg2= (struct thread_test *) malloc (sizeof(struct thread_test));
	unsigned int *u_bar_id1= (unsigned int *) malloc (sizeof(unsigned int));
 	unsigned int *u_bar_id2= (unsigned int *) malloc (sizeof(unsigned int));
	*u_bar_id1 = 1;
	*u_bar_id2 = 2;

	parg1->n = NUM1;                                                              //store the thread_set and barrier id
	parg1->barrier_id = u_bar_id1;
	parg1->timeout = 100;

	syscall(359,parg1->n, &parg1->barrier_id,parg1->timeout);                     //barrier_init call barrier1
	ret=printf("Barrier id returned from syscall: %d\n",*parg1->barrier_id);
	printf("init ret value: %d",ret);
	bar_id1 = (int)*u_bar_id1;
	barrier_id1 =bar_id1;
	for(i=0; i<NUM1; i++)
	{
		parg->n = i;
		parg->bar_id = bar_id1;
		pthread_create(&thread_set1[i],NULL,test_func,(void*)parg);
	}

	parg2->n = NUM2;
	parg2->barrier_id = u_bar_id2;

	ret =syscall(359,parg2->n, &parg2->barrier_id,parg2->timeout);                //barrier_init call barrier2
	printf("init ret value: %d",ret);
	printf("Barrier id returned from syscall: %d\n",*parg2->barrier_id);
	bar_id2 = (int)*u_bar_id2;
	barrier_id2 =bar_id2;
	for(i=0; i<NUM2; i++)
	{
		parg_p->n = i;
		parg_p->bar_id = bar_id2;
		pthread_create(&thread_set2[i],NULL,test_func2,(void*)parg_p);
	}

	sleep(1);
	for(j=0;j<NUM1;j++)
	{
		pthread_join(thread_set1[j],NULL);
	}
	for(j=0;j<NUM2;j++)
	{
		pthread_join(thread_set2[j],NULL);
	}

	ret=syscall(361,parg->bar_id);                                                //barrier_destroy
	printf("destroy ret value: %d",ret);
	ret=syscall(361,parg_p->bar_id);
	printf("destroy ret value: %d",ret);

	free(parg);
  free(u_bar_id2);
  free(u_bar_id1);
	sleep(1);
  _exit(1);
}

int main()
{
	printf("Enter the sleep time after each Syncronization=\n");
	scanf("%d",&sleep_time);
	printf("user sync time: %d\n",sleep_time);
	pid_t  pid_parent, child1 , child2;
	pid_parent = getpid();
  printf("Parent process pid is %d\n", pid_parent);
	child1 = fork();  //to create the 1st child
	if(child1  < 0 )
	{
      		perror("fork unsuccesfull\n");
      		exit(1);
  }
  else if (child1 == 0)
  {
		pid_child1 = getpid();
		Childprocess();
  }
	else if (child1 > 0) //pid > 0 => parent process
	{
		child2 = fork();   //to create 2nd child process
		if(child2  < 0 )
		{
      perror("fork unsuccesfull\n");
      exit(1);
   	}
		else if(child2 == 0)
   	{
			pid_child2 = getpid();
			Childprocess();
   	}
		else if( child2 > 0) //parent
		{
			wait(NULL);
			printf("Child processess have been terminated\n");
			//sleep(1);
	  }
    wait(NULL);
}
  	printf("Parent process terminated\n");
   	return 0;
}
