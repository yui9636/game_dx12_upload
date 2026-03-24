#pragma once

#include <functional>
#include <string>
#include <map>

class Messenger
{
public:
	using Function = void(void*);
	using Receiver = std::function<Function>;

	static constexpr const uint64_t InvalidKey = 0xFFFFFFFFFFFFFFFF;

private:
	Messenger() {}
	~Messenger() {}

public:
	static Messenger& Instance()
	{
		static Messenger instance;
		return instance;
	}

	void Clear();

	void SendData(const std::string& identifier, void* data);

	uint64_t AddReceiver(const std::string& identifier, Receiver receiver);

	void RemoveReceiver(uint64_t key);

private:
	struct Data
	{
		uint64_t	key;
		Receiver	func;
		Data( uint64_t key, Receiver func ) : key( key ), func( func ){}
		bool operator==( Data& r )
		{
			return key == r.key;
		}
	};
	std::multimap<std::string, Data> receivers;
	uint64_t incrementKey = 0;
};
