import CProcInfo
import Darwin

public func getLocalTcpPorts(of pid: pid_t) -> Set<Int32> {
    var count: Int32 = 0
    var ports: UnsafeMutablePointer<Int32>? = nil
    
    listLocalTcpPorts(pid, &ports, &count)
    defer { free(ports) }
    
    return Set(UnsafeBufferPointer<Int32>(start: ports, count: Int(count)).map{ $0 })
}
