==================================================================================================================================================

Submitted By: kothamas@usc.edu, marella@usc.edu
Date      : 2013-12-06
==================================================================================================================================================
Section C
It has been verified that the code written by us has been covered. The details are given below.

==================================================================================================================================================
Test cases provided as part of kernel 3 assignment covered our code path.

kernel/api/access.c:
addr_perm(): called up many a times when we wanted to check the permissions on an address and also called by range_perm.
range_perm(): This function is called when we wanted to check the permissions on a given address range.
   We observed that this code has been bit during memtest tests for example.
kernel/api/syscallc.:
sys_read(): This function is invoked when we want to read contents from the user space to kernel space and again copy to the user.
sys_write(): Almost the same functionality as above, and this code is hit as many time we try to copy something from user space to kernel space.
kernel/proc/kthread.c:
kthread_clone():newly written method to clone the thread. Hit when we call fork. E.g hello program.
fork.c(): Entire code path in fork.c is hit when we try to run the hello program and remaining programs which call fork.
kernel/vm/anon.c: Functions in this file are hit during the entire process of creating,cleaning pages and destroying ananoymus objects which are not assosicated with any files.
kernel/vm/brk.c: This code path is hit when we try to increase our heap area. This code path is tested by many test cases which are part of this project.
kernel/vm/shadow.c: Similar implementation as anon objects but beaviour is different. Code in this file is hit during the process of forking a current process and destroying these objects in later stages.
kernel/vm/vmmap.c,kernel/vm/mmap.c: These two files are used in mapping, inserting, removing the memory objects and unmapping the memory areas.
kernel/vm/pagefault.c: This is used when getting the page which is not present in the pagetable by using pt_map.
kernel/mm/pframe.c: Implemented pframe_get, pframe_pin and pframe_unpin which inturn calls the corresponding object functions.

================
