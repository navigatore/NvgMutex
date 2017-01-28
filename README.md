# NvgMutex

### Overview
This is a WinAPI project, it consists of two major parts:

1. Server – mutex service provider.
2. Client library – allows a programmer to use the mutex service using simple and convenient API.

Windows Mailslots mechanism is used to communicate between the server and the library. Windows Mutex Objects are used internally in the server.

Of course, it makes little sense to use NvgMutex at all, because it is inefficient and there are many better ways to perform synchronization.

However, it nicely shows usage of some WinAPI features (Mutex Objects, Mailslots, detecting thread shutdown etc.).

### Testing
A simple testing program is included. The server must be running when tests are performed. You can run multiple instances of the program, for example to see how mutex synchronization works on stdout.
