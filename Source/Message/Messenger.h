#pragma once

#include <functional>
#include <string>
#include <map>

// 文字列の識別子でイベントを送り、登録済みの受信関数へ data を配る簡易メッセンジャー。
class Messenger
{
public:
	// 受信関数の実体型。
	// void* で任意のデータを受け取る。
	using Function = void(void*);

	// 登録する受信関数の型。
	using Receiver = std::function<Function>;

	// 無効な登録キーを表す値。
	static constexpr const uint64_t InvalidKey = 0xFFFFFFFFFFFFFFFF;

private:
	// シングルトンとしてのみ使うため、外部からの生成を禁止する。
	Messenger() {}

	// シングルトンとしてのみ使うため、外部からの破棄を禁止する。
	~Messenger() {}

public:
	// Messenger の唯一のインスタンスを取得する。
	static Messenger& Instance()
	{
		static Messenger instance;
		return instance;
	}

	// 登録済みの受信関数をすべて削除する。
	void Clear();

	// 指定した識別子に登録されている受信関数へ data を送る。
	void SendData(const std::string& identifier, void* data);

	// 指定した識別子に受信関数を登録し、解除用のキーを返す。
	uint64_t AddReceiver(const std::string& identifier, Receiver receiver);

	// AddReceiver で返されたキーを使って受信関数を解除する。
	void RemoveReceiver(uint64_t key);

private:
	// 1つの受信登録を保持する内部データ。
	struct Data
	{
		// 登録解除に使う一意キー。
		uint64_t	key;

		// 実際に呼び出す受信関数。
		Receiver	func;

		// 登録キーと受信関数をまとめて初期化する。
		Data( uint64_t key, Receiver func ) : key( key ), func( func ){}

		// 登録キーが同じかどうかを調べる。
		bool operator==( Data& r )
		{
			return key == r.key;
		}
	};

	// 識別子ごとに複数の受信関数を持てるようにする受信テーブル。
	std::multimap<std::string, Data> receivers;

	// AddReceiver のたびに増える登録キー。
	uint64_t incrementKey = 0;
};
