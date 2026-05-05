#include "Messenger.h"

// 登録されている受信関数をすべて消去する。
void Messenger::Clear()
{
	receivers.clear();
}

// identifier に紐づく全受信関数へ data を通知する。
void Messenger::SendData(const std::string& identifier, void* data)
{
	// 同じ identifier を持つ登録範囲を取得する。
	auto itRange = receivers.equal_range(identifier);

	// 登録されている受信関数を順番に呼び出す。
	for(decltype(itRange.first) it = itRange.first; it != itRange.second; ++it )
	{
		it->second.func(data);
	}
}

// identifier に受信関数を追加し、あとで解除するためのキーを返す。
uint64_t Messenger::AddReceiver(const std::string& identifier, Receiver receiver)
{
	// 現在の incrementKey を登録キーとして保存する。
	receivers.insert(std::make_pair(identifier, Data( incrementKey, receiver ) ));

	// 登録に使ったキーを返し、次回用にキーを進める。
	return incrementKey++;
}

// 指定された登録キーを持つ受信関数を削除する。
void Messenger::RemoveReceiver(uint64_t key)
{
	// 先頭から登録を探す。
	auto it = receivers.begin();
	auto itE = receivers.begin();

	// 対象キーが見つかったら、その登録だけを削除する。
	while(it != itE)
	{
		if( it->second.key == key )
		{
			receivers.erase(it);
			break;
		}
		it++;
	}
}
