import CProcInfo
import Darwin

public struct ProcessProfile {
    var pid: pid_t
    var name: String
}

public func getLocalTcpPorts(of pid: pid_t) -> Set<Int32> {
    var count: Int32 = 0
    var ports: UnsafeMutablePointer<Int32>? = nil
    
    listLocalTcpPorts(pid, &ports, &count)
    defer { free(ports) }
    
    return Set(UnsafeBufferPointer<Int32>(start: ports, count: Int(count)).map{ $0 })
}

public func getAllProcess() -> [ProcessProfile] {
    var count: Int32 = 0
    var profiles: UnsafeMutablePointer<proc_profile>? = nil
    
    listProcs(&profiles, &count)
    
    defer {
        if let profiles = profiles {
            for i in 0 ..< Int(count) {
                let profile = profiles[i]
                free(profile.name)
            }
        }
        free(profiles)
    }
    
    return UnsafeBufferPointer<proc_profile>(start: profiles, count: Int(count)).map {
        return ProcessProfile(pid: $0.pid, name: String(cString: $0.name))
    }
}
