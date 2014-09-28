Heterogeneous execution
=======================

GHOST is capable of running on different compute architectures.
Distinction between architectures is done on a per-process base.
Each GHOST process has set a type of #ghost_type_t which can be either ::GHOST_TYPE_WORK for processes which work themselves or ::GHOST_TYPE_CUDA for processes which drive a CUDA GPU as an accelerator.
The type can either be set by means of the environment variable GHOST_TYPE (like `GHOST_TYPE=CUDA ./a.out`) or via the function ghost_type_set().

Type identification
-------------------

If the type has not been set, GHOST applies some heuristics in order to identify a senseful type automatically.
For example, on an heterogeneous compute node with two CPU sockets and two CUDA GPUs, the first process will be of ::GHOST_TYPE_WORK, covering the host CPU.
If two more processes get launched on this node, they will get assigned the type ::GHOST_TYPE_CUDA and one of the GPUs each.
For management purposes, both of those processes will get an exclusive CPU core on the socket which is closest to the respective GPU.
At this point, all of the resources on the node are used.
A further process on this node will be of type ::GHOST_TYPE_WORK.
In this case, the CPU resources will get divided equally between the two CPU processes.
I.e., each of those processes runs on a single socket.
In many cases, this is a sensible usage model as it lowers the danger of NUMA problems.
So, a rule of thumb for the number of processes to start on a heterogeneous node is the number of GPUs plus the number of CPU sockets.


Data locality
-------------

Sparse or dense matrices may reside in the host and/or device memory.
Obviously, if a process is of ::GHOST_TYPE_WORK, the data resides on host memory only.
If a process is of ::GHOST_TYPE_CUDA, a #ghost_densemat_t resides in both host and device memory by default.
This allows easy exchange of densemat data between host and device.
Note that all numerical kernels on the densemat will be executed on the device in this case.
This behaviour can be changed by setting the according flags ::GHOST_DENSEMAT_HOST and ::GHOST_DENSEMAT_DEVICE at creation time. 
In contrast ti this, sparse matrices are only stored on the device for ::GHOST_TYPE_CUDA processes.
