===================================================================================================================
(B)
KSHELL commands to execute the test cases for kernel1:

	1)Type "faber" from the kshell command to execute testproc()(and there by all the functions in it).

	2)Type "sunghan" from the kshell command to execute sunghan_test().

	3)Type "deadlock" from the kshell command to execute sunghan_deadlock_test().

(E)

Every code path has been covered by test cases in faber_test and sunghan_tests. The detailed analysis is given below.

====================================================================================================================
kmain.c
1. bootstrap()
	creates the idle process which creates init process that is tested in points (2) and (3) below
2. initproc_create()	
	create init process which creates processes in faber_test#testproc()
3. initproc_run()
	creates kshell which executes faber_test#testproc()
proc.c

1. proc_create()
	covered by faber_test#testproc()#startproc()
2. proc_cleanup()
	covered by faber_test#kthread_exit_test in CS402TESTS>0
	faber_test#startproc#kthread_exit_test calls kthread_exit() which calls proc_thread_exited() which, in turn, calls proc_cleanup()
3. proc_kill()
	called by proc_kill_all() which is tested in faber_test#testproc() in CS402TESTS > 7
4. proc_kill_all()
	covered by faber_test#testproc() in CS402TESTS > 7
5. proc_thread_exited()
	called by kthread_exit which is tested in faber_test#kthread_exit_test 	
6. do_waitpid()
	covered in faber_test#wait_for_proc	
	covered in faber_test#wait_for_proc	
	covered in faber_test#wait_for_proc	
	
7. do_exit()
	covered in faber_test#cancelme_test

kthread.c
1. kthread_create()
	covered by faber_test#testproc()#startproc()
2. kthread_cancel()
	called by proc_kill which is called by proc_kill_all tested in faber_test#testproc() in CS402TESTS > 7
3. kthread_exit()
	covered in faber_test#kthread_exit_test 	

kmutex.c
0. kmutex_init()
	covered in faber_test#racer_test
1. kmutex_lock()
	covered in sunghan_test#add_my_node
2. kmutex_lock_cancellable()
	covered in faber_test#mutex_test_cancelme
3. kmutex_unlock()
	covered in sunghan_test#remove_my_node

sched.c

1. sched_sleep_on()
	covered in sunghan_test#watch_dog
2. sched_cancellable_sleep_on()
	covered in faber_test#wakeme_test in CS402TEST>2
3. sched_wakeup_on()
	covered in faber_test#testproc in CS402TEST>2
4. sched_broadcast_on()
	covered in sunghan_test#watch_dog
5. sched_cancel()
	covered in faber_test#testproc in CS402TEST>4
6. sched_switch()
	covered in faber_test#racer_test
7. sched_make_runnable()
	covered in faber_test#racer_test
