#pragma once
#include "common.h"
#include "utlvector.h"

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

		Table(CUtlString title, CUtlString headers[columnCount]) : title(title)
		{
			for (u32 i = 0; i < columnCount; i++)
			{
				this->headers[i] = headers[i];
				columnLengths[i] = this->headers[i].Length();
			}
		}

		Table(const char *title, const char *headers[columnCount]) : title(title)
		{
			for (u32 i = 0; i < columnCount; i++)
			{
				this->headers[i] = headers[i];
				columnLengths[i] = this->headers[i].Length();
			}
		}

	private:
		struct TableEntry
		{
			CUtlString data[columnCount];
		};

		CUtlString title;
		CUtlVector<TableEntry> entries;
		CUtlString headers[columnCount];
		u32 columnLengths[columnCount];

	public:
		bool Set(u32 row, u32 column, const char *value)
		{
			if (column >= columnCount)
			{
				return false;
			}
			if (row >= entries.Count())
			{
				entries.SetCountNonDestructively(row + 1);
			}
			entries[row].data[column] = value;
			columnLengths[column] = MAX(columnLengths[column], V_strlen(value));
			return true;
		}

		bool Set(u32 row, u32 column, CUtlString value)
		{
			return Set(row, column, value.Get());
		}

		template<typename... Args>
		void SetRow(u32 row, const Args &...args)
		{
			static_assert(sizeof...(args) == N, "Number of arguments must match the column count");
			if (row >= (u32)entries.Count())
			{
				entries.SetCountNonDestructively(row + 1);
			}
			entries[row] = {args...};
			for (u32 i = 0; i < columnCount; i++)
			{
				columnLengths[i] = MAX(columnLengths[i], (u32)V_strlen(entries[row].data[i].Get()));
			}
		}

		u32 GetTableWidth()
		{
			// N-1 gap, but there are two spaces between each column.
			u32 result = (columnCount - 1) * 2;
			for (u32 i = 0; i < columnCount; i++)
			{
				result += columnLengths[i];
			}
			return MAX(result, (u32)title.Length());
		}

		CUtlString GetTitle()
		{
			return title;
		}

		CUtlString GetHeader()
		{
			CUtlString result;
			for (u32 i = 0; i < columnCount; i++)
			{
				CUtlString value;
				value.Format("%-*s%s", columnLengths[i], headers[i].Get(), i == columnCount - 1 ? "" : "  ");
				for (i32 i = 0; i < value.Length() - 1; i++)
				{
					if (value[i] == '%' && value[i + 1] == '%')
					{
						value.Append(" ");
						i++;
					}
				}
				result.Append(value.Get());
			}
			return result;
		}

		CUtlString GetLine(u32 row)
		{
			if (row >= (u32)entries.Count())
			{
				return "";
			}
			CUtlString result;
			for (u32 i = 0; i < columnCount; i++)
			{
				CUtlString value;
				value.Format("%-*s%s", columnLengths[i], entries[row].data[i].Get(), i == columnCount - 1 ? "" : "  ");
				for (i32 i = 0; i < value.Length() - 1; i++)
				{
					if (value[i] == '%' && value[i + 1] == '%')
					{
						value.Append(" ");
						i++;
					}
				}
				result.Append(value.Get());
			}
			return result;
		}

		CUtlString GetSeparator()
		{
			CUtlString result;
			for (u32 i = 0; i < GetTableWidth(); i++)
			{
				result += "_";
			}
			return result;
		}

		u32 GetNumEntries()
		{
			return entries.Count();
		}
	};

	// Side by side dual table
	template<size_t NL, size_t NR>
	class DualTable
	{
	public:
		Table<NL> left;
		Table<NR> right;

		DualTable(CUtlString titleLeft, CUtlString headersLeft[NL], CUtlString titleRight, CUtlString headersRight[NR])
			: left(titleLeft, headersLeft), right(titleRight, headersRight)
		{
		}

		DualTable(const char *titleLeft, const char *headersLeft[NL], const char *titleRight, const char *headersRight[NR])
			: left(titleLeft, headersLeft), right(titleRight, headersRight)
		{
		}

		CUtlString GetTitle()
		{
			CUtlString result;
			result.Format("%-*s | %-*s", left.GetTableWidth(), left.GetTitle().Get(), right.GetTableWidth(), right.GetTitle().Get());
			return result;
		}

		CUtlString GetHeader()
		{
			CUtlString result;
			result.Format("%-*s | %-*s", left.GetTableWidth(), left.GetHeader().Get(), right.GetTableWidth(), right.GetHeader().Get());
			return result;
		}

		CUtlString GetLine(u32 row)
		{
			if (row >= GetNumEntries())
			{
				return "";
			}
			CUtlString result;
			result.Format("%-*s | %-*s", left.GetTableWidth(), left.GetLine(row).Get(), right.GetTableWidth(), right.GetLine(row).Get());
			return result;
		}

		CUtlString GetSeparator()
		{
			CUtlString result;
			result.Format("%-*s | %-*s", left.GetTableWidth(), left.GetSeparator().Get(), right.GetTableWidth(), right.GetSeparator().Get());
			return result;
		}

		u32 GetNumEntries()
		{
			return MAX(left.GetNumEntries(), right.GetNumEntries());
		}
	};
} // namespace utils
