#pragma once

class JsonValue;

// 向Core发起查询的句柄基类，定义在Dependence层供所有mod使用
class PostHandle {
public:

	/*
	* 析构查询句柄
	*/
	virtual ~PostHandle() = default;

	/*
	* 向Core发起查询请求；结果通过GetResult()获取引用，避免返回值跨模块释放导致崩溃
	* @request: 查询请求JSON
	*/
	virtual void Post(const JsonValue& request) = 0;

	/*
	* 获取上一次Post()查询的结果
	* @return: 查询结果JSON的引用
	*/
	virtual const JsonValue& GetResult() const = 0;

};
