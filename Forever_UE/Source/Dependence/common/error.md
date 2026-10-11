# error.h / error.cpp

异常基类`ExceptionBase`及派生类（`IOException`/`JsonFormatException`等）、
`THROW_EXCEPTION`宏，不逐函数写文档。`json.cpp`的类型转换方法（`AsString`/`AsInt`等）和
`Core/common/config.cpp`都会用到这里的异常类型。
