// RUN: %target-run-simple-swift
// REQUIRES: executable_test

import StdlibUnittest
#if os(macOS)
import Darwin.C
#else
import Glibc
#endif

var SimpleMathTests = TestSuite("SimpleMath")

SimpleMathTests.test("Arithmetics") {
  let dfoo1 = #gradient({ (x: Float, y: Float) -> Float in
    return x * y
  })
  expectEqual((4, 3), dfoo1(3, 4))
  let dfoo2 = #gradient({ (x: Float, y: Float) -> Float in
    return -x * y
  })
  expectEqual((-4, -3), dfoo2(3, 4))
  let dfoo3 = #gradient({ (x: Float, y: Float) -> Float in
    return -x + y
  })
  expectEqual((-1, 1), dfoo3(3, 4))
}

SimpleMathTests.test("Fanout") {
  let dfoo1 = #gradient({ (x: Float) -> Float in
     x - x
  })
  expectEqual(0, dfoo1(100))
  let dfoo2 = #gradient({ (x: Float) -> Float in
     x + x
  })
  expectEqual(2, dfoo2(100))
  let dfoo3 = #gradient({ (x: Float, y: Float) -> Float in
    x + x + x * y
  })
  expectEqual((4, 3), dfoo3(3, 2))
}

SimpleMathTests.test("FunctionCall") {
  func foo(_ x: Float, _ y: Float) -> Float {
    return 3 * x + { $0 * 3 }(3) * y
  }
  expectEqual((3, 9), #gradient(foo)(3, 4))
  expectEqual(3, #gradient(foo, wrt: .0)(3, 4))
}

runAllTests()
