#include "hlds/TitlesFile.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace hlds
{
    namespace
    {
        std::string upper(std::string text)
        {
            for (char &c : text)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return text;
        }

        std::string trimmed(const std::string &line)
        {
            std::size_t from = 0, to = line.size();
            while (from < to && std::isspace(static_cast<unsigned char>(line[from])))
                ++from;
            while (to > from && std::isspace(static_cast<unsigned char>(line[to - 1])))
                --to;
            return line.substr(from, to - from);
        }
    } // namespace

    const Title *TitlesBook::find(const std::string &name) const
    {
        const auto found = titles.find(upper(name));
        return found != titles.end() ? &found->second : nullptr;
    }

    TitlesBook parseTitles(const std::string &text)
    {
        TitlesBook book;
        Title now; // the settings as they stand
        std::string name;
        bool inside = false;
        std::string body;
        std::istringstream in(text);
        std::string raw;
        while (std::getline(in, raw))
        {
            if (!raw.empty() && raw.back() == '\r')
                raw.pop_back();
            const std::string line = trimmed(raw);
            if (inside)
            {
                if (line == "}")
                {
                    // (The line before the brace ends it: no empty last line.)
                    while (!body.empty() && body.back() == '\n')
                        body.pop_back();
                    Title title = now;
                    title.text = body;
                    book.titles[upper(name)] = title;
                    inside = false;
                    name.clear();
                    body.clear();
                }
                else
                {
                    // (Text is as it is written, blank lines and all.)
                    body += raw;
                    body += '\n';
                }
                continue;
            }
            if (line.empty() || line.compare(0, 2, "//") == 0)
                continue;
            if (line[0] == '$')
            {
                std::istringstream words(line.substr(1));
                std::string key;
                words >> key;
                double v[3] = {0, 0, 0};
                int given = 0;
                while (given < 3 && (words >> v[given]))
                    ++given;
                if (key == "position" && given >= 2)
                {
                    now.x = v[0];
                    now.y = v[1];
                }
                else if (key == "effect" && given >= 1)
                    now.effect = static_cast<int>(v[0]);
                else if ((key == "color" || key == "color2") && given >= 3)
                {
                    for (int i = 0; i < 3; ++i)
                        (key == "color" ? now.colour : now.colour2)[i] = std::clamp(static_cast<int>(v[i]), 0, 255);
                }
                else if (key == "fadein" && given >= 1)
                    now.fadeIn = v[0];
                else if (key == "fadeout" && given >= 1)
                    now.fadeOut = v[0];
                else if (key == "holdtime" && given >= 1)
                    now.hold = v[0];
                else if (key == "fxtime" && given >= 1)
                    now.fxTime = v[0];
                continue;
            }
            if (line == "{")
            {
                inside = !name.empty();
                continue;
            }
            name = line;
        }
        return book;
    }
} // namespace hlds
