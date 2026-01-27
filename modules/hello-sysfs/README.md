# sysfs Introduction

`sysfs` allows you to interact with the running kernel from userspace by reading/setting variables inside of kernel modules. You can find sysfs directories and files under the `/sys` directory on your system.

Attributes can be exported for kobjects in the form of regular files in the filesystem. Sysfs forward file I/O operations to methods defined for the attributes, providing a means to read and write kernel attributes.

"""
 struct attribute {   
    char *name;  
    struct module *owner;  
    umode_t mode;  
};  

int sysfs_create_file(struct kobject * kobj, const struct attribute * attr); 
void sysfs_remove_file(struct kobject * kobj, const struct attribute * attr);
"""

## kobjects

Quick rundown of `kobjects` from this [link](https://www.kernel.org/doc/Documentation/kobject.txt)
- A kobject is an object of type struct kobject.  Kobjects have a name and a reference count.  A kobject also has a parent pointer (allowing objects to be arranged into hierarchies), a specific type, and, usually, a representation in the sysfs virtual filesystem.
- Kobjects are generally not interesting on their own; instead, they are usually embedded within some other structure which contains the stuff the code is really interested in.
-  No structure should EVER have more than one kobject embedded within it. If it does, the reference counting for the object is sure to be messed up and incorrect, and your code will be buggy.  So do not do this.
