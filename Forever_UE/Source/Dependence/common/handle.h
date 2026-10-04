#pragma once


class JsonValue;

// 向Core发起查询的句柄基类，定义在Dependence层供所有mod使用
class PostHandle {
public:
	virtual ~PostHandle() = default;

	virtual void Post(const JsonValue& request) = 0;

	// 返回引用而不是按值返回查询结果，避免跨模块传值触发"由哪个模块的分配器释放"的问题，
	// 见handle.md。
	virtual const JsonValue& GetResult() const = 0;
};
