# json.h / json.cpp

原样移植自旧工程`E:\Projects\Forever_UE\Source\Dependence\common\json.h/.cpp`(JsonCpp衍生
的宽松JSON解析器,支持`//`/`/* */`注释,`JsonValue`/`JsonReader`/`FastWriter`/`StyledWriter`
等),未做任何修改,不逐函数写文档。选它而不是UE自带`Json`模块的原因见
`Source/Core/common/config.md`——UE的`FJsonSerializer`是严格JSON,不支持注释,而
`config.json`需要保留注释能力。
