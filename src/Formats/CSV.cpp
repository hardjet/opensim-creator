#include "CSV.hpp"

#include "src/Utils/Assertions.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

// helpers
namespace
{
    bool IsSpecialCSVCharacter(char c)
    {
        return c == ',' || c == '\r' || c == '\n' || c == '"';
    }

    bool ShouldBeQuoted(std::string_view v)
    {
        return std::any_of(v.begin(), v.end(), IsSpecialCSVCharacter);
    }
}

// CSV reader implementation
class osc::CSVReader::Impl final {
public:
    Impl(std::istream& input) : m_Input{input}
    {
    }

    std::optional<std::vector<std::string>> next()
    {
        std::istream& in = m_Input.get();

        if (in.eof())
        {
            return std::nullopt;
        }

        std::vector<std::string> cols;
        std::string s;
        bool insideQuotes = false;

        while (!in.bad())
        {
            int const c = in.get();

            if (c == std::istream::traits_type::eof())
            {
                // EOF
                cols.push_back(s);
                break;
            }
            else if (c == '\n' && !insideQuotes)
            {
                // standard newline
                cols.push_back(s);
                break;
            }
            else if (c == '\r' && in.peek() == '\n' && !insideQuotes)
            {
                // windows newline

                in.get();  // skip the \n
                cols.push_back(s);
                break;
            }
            else if (c == '"' && s.empty() && !insideQuotes)
            {
                // quote at beginning of quoted column
                insideQuotes = true;
                continue;
            }
            else if (c == '"' && in.peek() == '"')
            {
                // escaped quote

                in.get();  // skip the second '"'
                s += '"';
                continue;
            }
            else if (c == '"' && insideQuotes)
            {
                // quote at end of of quoted column
                insideQuotes = false;
                continue;
            }
            else if (c == ',' && !insideQuotes)
            {
                // comma delimiter at end of column

                cols.push_back(s);
                s.clear();
                continue;
            }
            else
            {
                // normal text
                s += static_cast<char>(c);
                continue;
            }
        }

        if (!cols.empty())
        {
            return cols;
        }
        else
        {
            return std::nullopt;
        }
    }

private:
    std::reference_wrapper<std::istream> m_Input;
};

// CSV writer implementation
class osc::CSVWriter::Impl final {
public:
    Impl(std::ostream& output) : m_Output{output}
    {
    }

    void writeRow(std::vector<std::string> const& cols)
    {
        std::ostream& out = m_Output.get();

        char const* delim = "";
        for (std::string const& col : cols)
        {
            bool const quoted = ShouldBeQuoted(col);

            out << delim;
            if (quoted)
            {
                out << '"';
            }

            for (char c : col)
            {
                if (c != '"')
                {
                    out << c;
                }
                else
                {
                    out << '"' << '"';
                }
            }

            if (quoted)
            {
                out << '"';
            }

            delim = ",";
        }
        out << '\n';
    }

private:
    std::reference_wrapper<std::ostream> m_Output;
};


// public API (PIMPL)

osc::CSVReader::CSVReader(std::istream& input) :
    m_Impl{std::make_unique<Impl>(input)}
{
}

osc::CSVReader::CSVReader(CSVReader&&) noexcept = default;
osc::CSVReader& osc::CSVReader::operator=(CSVReader&&) noexcept = default;
osc::CSVReader::~CSVReader() noexcept = default;

std::optional<std::vector<std::string>> osc::CSVReader::next()
{
    return m_Impl->next();
}


osc::CSVWriter::CSVWriter(std::ostream& output) :
    m_Impl{std::make_unique<Impl>(output)}
{
}

osc::CSVWriter::CSVWriter(CSVWriter&&) noexcept = default;
osc::CSVWriter& osc::CSVWriter::operator=(CSVWriter&&) noexcept = default;
osc::CSVWriter::~CSVWriter() noexcept = default;

void osc::CSVWriter::writeRow(std::vector<std::string> const& cols)
{
    m_Impl->writeRow(cols);
}