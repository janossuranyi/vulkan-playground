#pragma once

#include <vector>
#include <string>
#include <limits>
#include <unordered_map>

namespace common {

	template<class Type, class HandleType, typename IndexType = uint32_t>
	class ResourceContainer {
	public:
		ResourceContainer() = default;

		HandleType allocate(const Type& value, const std::string& key = "");

		Type free(HandleType handle);

		bool get(HandleType handle, Type& out) const;

		bool findKey(const std::string& key, HandleType& out);

		const Type& operator[](HandleType handle) const;

	private:
		static const IndexType invalidIndex = std::numeric_limits<IndexType>::max();
		std::vector<Type> m_vector;
		std::vector<HandleType> m_freelist;
		std::unordered_map<std::string, HandleType> m_name_to_handle;
	};

	template<class Type, class HandleType, typename IndexType>
	inline HandleType ResourceContainer<Type, HandleType, IndexType>::allocate(const Type& value, const std::string& key)
	{
		auto result = HandleType();

		if (!m_freelist.empty())
		{
			result = m_freelist.back();
			m_freelist.pop_back();
			m_vector[result.index] = value;
		}
		else 
		{
			result.index = static_cast<IndexType>( m_vector.size() );
			m_vector.push_back(value);
		}

		if (!key.empty()) {
			m_name_to_handle[key] = result;
		}
		
		return result;
	}

	template<class Type, class HandleType, typename IndexType>
	inline Type ResourceContainer<Type, HandleType, IndexType>::free(HandleType handle)
	{
		Type old = {};
		if (handle.index != invalidIndex && static_cast<IndexType>(m_vector.size()) > handle.index)
		{
			std::swap(m_vector[handle.index], old);
			m_freelist.push_back(handle);
		}

		for (auto& it : m_name_to_handle)
		{
			if (it.second.index == handle.index) {
				m_name_to_handle.erase(it.first);
				break;
			}
		}

		return old;
	}
	template<class Type, class HandleType, typename IndexType>
	inline bool ResourceContainer<Type, HandleType, IndexType>::get(HandleType handle, Type& out) const
	{
		if (handle.index != invalidIndex && static_cast<IndexType>(m_vector.size()) > handle.index)
		{
			out = m_vector[handle.index];
			return true;
		}
		
		return false;
	}
	template<class Type, class HandleType, typename IndexType>
	inline bool ResourceContainer<Type, HandleType, IndexType>::findKey(const std::string& key, HandleType& out)
	{
		auto it = m_name_to_handle.find(key);

		if (it != std::end(m_name_to_handle)) {
			out = it->second;
			return true;
		}

		return false;
	}

	template<class Type, class HandleType, typename IndexType>
	inline const Type& ResourceContainer<Type, HandleType, IndexType>::operator[](HandleType handle) const
	{
		return m_vector[handle.index];
	}
}