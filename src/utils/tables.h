#pragma once
#include "common.h"
#include <vector>

namespace utils
{
	// A table with column sizes that adapts to the content.
	// The table column count must be defined in compile time!
	template<size_t N>
	class Table
	{
		static_assert(N > 0, "There must be at least 1 column"); // Compile-time check

	public:
		static constexpr const size_t columnCount = N;

		Table(const std::string &title, std::string headers[columnCount]) : title(title)
		{
			for (u32 i = 0; i < columnCount; i++)
			{
				this->headers[i] = headers[i];
				this->headers[i] += "ᅟ"; // Add a space to reset the "font" to monospace
				columnLengths[i] = GetDisplayByteLength(this->headers[i].c_str());
			}
		}

		Table(const char *title, const char *headers[columnCount]) : title(title)
		{
			for (u32 i = 0; i < columnCount; i++)
			{
				this->headers[i] = headers[i];
				this->headers[i] += "ᅟ"; // Add a space to reset the "font" to monospace
				columnLengths[i] = GetDisplayByteLength(this->headers[i].c_str());
			}
		}

	private:
		static constexpr const char *fontResetSuffix = "ᅟ";

		static u32 GetUtf8CodepointCount(const char *value, u32 byteCount)
		{
			u32 codepoints = 0;
			for (u32 i = 0; i < byteCount; i++)
			{
				unsigned char c = static_cast<unsigned char>(value[i]);
				if ((c & 0xC0) != 0x80)
				{
					codepoints++;
				}
			}
			return codepoints;
		}

		static bool EndsWithFontResetSuffix(const char *value, u32 length)
		{
			const u32 suffixLength = static_cast<u32>(V_strlen(fontResetSuffix));
			if (length < suffixLength)
			{
				return false;
			}
			return V_strncmp(value + (length - suffixLength), fontResetSuffix, suffixLength) == 0;
		}

		static u32 GetDisplayByteLength(const char *value)
		{
			u32 length = static_cast<u32>(V_strlen(value));
			if (EndsWithFontResetSuffix(value, length))
			{
				length -= static_cast<u32>(V_strlen(fontResetSuffix));
			}
			return length;
		}

		static u32 GetDisplayPadding(const char *value)
		{
			u32 displayBytes = GetDisplayByteLength(value);
			u32 displayCodepoints = GetUtf8CodepointCount(value, displayBytes);
			return displayBytes > displayCodepoints ? displayBytes - displayCodepoints : 0;
		}

		struct TableEntry
		{
			std::string data[columnCount];
		};

		std::string title;
		std::vector<TableEntry> entries;
		std::string headers[columnCount];
		u32 columnLengths[columnCount];

	public:
		bool Set(u32 row, u32 column, const char *value)
		{
			if (column >= columnCount)
			{
				return false;
			}
			if (row >= entries.size())
			{
				entries.resize(row + 1);
			}
			entries[row].data[column] = value;
			entries[row].data[column] += "ᅟ"; // Add a space to reset the "font" to monospace
			columnLengths[column] = MAX(columnLengths[column], GetDisplayByteLength(entries[row].data[column].c_str()));
			return true;
		}

		bool Set(u32 row, u32 column, const std::string &value)
		{
			return Set(row, column, value.c_str());
		}

		template<typename... Args>
		void SetRow(u32 row, const Args &...args)
		{
			static_assert(sizeof...(args) == N, "Number of arguments must match the column count");
			if (row >= entries.size())
			{
				entries.resize(row + 1);
			}
			entries[row] = {args...};
			for (u32 i = 0; i < columnCount; i++)
			{
				entries[row].data[i] += "ᅟ"; // Add a space to reset the "font" to monospace
				columnLengths[i] = MAX(columnLengths[i], GetDisplayByteLength(entries[row].data[i].c_str()));
			}
		}

		TableEntry GetEntry(u32 row)
		{
			return entries[row];
		}

		u32 GetTableWidth()
		{
			// N-1 gap, but there are two spaces between each column.
			u32 result = (columnCount - 1) * 2;
			for (u32 i = 0; i < columnCount; i++)
			{
				result += columnLengths[i];
			}
			return MAX(result, (u32)title.size());
		}

		std::string GetTitle()
		{
			return title;
		}

		std::string GetHeader()
		{
			std::string result;
			const u32 fontResetSuffixLength = static_cast<u32>(V_strlen(fontResetSuffix));
			for (u32 i = 0; i < columnCount; i++)
			{
				std::string value = tfm::format("%-*s%s", columnLengths[i] + GetDisplayPadding(headers[i].c_str()) + fontResetSuffixLength,
												headers[i], i == columnCount - 1 ? "" : "  ");
				for (size_t i = 0; i + 1 < value.size(); i++)
				{
					if (value[i] == '%' && value[i + 1] == '%')
					{
						value += " ";
						i++;
					}
				}
				result += value;
			}
			return result;
		}

		std::string GetLine(u32 row)
		{
			if (row >= entries.size())
			{
				return "";
			}
			std::string result;
			const u32 fontResetSuffixLength = static_cast<u32>(V_strlen(fontResetSuffix));
			for (u32 i = 0; i < columnCount; i++)
			{
				std::string value = tfm::format("%-*s%s", columnLengths[i] + GetDisplayPadding(entries[row].data[i].c_str()) + fontResetSuffixLength,
												entries[row].data[i], i == columnCount - 1 ? "" : "  ");
				for (size_t i = 0; i + 1 < value.size(); i++)
				{
					if (value[i] == '%' && value[i + 1] == '%')
					{
						value += " ";
						i++;
					}
				}
				result += value;
			}
			return result;
		}

		std::string GetSeparator(const char *characters = "_")
		{
			std::string result;
			for (u32 i = 0; i < GetTableWidth(); i++)
			{
				result += characters;
			}
			return result;
		}

		u32 GetNumEntries()
		{
			return entries.size();
		}
	};

	// Side by side dual table
	template<size_t NL, size_t NR>
	class DualTable
	{
	public:
		Table<NL> left;
		Table<NR> right;

		DualTable(const std::string &titleLeft, std::string headersLeft[NL], const std::string &titleRight, std::string headersRight[NR])
			: left(titleLeft, headersLeft), right(titleRight, headersRight)
		{
		}

		DualTable(const char *titleLeft, const char *headersLeft[NL], const char *titleRight, const char *headersRight[NR])
			: left(titleLeft, headersLeft), right(titleRight, headersRight)
		{
		}

		std::string GetTitle()
		{
			return tfm::format("%-*s\t|\t%-*s", left.GetTableWidth(), left.GetTitle(), right.GetTableWidth(), right.GetTitle());
		}

		std::string GetHeader()
		{
			return tfm::format("%-*s\t|\t%-*s", left.GetTableWidth(), left.GetHeader(), right.GetTableWidth(), right.GetHeader());
		}

		std::string GetLine(u32 row)
		{
			if (row >= GetNumEntries())
			{
				return "";
			}
			return tfm::format("%-*s\t|\t%-*s", left.GetTableWidth(), left.GetLine(row), right.GetTableWidth(), right.GetLine(row));
		}

		std::string GetSeparator(const char *characters = "_")
		{
			return tfm::format("%-*s \t|\t %-*s", left.GetTableWidth(), left.GetSeparator(characters), right.GetTableWidth(),
							   right.GetSeparator(characters));
		}

		u32 GetNumEntries()
		{
			return MAX(left.GetNumEntries(), right.GetNumEntries());
		}
	};
} // namespace utils
