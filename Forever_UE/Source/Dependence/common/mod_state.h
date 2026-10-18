#pragma once

#include <cstdint>


// 存档时mod写自己私有数据、读档时读回来用的接口——宿主(Core)实现、mod调用。所有方法都是
// virtual：mod DLL里调用时实际执行的是宿主编译的那份代码，存储容器的分配/释放始终在宿主一侧，
// 不跨DLL释放内存(CONVENTIONS.md §4)。键值式：在当前对象里按key写/读一个值，BeginObject/
// BeginArray进入嵌套层级，EndObject/EndArray退出。数组里的元素用空key写、按下标读。
// 详见mod_state.md。
class StateWriter {
public:
	virtual ~StateWriter() = default;

	virtual void SetInt(const char* key, int64_t value) = 0;
	virtual void SetDouble(const char* key, double value) = 0;
	virtual void SetBool(const char* key, bool value) = 0;
	virtual void SetString(const char* key, const char* value) = 0;
	// 任意二进制数据(宿主内部按base64之类的方式保存)，size是字节数。
	virtual void SetBytes(const char* key, const void* data, int64_t size) = 0;

	// 在当前层级下新建一个子对象/子数组并进入它；数组里的元素key传空串("")或nullptr。
	virtual void BeginObject(const char* key) = 0;
	virtual void EndObject() = 0;
	virtual void BeginArray(const char* key) = 0;
	virtual void EndArray() = 0;
};

class StateReader {
public:
	virtual ~StateReader() = default;

	// 这份数据保存时mod声明的GetStateVersion()——mod据此判断要不要按旧格式读、怎么迁移。
	virtual int GetSavedVersion() const = 0;

	virtual bool Has(const char* key) const = 0;
	virtual int64_t GetInt(const char* key, int64_t fallback = 0) const = 0;
	virtual double GetDouble(const char* key, double fallback = 0.0) const = 0;
	virtual bool GetBool(const char* key, bool fallback = false) const = 0;
	// 返回的指针指向宿主内存，只在下一次调用这个Reader之前有效，mod要自己拷贝。key不存在时返回fallback。
	virtual const char* GetString(const char* key, const char* fallback = "") const = 0;
	// 返回字节数，数据写入out(容量outCapacity字节，超出部分截断)；outCapacity传0可以只查大小。
	virtual int64_t GetBytes(const char* key, void* out, int64_t outCapacity) const = 0;

	// 进入子对象/子数组，key不存在或类型不对返回false(不进入)。数组用GetArraySize取长度，
	// 用EnterArrayElement(index)进入第index个元素(元素本身是对象时)，或者用下面按下标读的方法读标量。
	virtual bool EnterObject(const char* key) = 0;
	virtual void ExitObject() = 0;
	virtual bool EnterArray(const char* key) = 0;
	virtual void ExitArray() = 0;
	virtual int GetArraySize() const = 0;
	virtual bool EnterArrayElement(int index) = 0;
	virtual int64_t GetIntAt(int index, int64_t fallback = 0) const = 0;
	virtual double GetDoubleAt(int index, double fallback = 0.0) const = 0;
	virtual const char* GetStringAt(int index, const char* fallback = "") const = 0;
};
