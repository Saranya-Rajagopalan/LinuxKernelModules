RBTree device driver

Linux kernel consists of several generic data structures. One of them is RB tree (red-black tree) which
is a form of semi-balanced binary tree. To form a binary tree, each node in the tree contains up to two
children. A node in a RB tree should consist of a value that is greater than that of all children in the "left"
child branch, and less than that of all children in the "right" branch. Thus, it is possible to organize a redblack
tree by performing a depth-first, left-to-right traversal. The implementation is provided in
include/linux/rbtree.h and lib/rbtree.c.

This device driver saves the data written by the user as a key-value pair in a red-black tree and removes the data
with minimum/maximum key value(configurable using ioctl) from the tree when the user "reads"  from the tree.

Functions used in the device driver:
1. open: to open a device (the device is “rbtdevice”).
2. write: if the input object of rb_object_t has a non-zero data field, a node is created and added to
the tree. If an object with the same key already exists in the tree, it is replaced with the
new one. If the data field is 0, any existing object with the input key is deleted from the table.
3. read: to retrieve the first object (the object with the minimal “key”) or last object (the object with
the maximal “key”) from the RB tree. If the RB tree is empty, -1 is returned and errno is set to
EINVAL. After reading, the object is removed from the tree.
4. Ioctl: The command “set_end” to set which object is to be read. If the argument is 0, read calls
retrieve the first object in the tree. It if is 1, read calls get the last object. Otherwise, -1 is returned
and errno is set to EINVAL.
5. release: to close the descriptor of an opened device file.


Instructions to run:

1. Download and save everything in the same directory.
2. Open terminal in the same directory and enter "make" in superuser mode.
3. You will see that the object files are created in the directory.
4. Insert the driver module using "sudo insmod rbt_drv.ko".
5. Look for rbtdeice under /dev directory to verify if the device is created.
6. Run the test program ./a.out
