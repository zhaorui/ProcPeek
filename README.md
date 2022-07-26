# ProcPeek

This library can help you to get a list of running processes, get a list of tcp ports opened by the process,
and find a process by the tcp port it opened.    

## Get a list of opened TCP ports of the process.    
Just pass the pid of process to the function `getLocalTcpPorts(of: pid_t)`.   

```swift
let result = getLocalTcpPorts(of: 56605)
print(result)
```

It's quite easy to get all the TCP ports opened by one process with a command.   
```shell
lsof -nP -p <pid> -a -i4TCP
```
However, if you need to do the job programmatically with Swift in macOS. There's no available documented API to do this.    
So I use below functions from `libproc` just like the command `lsof` do.

* proc_pidfdinfo
* proc_pidinfo

## Get a list of BSD processes
Just use `getAllProcess()`. 
```swift
for process in getAllProcess() {
    print(process.pid, process.name)
}
```

## Find process by the TCP port it opened
```swift
let process = getProcess(from: 13659)
```



