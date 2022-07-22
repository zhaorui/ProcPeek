import XCTest
@testable import ProcPeek

final class ProcPeekTests: XCTestCase {
    
    func testWithRootProcess() {
        let result = getLocalTcpPorts(of: 0)
        XCTAssertTrue(result.isEmpty)
    }
    
    func testWithInExistProcess() {
        let result = getLocalTcpPorts(of: 2)
        XCTAssertTrue(result.isEmpty)
    }
    
    func testWithProcessWithoutConnection() {
        let result = getLocalTcpPorts(of: 518)
        XCTAssertTrue(result.isEmpty)
    }
    
    func testPerformance() {
        measure {
            let result = getLocalTcpPorts(of: 56605)
            print(result)
        }
    }
}
