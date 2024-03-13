#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <optional>
#include <set>
#include <map>

namespace std_alias {
	template<typename T>
	using Uptr = std::unique_ptr<T>;

	template<typename T, typename... Args>
	Uptr<T> mkuptr(Args &&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T>
	using Vec = std::vector<T>;

	template<typename T>
    using Set = std::set<T, std::less<void>>;

	template<typename K, typename V>
	using Map = std::map<K, V, std::less<void>>;

	template<typename T>
	decltype(auto) mv(T &&arg) { return std::move(arg); }

	template<typename T>
	using Opt = std::optional<T>;

	template<typename T1, typename T2>
	using Pair = std::pair<T1, T2>;

	template<typename D, typename S>
	Vec<D> &operator+=(Vec<D> &dest, const Vec<S> &source) {
		dest.insert(dest.end(), source.begin(), source.end());
		return dest;
	}

	template<typename D, typename S>
	Set<D> &operator+=(Set<D> &dest, const Set<S> &source) {
		dest.insert(source.begin(), source.end());
		return dest;
	}

	template<typename D, typename S>
	Set<D> &operator-=(Set<D> &dest, const Set<S> &source) {
		for (const S &s : source) {
			if (auto it = dest.find(s); it != dest.end()) {
				dest.erase(it);
			}
		}
		return dest;
	}
}
