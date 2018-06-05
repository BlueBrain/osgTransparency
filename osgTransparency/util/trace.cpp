/* Copyright (c) 2006-2018, École Polytechnique Fédérale de Lausanne (EPFL) /
 *                           Blue Brain Project and
 *                          Universidad Politécnica de Madrid (UPM)
 *                          Juan Hernando <juan.hernando@epfl.ch>
 *
 * This file is part of osgTransparency
 * <https://github.com/BlueBrain/osgTransparency>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "trace.h"

#ifdef OSGTRANSPARENCY_USE_EXTRAE

#include <boost/current_function.hpp>

#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

namespace bbp
{
namespace osgTransparency
{
namespace detail
{
class TranslationTable
{
public:
    TranslationTable()
        : _offset(1)
    {
        if (::getenv("OSGTRANSPARENCY_EXTRAE_VALUE_OFFSET") != 0)
        {
            const char *value = ::getenv("OSGTRANSPARENCY_EXTRAE_VALUE_OFFSET");
            int n = strtol(value, 0, 10);
            if (n > 0)
                _offset = n;
        }
    }

    ~TranslationTable() { dumpSymbols(); }
    void dumpSymbols()
    {
        if (_functions.size() == 0)
            return;

        /* Dumping the translations */
        std::ofstream labels("EXTRAE_osgTransparency_labels.pcf");
        labels << "EVENT_TYPE" << std::endl;
        labels << "0 " << FunctionTraceID::extraeType << " User function"
               << std::endl;
        labels << "VALUES" << std::endl;
        for (size_t i = 0; i != _functions.size(); ++i)
        {
            labels << (_offset + i) << ' ' << _functions[i] << std::endl;
        }

        std::vector<extrae_value_t> values;
        std::vector<char *> names;
        names.push_back((char *)"End");
        values.push_back(0);
        for (size_t i = 0; i != _functions.size(); ++i)
        {
            values.push_back(_offset + i);
            names.push_back((char *)_functions[i].c_str());
        }
        Extrae_define_event_type(FunctionTraceID::extraeType,
                                 (char *)"User function", names.size(),
                                 &values[0], &names[0]);

        _functions.clear();
    }

    int addFunction(const char *function)
    {
        std::mutex::scoped_lock lock(_mutex);
        std::string name(function);
        name = name.substr(0, name.find_first_of("("));
        if (name.find_first_of(" ") != std::string::npos)
            name = name.substr(name.find_first_of(" ") + 1, std::string::npos);
        int id = _functions.size() + _offset;
        _functions.push_back(name);
        return id;
    }

private:
    int _offset;
    std::mutex _mutex;
    std::vector<std::string> _functions;
};

TranslationTable &getTranslationTable()
{
    static TranslationTable table;
    return table;
}

FunctionTraceID::FunctionTraceID(const char *function)
{
    _id = getTranslationTable().addFunction(function);
}

FunctionTraceID::operator int() const
{
    return _id;
}

void FunctionTraceID::dumpSymbols()
{
    getTranslationTable().dumpSymbols();
}
}
}
}
#endif
