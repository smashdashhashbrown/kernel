# Managing /proc files with `seq_file`

There is an API named `seq_file` that helps with formatting /proc files for output. It is based on sequence, which is composed of 3 functions:
    - `start()`
    - `next()`
    - `stop()`
The `seq_file` API starts a sequence when a user reads the /proc file.

A sequence begins with the call of the function start() . If the return is a non NULL value, the function next() is called; otherwise, the stop() function is called directly. This function is an iterator, the goal is to go through all the data. Each time next() is called, the function show() is also called. It writes data values in the buffer read by the user. The function next() is called until it returns NULL . The sequence ends when next() returns NULL , then the function stop() is called. 

The `seq_file` provides basic functions for `proc_ops`, such as `seq_read`, `seq_lseek`, and etc. But nothing for write operations.
