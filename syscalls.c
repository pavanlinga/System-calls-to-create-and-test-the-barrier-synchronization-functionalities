#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>


static LIST_HEAD(list_start);

int b_count =1,child1,i;
bool condition_check=0,flag1=0;
int error_flag=0,destroy_flag =0;
spinlock_t b_init_lock;

struct my_node
{
	struct list_head list_h;                  //list-head contains pointers of next and prev nodes
	int child_pid;                            //to store the child pid
	int thread_set;                           //to store the number of threads the node should deal with
	int barrier_num;                          //to store the barrier id
	int thread_count;                         //count the number of incoming threads into the node
	int sync_round;                           //stores the Syncronization round
	long long task_ptr_array[20];             //array to store the task_struct pointer of the thread that goes into sleep
	ktime_t ktime;                            //hrtimer
	struct hrtimer timer;                     //each node contain its own hrtimer
};

static enum hrtimer_restart timer_callback(struct hrtimer *timer_ptr)
{
	spin_lock(&b_init_lock);
	struct my_node *b_node;
	b_node = container_of(timer_ptr, struct my_node, timer);
	flag1 = 1;                                                   //flag=1 says that the timeout occured
	spin_unlock(&b_init_lock);
  return HRTIMER_NORESTART;                                    //dont restart the timer
}

asmlinkage long sys_barrier_destroy(unsigned int barrier_id)
{
  struct my_node *b_node1, *temp;
  b_count = 1;
  list_for_each_entry_safe(b_node1, temp, &list_start, list_h)
  {
     printk(KERN_INFO "freeing barrier id: %d, child pid: %d\n", b_node1->barrier_num,b_node1->child_pid);
     list_del(&b_node1->list_h);                                 //delete the node with matching barrier_id
     kfree(b_node1);
		 destroy_flag =1;
  }
	if(destroy_flag == 0)
	{
		return -EINVAL;
	}
	return 0;
}

asmlinkage long sys_barrier_wait(unsigned barrier_id){
  struct my_node *b_node;                                     //initialise the my_node ----per node structure
	struct task_struct *task1;                                  //initialise the task_struct
	condition_check = 0;
	spin_lock(&b_init_lock);
	task1 = current;                                            //gives the pointer to the current task's task_struct
	list_for_each_entry(b_node, &list_start, list_h){                                //find the node with the given conditions
		if((b_node->child_pid == task1->tgid) && (b_node->barrier_num == barrier_id)){    //get the pointer to that node
			condition_check = 1;
			break;
		}
	}
	if(condition_check == 0)
	{
		spin_unlock(&b_init_lock);
		return -EINVAL;
	}
	if(b_node->thread_count < b_node->thread_set)                              //check whether the thread count is less than the
	{                                                                          //num of threads set
		b_node->thread_count++;
		b_node->task_ptr_array[b_node->thread_count] = (int)current;             //store the current task_struct pointer in an array
		if(b_node->thread_count == 1){                                           //which is required to wake up the thread by the scheduler
			b_node->sync_round++;
			hrtimer_start(&b_node->timer, b_node->ktime, HRTIMER_MODE_REL);       //start the timer only if 1st thread of thread set enters
		}
		if(b_node->thread_count == b_node->thread_set)
		{
			for(i=1;i<=b_node->thread_set;i++){
				wake_up_process(b_node->task_ptr_array[i]);                         //wake up all the sleepig threads
				b_node->task_ptr_array[i] = 0;                                      //reset the array with zeros
			}
			hrtimer_cancel(&b_node->timer);                                       //cancel the hrtimer
			b_node->thread_count =0;
			error_flag =0;
			spin_unlock(&b_init_lock);
		}
		else{
			set_current_state(TASK_INTERRUPTIBLE);                               //set the current task as interuptible
			spin_unlock(&b_init_lock);
			schedule();                                                          //put the current task to sleep
			spin_unlock(&b_init_lock);
		}
	}
	return 0;
}

asmlinkage long sys_barrier_init(unsigned int count, unsigned int *barrier_id,signed timeout)
{
	struct task_struct *task1 = current;
	struct my_node *b_node;
	if(b_count ==1){
		spin_lock_init(&b_init_lock);
		INIT_LIST_HEAD(&list_start);                                            //initialise the list head which sholud run only for the 1st time
		b_count =2;
	}
	spin_lock(&b_init_lock);
	b_node = kmalloc(sizeof(struct my_node), GFP_KERNEL);                     //allocate memory for each node in the linked list
	list_add(&b_node->list_h, &list_start);                                   //add the node to the list
	b_node->child_pid = task1->tgid;                                          //store the tgid of the child process
	b_node->thread_set = count;                                               //store the thread count
	b_node->barrier_num = (int)*barrier_id;                                         //store the barrier id
	b_node->thread_count = 0;
	b_node->sync_round =0;
	b_node->ktime = ktime_set(0,timeout);
	hrtimer_init(&b_node->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);          //initialise the hrtimer
	b_node->timer.function = &timer_callback;
	copy_to_user(barrier_id,&b_node->barrier_num,sizeof(barrier_id));
  printk(KERN_ALERT "Barrier init successful\n");
	spin_unlock(&b_init_lock);
	return 0;
}
