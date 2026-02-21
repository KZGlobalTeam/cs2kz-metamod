#pragma once
#include "common.h"

#include <vendor/json/single_include/nlohmann/json.hpp>
#include <vendor/json/single_include/nlohmann/json_fwd.hpp>

class Json
{
public:
	Json() : inner(nlohmann::json::object()) {}

	Json(const char *text) : inner(nlohmann::json::parse(text, nullptr, false)) {}

	Json(const std::string &text) : inner(nlohmann::json::parse(text, nullptr, false)) {}

	template<typename T>
	Json(const T &value)
	{
		this->inner = nlohmann::json::object();
		value.ToJson(*this);
	}

	std::string ToString() const
	{
		return this->inner.dump();
	}

	bool ContainsKey(const std::string &key, bool allowNull = false) const
	{
		if (!this->inner.is_object())
		{
			META_CONPRINTF("[JSON] Cannot extract value from non-object.\n");
			return false;
		}

		if (!this->inner.contains(key))
		{
			META_CONPRINTF("[JSON] Key `%s` does not exist.\n", key.c_str());
			return false;
		}

		if (!allowNull && this->inner[key].is_null())
		{
			META_CONPRINTF("[JSON] Key `%s` is null.\n", key.c_str());
			return false;
		}

		return true;
	}

	template<typename T>
	bool Decode(std::optional<T> &out) const
	{
		if (this->inner.is_null())
		{
			out = std::nullopt;
			return true;
		}

		T value;

		if (!value.FromJson(*this))
		{
			return false;
		}

		out = std::make_optional(std::move(value));
		return true;
	}

	template<typename T>
	bool Decode(T &out) const
	{
		if (this->inner.is_null())
		{
			return false;
		}

		return out.FromJson(*this);
	}

	template<typename T>
	bool Set(const std::string &key, const T &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		Json json;

		if (!value.ToJson(json))
		{
			return false;
		}

		this->inner[key] = json.inner;

		return true;
	}

	bool Set(const std::string &key, const Json &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value.inner;
		return true;
	}

	bool Set(const std::string &key, const bool &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const u8 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const u16 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const u32 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const u64 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const f32 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const f64 &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, const char *value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = std::string(value);
		return true;
	}

	bool Set(const std::string &key, const std::string &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	bool Set(const std::string &key, std::string_view value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = value;
		return true;
	}

	template<typename T>
	bool Set(const std::string &key, const std::vector<T> &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = nlohmann::json::array();

		for (const auto &item : value)
		{
			Json json;

			if (!json.Set("value", item))
			{
				return false;
			}

			this->inner[key].push_back(json.inner["value"]);
		}

		return true;
	}

	template<typename K, typename V>
	bool Set(const std::string &key, const std::unordered_map<K, V> &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		this->inner[key] = nlohmann::json::object();

		for (const auto &[k, v] : value)
		{
			Json json;

			if (!v.ToJson(json))
			{
				return false;
			}

			this->inner[key][std::to_string(k)] = json.inner;
		}

		return true;
	}

	template<typename T>
	bool Set(const std::string &key, const std::optional<T> &value)
	{
		if (!this->inner.is_object())
		{
			return false;
		}

		if (value)
		{
			return this->Set(key, value.value());
		}
		else
		{
			this->inner[key] = nlohmann::json(nullptr);
			return true;
		}
	}

	bool IsValid() const
	{
		return !this->inner.is_discarded();
	}

	bool Get(const std::string &key, Json &out) const
	{
		if (!this->ContainsKey(key, /* allowNull */ true))
		{
			return false;
		}

		out.inner = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, bool &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_boolean())
		{
			META_CONPRINTF("[JSON] Key `%s` is not a boolean.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, u16 &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_number_unsigned())
		{
			META_CONPRINTF("[JSON] Key `%s` is not an unsigned integer.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, u32 &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_number_unsigned())
		{
			META_CONPRINTF("[JSON] Key `%s` is not an unsigned integer.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, u64 &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_number_unsigned())
		{
			META_CONPRINTF("[JSON] Key `%s` is not an unsigned integer.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, f32 &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_number())
		{
			META_CONPRINTF("[JSON] Key `%s` is not a float.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, f64 &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_number())
		{
			META_CONPRINTF("[JSON] Key `%s` is not a float.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	bool Get(const std::string &key, std::string &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_string())
		{
			META_CONPRINTF("[JSON] Key `%s` is not a string.\n", key.c_str());
			return false;
		}

		out = this->inner[key];
		return true;
	}

	template<typename T>
	bool Get(const std::string &key, std::optional<T> &out) const
	{
		if (!this->ContainsKey(key) || this->inner[key].is_null())
		{
			out = std::nullopt;
			return true;
		}

		T value;

		if (!this->Get(key, value))
		{
			return false;
		}

		out = std::make_optional(std::move(value));
		return true;
	}

	template<typename T>
	bool Get(const std::string &key, std::vector<T> &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		if (!this->inner[key].is_array())
		{
			META_CONPRINTF("[JSON] Key `%s` is not an array.\n", key.c_str());
			return false;
		}

		for (const auto &item : this->inner[key])
		{
			T value;

			if (!value.FromJson(item))
			{
				return false;
			}

			out.push_back(value);
		}

		return true;
	}

	template<typename T>
	bool Get(const std::string &key, T &out) const
	{
		if (!this->ContainsKey(key))
		{
			return false;
		}

		Json json = Json(this->inner[key]);

		return out.FromJson(json);
	}

private:
	nlohmann::json inner;

	Json(nlohmann::json json) : inner(json) {}
};
