#include <linux/kernel.h>  
#include <linux/sched.h>
#include <linux/list.h>


asmlinkage long sys_hello(void) 
{
    printk("Hello, World!\n");
    return 0;
} 

asmlinkage long sys_set_weight(int weight)
{
    if(weight < 0)
        return -EINVAL;
    current->weight = weight;
    return 0;  
}

asmlinkage long sys_get_weight(void)
{
    return current->weight;
}

asmlinkage long sys_get_siblings_sum(void)
{
    long sum = 0;
    struct list_head* node;
    struct task_struct* current_task;
    long count = 0;
    list_for_each(node, &current->sibling)
    {
        current_task = list_entry(node, struct task_struct, sibling);
	    if(current_task->pid != 0)
	    {
        	count++;
        	sum += current_task->weight;
	    }
    }
    if(count == 0)
    {
        return -ESRCH;
    }
    return sum;
}

asmlinkage long sys_get_lightest_divisor_ancestor(void)
{
    long initial_weight = current->weight;
    long current_divisor = initial_weight;
    pid_t my_pid = current->pid;
    struct task_struct* current_task;
    current_task = current;
    while (current_task->pid != 1)
    {
        current_task = current_task->real_parent;
        if
        (
            (current_task->weight != 0) &&
            ((initial_weight % current_task->weight) == 0) &&
            current_task->weight < current_divisor
        )
        {
            current_divisor = current_task->weight;
            my_pid = current_task->pid;
        }
    }
    return my_pid;
}