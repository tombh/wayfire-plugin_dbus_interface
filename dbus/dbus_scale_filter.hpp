/*********************************************************************
* This file is licensed under the MIT license.
* Copyright (C) 2020 Daniel Kondor <kondor.dani@gmail.com>
*
* dbus_scale_filter.hpp -- helper class to filter scaled apps by dbus
*********************************************************************/

#ifndef DBUS_SCALE_FILTER_HPP
#define DBUS_SCALE_FILTER_HPP

#include <cctype>
#include <string>
#include <algorithm>
#include <wayfire/output.hpp>
#include <wayfire/plugins/scale-signal.hpp>

/* one instance of each of this class is added to each output */
class dbus_scale_filter : public wf::custom_data_t
{
  protected:
    std::string filter;

    static unsigned char
    transform (unsigned char c)
    {
        if (std::isspace(c))
        {
            return ' ';
        }

        return (c <= 127) ? (unsigned char)std::tolower(c) : c;
    }

    bool
    should_show_view (wayfire_view v)
    {
        if (filter.empty())
        {
            return true;
        }

        std::string app_id = v->get_app_id();
        std::transform(app_id.begin(), app_id.end(), app_id.begin(), transform);

        return filter == app_id;
    }

    wf::signal_connection_t view_filter{[this] (wf::signal_data_t* data)
        {
            auto signal = static_cast<scale_filter_signal*> (data);
            scale_filter_views(signal, [this] (wayfire_view v)
            {
                return !should_show_view(v);
            });
        }
    };

    wf::signal_connection_t scale_end{[this] (auto)
        {
            filter.clear();
        }
    };

    void
    connect_signals (wf::output_t* output)
    {
        output->connect_signal("scale-filter", &view_filter);
        output->connect_signal("scale-end", &scale_end);
    }

  public:
    static nonstd::observer_ptr<dbus_scale_filter>
    get (wf::output_t* output)
    {
        auto ret = output->get_data<dbus_scale_filter> ();
        if (ret)
        {
            return ret;
        }

        dbus_scale_filter* new_ret = new dbus_scale_filter();
        output->store_data(std::unique_ptr<dbus_scale_filter> (new_ret));
        new_ret->connect_signals(output);

        return new_ret;
    }

    void
    set_filter (const std::string& new_filter)
    {
        filter = new_filter;
        std::transform(filter.begin(), filter.end(), filter.begin(), transform);
    }

    void
    set_filter (std::string&& new_filter)
    {
        filter = std::move(new_filter);
        std::transform(filter.begin(), filter.end(), filter.begin(), transform);
    }

    // unload all instances of filters attached to all outputs
    static void
    unload ()
    {
        wf::compositor_core_t& core = wf::get_core();
        for (auto output : core.output_layout->get_outputs())
        {
            output->erase_data<dbus_scale_filter> ();
        }
    }
};

#endif