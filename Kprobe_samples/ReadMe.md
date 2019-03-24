
Linux has static and dynamic tracing facilities with which callback functions can be invoked when
trace points (or probes) are hit.

This is a sample program to understand Kprobe feature of debugging. RBProbe is the name of the device that
is used to attach Kprobe to the device driver which uses a Red-black binary tree to store and retrieve data

Other than open and close file operations, the read and write operations of RBprobe device can be defined
as:
1. write: to register or unregister a kprobe. The location (offset) of the kprobe is passed in the buffer
*buf along with an integer flag. A kprobe is registered if the flag is 1, or unregistered if 0.
2. read: to retrieve the trace data items collected in a probe hit and saved in the buffer. If the buffer
is empty, -1 is returned and errno is set to EINVAL.
