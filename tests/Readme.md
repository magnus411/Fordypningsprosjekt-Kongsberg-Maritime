# Tests

During roughly the first half of development, we wrote tests for the program. However, we realised that the tests we were writing were just the implementation of the program. Unit tests are not suited for SensorDHS, as the components interact with each other and don't do work in isolation. Given that the main purpose of the program is as a reference for how the client can develop a system that let's them easily develop and test different data handler implementations, the Modbus + PostgreSQL implementation works as a form of both end-to-end and integration test. 

These tests are therefore deprecated, and will not compile or run since they are written for the old module-based implementation, not the thread group implementation. We have decided to keep them here for documentation purposes.
