/*******************************************************************
 * This file is licensed under the MIT license.
 * Copyright (C) 2019 - 2020 Damian Ivanov <damianatorrpm@gmail.com>
 ********************************************************************/
#define DBUS_PLUGIN_DEBUG TRUE
#define DBUS_PLUGIN_WARN TRUE

extern "C" {
#define class class_t
#define static
#include <X11/Xatom.h>
#include <wlr/xwayland.h>
#include <xcb/res.h>
#include <xcb/xcb.h>
// #include <xwayland/xwm.h>
#undef static
#undef class
#include <sys/socket.h>
#include <wlr/types/wlr_idle.h>
};

#include <gio/gio.h>
#include <ctime>
#include <iostream>
#include <list>
#include <set>

#include <unistd.h>

#include "dbus_scale_filter.hpp"
#include "wayfire/view-transform.hpp"
#include <wayfire/compositor-view.hpp>
#include <wayfire/core.hpp>
#include <wayfire/debug.hpp>
#include <wayfire/gtk-shell.hpp>
#include <wayfire/option-wrapper.hpp>
#include <wayfire/output-layout.hpp>
#include <wayfire/output.hpp>
#include <wayfire/plugin.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/util.hpp>
#include <wayfire/util/duration.hpp>
#include <wayfire/util/log.hpp>
#include <wayfire/view.hpp>
#include <wayfire/workspace-manager.hpp>

wf::option_wrapper_t<bool> xwayland_enabled("core/xwayland");

wf::compositor_core_t& core = wf::get_core();
std::vector<wf::output_t*> wf_outputs = core.output_layout->get_outputs();
std::set<wf::output_t*> connected_wf_outputs;
GSettings* settings;
std::map<wf::output_t*, std::unique_ptr<wf::plugin_grab_interface_t>>
grab_interfaces;

uint focused_view_id;
bool find_view_under_action = false;
GDBusNodeInfo* introspection_data = nullptr;
GDBusConnection* dbus_connection;
uint owner_id;

static gboolean
check_view_toplevel (wayfire_view view)
{
    if (!view) {
        return FALSE;
    }

    if (!view->is_mapped()) {
        return FALSE;
    }

    if (view->role != wf::VIEW_ROLE_TOPLEVEL) {
        return FALSE;
    }

    if (!view->get_output()) {
        return FALSE;
    }

    return TRUE;
}

static wayfire_view
get_view_from_view_id (uint view_id)
{
    std::vector<wayfire_view> view_vector;
    wayfire_view view;

    view_vector = core.get_all_views();

    // there is no view_id 0 use it as get_active_view(hint)
    if (view_id == 0) {
        view = core.get_cursor_focus_view();
        if (check_view_toplevel(view)) {
            return view;
        }
    }

    for (auto it = view_vector.begin(); it != view_vector.end(); ++it)
    {
        wayfire_view v = *it;
        if (check_view_toplevel(v)) {
            if (v->get_id() == view_id) {
                return v;
            }
        }
    }

    return view;
}

static wf::output_t*
get_output_from_output_id (uint output_id)
{
    for (wf::output_t* wf_output : wf_outputs)
    {
        if (wf_output->get_id() == output_id) {
            return wf_output;
        }
    }

    return nullptr;
}

static void
restack_view (uint view_id, uint related_view_id, gboolean above)
{
    if (view_id == related_view_id) {
        return;
    }

    wf::wl_idle_call* idle_call = new wf::wl_idle_call;
    idle_call->run_once([=] ()
    {
        wayfire_view view = get_view_from_view_id(view_id);
        wayfire_view related_view = get_view_from_view_id(related_view_id);

        if (!check_view_toplevel(view) || !check_view_toplevel(related_view)) {
            delete idle_call;

            return;
        }

        wf::output_t* output = view->get_output();
        if (!output) {
            delete idle_call;

            return;
        }

        if (above) {
            view->get_output()->workspace->restack_above(view, related_view);
        }
        else
        {
            view->get_output()->workspace->restack_below(view, related_view);
        }

        delete idle_call;
    });
}

/*
 * It is a deliberate design choice to have
 * methods / signals instead of properties
 *
 * Not all clients fully support
 * automatic property change notifcations!
 */
const gchar introspection_xml [] =
    "<node>"
    "  <interface name='org.wayland.compositor'>"
    // "<property type='au' name='view_vector_ids' access='read'/>"
    // kept as example
    /************************* Methods ************************/
    // Draft methods
    // "    <method name='inhibit_output_start'/>"
    // "    <method name='inhibit_output_stop'/>"

    // "    <method name='trigger_show_desktop'/>"
    // "    <method name='trigger_show_overview'/>"

    "    <method name='query_cursor_position'>"
    "      <arg type='d' name='x_pos' direction='out'/>"
    "      <arg type='d' name='y_pos' direction='out'/>"
    "    </method>"
    "    <method name='enable_property_mode'>"
    "      <arg type='b' name='enable' direction='in'/>"
    "    </method>"

    /************************* Output Methods ************************/
    "    <method name='query_output_ids'>"
    "      <arg direction='out' type='au' />"
    "    </method>"
    "    <method name='query_active_output'>"
    "      <arg type='u' name='output_id' direction='out'/>"
    "    </method>"
    "    <method name='query_output_name'>"
    "      <arg type='u' name='output_id' direction='in'/>"
    "      <arg type='s' name='name' direction='out'/>"
    "    </method>"
    "    <method name='query_output_manufacturer'>"
    "      <arg type='u' name='output_id' direction='in'/>"
    "      <arg type='s' name='name' direction='out'/>"
    "    </method>"
    "    <method name='query_output_model'>"
    "      <arg type='u' name='output_id' direction='in'/>"
    "      <arg type='s' name='name' direction='out'/>"
    "    </method>"
    "    <method name='query_output_serial'>"
    "      <arg type='u' name='output_id' direction='in'/>"
    "      <arg type='s' name='name' direction='out'/>"
    "    </method>"
    "    <method name='query_output_workspace'>"
    "      <arg type='u' name='output_id' direction='in'/>"
    "      <arg type='u' name='xHorizontal' direction='out'/>"
    "      <arg type='u' name='yVertical' direction='out'/>"
    "    </method>"
    "    <method name='query_xwayland_display'>"
    "      <arg type='s' name='xdisplay' direction='out'/>"
    "    </method>"
    /************************* View Methods ************************/
    "    <method name='query_view_vector_ids'>"
    "      <arg direction='out' type='au' />"
    "    </method>"
    "    <method name='query_view_vector_taskman_ids'>"
    "      <arg direction='out' type='au' />"
    "    </method>"
    "    <method name='query_view_app_id'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='app_id' direction='out'/>"
    "    </method>"
    "    <method name='query_view_app_id_gtk_shell'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='app_id' direction='out'/>"
    "    </method>"
    "    <method name='query_view_app_id_xwayland_net_wm_name'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='app_id' direction='out'/>"
    "    </method>"
    "    <method name='query_view_title'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='title' direction='out'/>"
    "    </method>"
    "    <method name='query_view_credentials'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='i' name='pid' direction='out'/>"
    "      <arg type='u' name='uid' direction='out'/>"
    "      <arg type='u' name='gid' direction='out'/>"
    "    </method>"
    "    <method name='query_view_active'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='view_state' direction='out'/>"
    "    </method>"
    "    <method name='query_view_minimized'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='view_state' direction='out'/>"
    "    </method>"
    "    <method name='query_view_maximized'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='view_state' direction='out'/>"
    "    </method>"
    "    <method name='query_view_fullscreen'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='view_state' direction='out'/>"
    "    </method>"
    "    <method name='query_view_output'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='output' direction='out'/>"
    "    </method>"
    "    <method name='query_view_above'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='above' direction='out'/>"
    "    </method>"
    "    <method name='query_view_workspaces'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='a(ii)' name='workspaces' direction='out'/>"
    "    </method>"
    "    <method name='query_view_group_leader'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='view_group_leader_view_id' direction='out'/>"
    "    </method>"
    "    <method name='query_view_role'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='view_state' direction='out'/>"
    "    </method>"
    "    <method name='query_workspace_grid_size'>"
    "      <arg type='i' name='rows' direction='out'/>"
    "      <arg type='i' name='columns' direction='out'/>"
    "    </method>"
    "    <method name='query_view_attention'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='b' name='attention' direction='out'/>"
    "    </method>"
    "    <method name='query_view_xwayland_wid'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='xwayland_wid' direction='out'/>"
    "    </method>"
    "    <method name='query_view_xwayland_atom_cardinal'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='atom' direction='in'/>"
    "      <arg type='u' name='atom_value' direction='out'/>"
    "    </method>"
    "    <method name='query_view_xwayland_atom_string'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='s' name='atom' direction='in'/>"
    "      <arg type='s' name='atom_value' direction='out'/>"
    "    </method>"
    "    <method name='query_view_test_data'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='value' direction='out'/>"
    "      <arg type='u' name='value2' direction='out'/>"
    "    </method>"
    "    <method name='query_view_below_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='i' name='below_view_id' direction='out'/>"
    "    </method>"
    "    <method name='query_view_above_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='i' name='above_view_id' direction='out'/>"
    "    </method>"
    "    <method name='minimize_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='action' direction='in'/>"
    "    </method>"
    "    <method name='maximize_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='action' direction='in'/>"
    "    </method>"
    "    <method name='focus_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='action' direction='in'/>"
    "    </method>"
    "    <method name='fullscreen_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='action' direction='in'/>"
    "    </method>"
    "    <method name='close_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "    </method>"
    "    <method name='change_view_minimize_hint'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='i' name='a' direction='in'/>"
    "      <arg type='i' name='b' direction='in'/>"
    "      <arg type='i' name='c' direction='in'/>"
    "      <arg type='i' name='d' direction='in'/>"
    "    </method>"
    "    <method name='change_output_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='output' direction='in'/>"
    "    </method>"
    "    <method name='change_workspace_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='i' name='workspace_horizontal' direction='in'/>"
    "      <arg type='i' name='workspace_vertical' direction='in'/>"
    "    </method>"
    "    <method name='change_workspace_output'>"
    "      <arg type='u' name='output' direction='in'/>"
    "      <arg type='i' name='workspace_horizontal' direction='in'/>"
    "      <arg type='i' name='workspace_vertical' direction='in'/>"
    "    </method>"
    "    <method name='change_workspace_all_outputs'>"
    "      <arg type='i' name='workspace_horizontal' direction='in'/>"
    "      <arg type='i' name='workspace_vertical' direction='in'/>"
    "    </method>"
    "    <method name='change_view_above'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='action' direction='in'/>"
    "    </method>"
    "    <method name='shade_view'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='d' name='value' direction='in'/>"
    "    </method>"
    "    <method name='bring_view_to_front'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "    </method>"
    "    <method name='restack_view_above'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='view_id_now_above_view_id' direction='in'/>"
    "    </method>"
    "    <method name='restack_view_below'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "      <arg type='u' name='view_id_now_below_view_id' direction='in'/>"
    "    </method>"
    "    <method name='update_view_minimize_hint'>"
    "      <arg type='u' name='view_id' direction='in'/>"
    "    </method>"
    "    <method name='show_desktop'>"
    "      <arg type='b' name='show' direction='in'/>"
    "    </method>"
    "   <method name='scale'>"
    "     <arg type='b' name='all_workspaces' direction='in'/>"
    "     <arg type='s' name='app_id_filter' direction='in'/>"
    "   </method>"
    "   <method name='ensure_view_visible'>"
    "     <arg type='u' name='view_id' direction='in'/>"
    "   </method>"
    /************************* Signals ************************/
    /***
     * Core Input Signals
     ***/
    "    <signal name='pointer_clicked'>"
    "      <arg type='d' name='x_pos'/>"
    "      <arg type='d' name='y_pos'/>"
    "      <arg type='u' name='button'/>"
    "      <arg type='b' name='button_released'/>"
    "    </signal>"
    "    <signal name='tablet_touched'/>"

    /***
     * View related signals, emitted from various
     * sources
     ***/
    "    <signal name='view_added'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_closed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_app_id_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='s' name='new_app_id'/>"
    "    </signal>"
    "    <signal name='view_title_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='s' name='new_title'/>"
    "    </signal>"
    "    <signal name='view_output_move_requested'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='u' name='old_output'/>"
    "      <arg type='u' name='new_output'/>"
    "    </signal>"
    "    <signal name='view_output_moved'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='u' name='old_output'/>"
    "      <arg type='u' name='new_output'/>"
    "    </signal>"
    "    <signal name='view_workspaces_changed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_attention_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='b' name='attention'/>"
    "    </signal>"
    "    <signal name='view_group_leader_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='u' name='view_group_leader_view_id'/>"
    "    </signal>"
    "    <signal name='view_tiling_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='u' name='edges'/>"
    "    </signal>"
    "    <signal name='view_geometry_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='i' name='x'/>"
    "      <arg type='i' name='y'/>"
    "      <arg type='i' name='width'/>"
    "      <arg type='i' name='height'/>"
    "    </signal>"
    "    <signal name='view_moving_changed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_resizing_changed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_role_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='u' name='view_role'/>"
    "    </signal>"
    "    <signal name='view_maximized_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='b' name='maximized'/>"
    "    </signal>"
    "    <signal name='view_minimized_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='b' name='minimized'/>"
    "    </signal>"
    "    <signal name='view_fullscreen_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='b' name='fullscreened'/>"
    "    </signal>"
    "    <signal name='view_focus_changed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    "    <signal name='view_keep_above_changed'>"
    "      <arg type='u' name='view_id'/>"
    "      <arg type='b' name='above'/>"
    "    </signal>"
    /***
     * Output related signals, emitted from various
     * sources
     ***/
    "    <signal name='output_workspace_changed'>"
    "      <arg type='u' name='output_id'/>"
    "      <arg type='i' name='workspace_horizontal'/>"
    "      <arg type='i' name='workspace_vertical'/>"
    "    </signal>"
    "    <signal name='output_added'>"
    "      <arg type='u' name='output_id'/>"
    "    </signal>"
    "    <signal name='output_removed'>"
    "      <arg type='u' name='output_id'/>"
    "    </signal>"
    "    <signal name='output_configuration_changed'/>"

    /***
     * For wf-prop & co
     ***/
    "    <signal name='view_pressed'>"
    "      <arg type='u' name='view_id'/>"
    "    </signal>"
    /***
     * Tentative signals
     *  "    <signal name='hotspot_edge_trigger_stop'>"
     *  "      <arg type='u' name='hotspot_edge'/>"
     *  "    </signal>"
     *  "    <signal name='hotspot_edge_triggered'>"
     *  "      <arg type='i' name='hotspot_edge'/>"
     *  "    </signal>"
     *  "    <signal name='hotspot_edge_trigger_stop'>"
     *  "      <arg type='i' name='hotspot_edge'/>"
     *  "    </signal>"
     *
     *  "    <signal name='inhibit_output_started'/>"
     *  "    <signal name='inhibit_output_stopped'/>"
     ***/

    "  </interface>"
    "</node>";

static void
handle_method_call (GDBusConnection* connection, const gchar* sender,
                    const gchar* object_path,
                    const gchar* interface_name,
                    const gchar* method_name, GVariant* parameters,
                    GDBusMethodInvocation* invocation,
                    gpointer user_data)
{
#ifdef DBUS_PLUGIN_DEBUG
    LOG(wf::log::LOG_LEVEL_DEBUG, "handle_method_call bus called ", method_name);
#endif

    if (g_strcmp0(method_name, "change_view_above") == 0) {
        uint view_id;
        uint action;

        g_variant_get(parameters, "(uu)", &view_id, &action);
        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            bool is_above;
            wf::output_t* output;
            wf::_view_signal signal_data;
            is_above = view->has_data("wm-actions-above");
            output = view->get_output();

            if ((action == 0) && is_above) {
                signal_data.view = view;
                output->emit_signal("wm-actions-toggle-above", &signal_data);
            }
            else
            if ((action == 1) && !is_above)
            {
                signal_data.view = view;
                output->emit_signal("wm-actions-toggle-above", &signal_data);
            }
            else
            if (action == 2)
            {
                signal_data.view = view;
                output->emit_signal("wm-actions-toggle-above", &signal_data);
            }

            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }

    /*************** View Actions ****************/
    else
    if (g_strcmp0(method_name, "ensure_view_visible") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);
        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);

            if (check_view_toplevel(view)) {
                view->get_output()->ensure_visible(view);
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "update_view_minimize_hint") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);
        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);

            if (check_view_toplevel(view)) {
                wf::pointf_t pos;
                pos = core.get_active_output()->get_cursor_position();
                view->set_minimize_hint({(int)pos.x, (int)pos.y, 5, 5});
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "shade_view") == 0)
    {
        uint view_id;
        double intensity;

        g_variant_get(parameters, "(ud)", &view_id, &intensity);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            if (intensity == 1.0) {
                if (view->get_transformer("dbus-shade")) {
                    view->pop_transformer("dbus-shade");
                }
            }
            else
            {
                wf::view_2D* transformer;
                if (!view->get_transformer("dbus-shade")) {
                    view->add_transformer(std::make_unique<wf::view_2D> (view),
                                          "dbus-shade");
                }

                transformer = dynamic_cast<wf::view_2D*> (
                    view->get_transformer("dbus-shade").get());

                if (transformer->alpha != (float)intensity) {
                    transformer->alpha = (float)intensity;
                    // view->damage();
                }
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "bring_view_to_front") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);

            if (check_view_toplevel(view)) {
                wf::output_t* output = view->get_output();
                output->workspace->bring_to_front(view);
            }

            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "restack_view_above") == 0)
    {
        uint view_id;
        uint related_view_id;
        g_variant_get(parameters, "(uu)", &view_id, &related_view_id);
        restack_view(view_id, related_view_id, TRUE);
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "restack_view_below") == 0)
    {
        uint view_id;
        uint related_view_id;
        g_variant_get(parameters, "(uu)", &view_id, &related_view_id);
        restack_view(view_id, related_view_id, FALSE);
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "minimize_view") == 0)
    {
        uint view_id;
        uint action;

        g_variant_get(parameters, "(uu)", &view_id, &action);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            if ((action == 0) && view->minimized) {
                view->minimize_request(false);
            }

            else
            if ((action == 1) && !view->minimized)
            {
                view->minimize_request(true);
            }

            else
            if (action == 2)
            {
                view->minimize_request(!view->minimized);
            }

            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "maximize_view") == 0)
    {
        uint view_id;
        uint action;
        g_variant_get(parameters, "(uu)", &view_id, &action);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            if (action == 0) {
                view->tile_request(0);
            }

            else
            if (action == 1)
            {
                view->tile_request(wf::TILED_EDGES_ALL);
            }

            else
            if (action == 2)
            {
                if (view->tiled_edges == wf::TILED_EDGES_ALL) {
                    view->tile_request(0);
                }
                else
                {
                    view->tile_request(wf::TILED_EDGES_ALL);
                }
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "focus_view") == 0)
    {
        uint view_id;
        uint action;
        g_variant_get(parameters, "(uu)", &view_id, &action);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            if (action == 0) {
                view->set_activated(false);
            }

            else
            if (action == 1)
            {
                view->set_activated(true);
                view->focus_request();
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "fullscreen_view") == 0)
    {
        uint view_id;
        uint action;
        g_variant_get(parameters, "(uu)", &view_id, &action);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            wf::output_t* output = core.get_active_output();

            if (action == 0) {
                view->fullscreen_request(output, false);
            }

            else
            if (action == 1)
            {
                view->fullscreen_request(output, true);
            }

            else
            if (action == 2)
            {
                view->fullscreen_request(output, !view->fullscreen);
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "close_view") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);

            if (check_view_toplevel(view)) {
                view->close();
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "change_view_minimize_hint") == 0)
    {
        uint view_id;
        int x, y, width, height;

        g_variant_get(parameters, "(uiiii)", &view_id, &x, &y, &width, &height);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            view->set_minimize_hint({x, y, width, height});
            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, nullptr);

        return;
    }

    else
    if (g_strcmp0(method_name, "change_output_view") == 0)
    {
        uint view_id;
        uint output_id;

        g_variant_get(parameters, "(uu)", &view_id, &output_id);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            wf::output_t* output = get_output_from_output_id(output_id);
            if (output) {
                core.move_view_to_output(view, output, TRUE);
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "change_workspace_view") == 0)
    {
        uint view_id;
        int new_workspace_x;
        int new_workspace_y;

        g_variant_get(parameters, "(uii)", &view_id, &new_workspace_x,
                      &new_workspace_y);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wayfire_view view = get_view_from_view_id(view_id);
            if (!check_view_toplevel(view)) {
                delete idle_call;

                return;
            }

            wf::point_t new_workspace_coord = {new_workspace_x, new_workspace_y};
            wf::output_t* output = view->get_output();
            output->workspace->move_to_workspace(view, new_workspace_coord);
            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "change_workspace_output") == 0)
    {
        uint output_id;
        int new_workspace_x;
        int new_workspace_y;

        g_variant_get(parameters, "(uii)", &output_id, &new_workspace_x,
                      &new_workspace_y);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wf::output_t* output = get_output_from_output_id(output_id);

            if (output) {
                wf::point_t new_workspace_coord;
                new_workspace_coord = {new_workspace_x, new_workspace_y};
                output->workspace->request_workspace(new_workspace_coord);
                // Provides animation if available
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "change_workspace_all_outputs") == 0)
    {
        int new_workspace_x;
        int new_workspace_y;

        g_variant_get(parameters, "(ii)", &new_workspace_x, &new_workspace_y);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once([=] ()
        {
            wf::point_t new_workspace_coord;
            new_workspace_coord = {new_workspace_x, new_workspace_y};

            for (wf::output_t* output : wf_outputs)
            {
                if (output) {
                    output->workspace->request_workspace(new_workspace_coord);
                }
            }

            delete idle_call;
        });
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "show_desktop") == 0)
    {
        // g_variant_ref(parameters);
        // wl_event_loop_add_idle(core.ev_loop,
        // local_thread_show_desktop,
        // static_cast<void*> (parameters));
        g_dbus_method_invocation_return_value(invocation, NULL);

        return;
    }
    else
    if (g_strcmp0(method_name, "scale") == 0)
    {
        gboolean all_workspaces = FALSE;
        gchar* app_id = nullptr;
        g_variant_get(parameters, "(bs)", &all_workspaces, &app_id);

        wf::wl_idle_call* idle_call = new wf::wl_idle_call;
        idle_call->run_once(
            [all_workspaces, app_id = std::string(app_id), idle_call] ()
        {
            wf::output_t* output = core.get_active_output();
            auto filter = dbus_scale_filter::get(output);
            filter->set_filter(std::move(app_id));

            if (output->is_plugin_active("scale")) {
                output->emit_signal("scale-update", nullptr);
            }
            else
            {
                wf::activator_data_t adata;
                adata.source = wf::activator_source_t::PLUGIN;
                output->call_plugin(
                    all_workspaces ? "scale/toggle_all" : "scale/toggle", adata);
            }

            delete idle_call;
        });

        g_dbus_method_invocation_return_value(invocation, nullptr);
    }

    /*************** Non-reffing actions at end ****************/
    else
    if (g_strcmp0(method_name, "enable_property_mode") == 0)
    {
        bool enable;
        g_variant_get(parameters, "(b)", &enable);
        find_view_under_action = enable;

        /**
         * Eventually store current cursor
         * and restore it if different from
         * "default"
         */
        if (enable) {
            for (wf::output_t* output : wf_outputs)
            {
                if (!output->activate_plugin(grab_interfaces[output])) {
                    continue;
                }

                grab_interfaces[output]->grab();
            }

            wf::wl_idle_call* idle_call = new wf::wl_idle_call;
            idle_call->run_once([core, idle_call] ()
            {
                core.set_cursor("crosshair");
                delete idle_call;
            });
        }
        else
        {
            for (wf::output_t* output : wf_outputs)
            {
                output->deactivate_plugin(grab_interfaces[output]);
                grab_interfaces[output]->ungrab();
            }

            wf::wl_idle_call* idle_call = new wf::wl_idle_call;
            idle_call->run_once([=] ()
            {
                core.set_cursor("default");
                delete idle_call;
            });
        }

        g_dbus_method_invocation_return_value(invocation, nullptr);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_cursor_position") == 0)
    {
        /*
         * It uses the output relative cursor position
         * as expected by minimize rect and popup positions
         */
        wf::pointf_t cursor_position;
        GVariant* value;

        cursor_position = core.get_active_output()->get_cursor_position();
        value = g_variant_new("(dd)", cursor_position.x, cursor_position.y);
        g_dbus_method_invocation_return_value(invocation, value);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_output_ids") == 0)
    {
        GVariantBuilder builder;
        GVariant* value;

        g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));

        for (wf::output_t* wf_output : wf_outputs)
        {
            g_variant_builder_add(&builder, "u", wf_output->get_id());
        }

        value = g_variant_new("(au)", &builder);
        g_dbus_method_invocation_return_value(invocation, value);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_active_output") == 0)
    {
        uint output_id;
        output_id = core.get_active_output()->get_id();
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(u)", output_id));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_vector_ids") == 0)
    {
        std::vector<nonstd::observer_ptr<wf::view_interface_t>> view_vector;
        GVariantBuilder builder;
        GVariant* value;

        view_vector = core.get_all_views();
        g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));
        for (auto it = begin(view_vector); it != end(view_vector); ++it)
        {
            g_variant_builder_add(&builder, "u", it->get()->get_id());
        }

        value = g_variant_new("(au)", &builder);
        g_dbus_method_invocation_return_value(invocation, value);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_vector_taskman_ids") == 0)
    {
        std::vector<nonstd::observer_ptr<wf::view_interface_t>> view_vector =
            core.get_all_views();
        GVariantBuilder builder;
        GVariant* value;

        g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));
        for (auto it = begin(view_vector); it != end(view_vector); ++it)
        {
            if ((it->get()->role != wf::VIEW_ROLE_TOPLEVEL) ||
                !it->get()->is_mapped()) {
                continue;
            }
            else
            {
                g_variant_builder_add(&builder, "u", it->get()->get_id());
            }
        }

        value = g_variant_new("(au)", &builder);
        g_dbus_method_invocation_return_value(invocation, value);

        return;
    }
    /*************** Output Properties ****************/
    else
    if (g_strcmp0(method_name, "query_output_name") == 0)
    {
        uint output_id;
        gchar* response = "nullptr";
        g_variant_get(parameters, "(u)", &output_id);
        wf::output_t* wf_output = get_output_from_output_id(output_id);

        if (wf_output != nullptr) {
            response = g_strdup_printf(wf_output->to_string().c_str());
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        if (wf_output != nullptr) {
            g_free(response);
        }

        return;
    }
    else
    if (g_strcmp0(method_name, "query_output_manufacturer") == 0)
    {
        uint output_id;
        gchar* response = "nullptr";
        wf::output_t* output;
        wlr_output* wlr_output = nullptr;

        g_variant_get(parameters, "(u)", &output_id);
        output = get_output_from_output_id(output_id);
        if (output) {
            wlr_output = output->handle;

            if (wlr_output != nullptr) {
                response = g_strdup_printf(wlr_output->make);
            }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        if (wlr_output != nullptr) {
            g_free(response);
        }

        return;
    }
    else
    if (g_strcmp0(method_name, "query_output_model") == 0)
    {
        uint output_id;
        gchar* response = "nullptr";
        wf::output_t* output;
        wlr_output* wlr_output = nullptr;

        g_variant_get(parameters, "(u)", &output_id);
        output = get_output_from_output_id(output_id);
        if (output) {
            wlr_output = output->handle;

            if (wlr_output != nullptr) {
                response = g_strdup_printf(wlr_output->model);
            }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        if (wlr_output != nullptr) {
            g_free(response);
        }

        return;
    }
    else
    if (g_strcmp0(method_name, "query_output_serial") == 0)
    {
        uint output_id;
        gchar* response = "nullptr";
        wf::output_t* wf_output;
        wlr_output* wlr_output = nullptr;

        g_variant_get(parameters, "(u)", &output_id);
        wf_output = get_output_from_output_id(output_id);
        wlr_output = wf_output->handle;

        if (wlr_output != nullptr) {
            response = g_strdup_printf(wlr_output->serial);
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        if (wlr_output != nullptr) {
            g_free(response);
        }

        return;
    }
    else
    if (g_strcmp0(method_name, "query_output_workspace") == 0)
    {
        uint output_id;
        uint horizontal_workspace = 0;
        uint vertical_workspace = 0;
        wf::output_t* wf_output;
        wf::point_t ws;

        g_variant_get(parameters, "(u)", &output_id);
        wf_output = get_output_from_output_id(output_id);
        if (wf_output) {
            ws = wf_output->workspace->get_current_workspace();
            horizontal_workspace = ws.x;
            vertical_workspace = ws.y;
        }

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(uu)", horizontal_workspace, vertical_workspace));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_workspace_grid_size") == 0)
    {
        wf::dimensions_t workspaces;
        workspaces = core.get_active_output()->workspace->get_workspace_grid_size();

        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(ii)", workspaces.width, workspaces.height));

        return;
    }
    /*************** View Properties ****************/
    else
    if (g_strcmp0(method_name, "query_view_above_view") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);

        wayfire_view view = get_view_from_view_id(view_id);
        wf::output_t* output;
        int view_above = -1;
        std::vector<wayfire_view> workspace_views;

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(i)", view_above));

            return;
        }

        while (view->parent)
        {
            view = view->parent;
        }

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(i)", view_above));

            return;
        }

        output = view->get_output();
        if (!output) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(i)", view_above));

            return;
        }

        workspace_views =
            output->workspace->get_views_in_layer(wf::MIDDLE_LAYERS);

        for (int i = 0; i < workspace_views.size() - 1; i++)
        {
            wayfire_view v = workspace_views[i];
            if (!check_view_toplevel(v)) {
                continue;
            }

            if (v == view) {
                if (i != 0) {
                    if (check_view_toplevel(workspace_views[i - 1])) {
                        view_above = workspace_views[i - 1]->get_id();
                    }

                    break;
                }
            }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(i)", view_above));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_below_view") == 0)
    {
        uint view_id;
        g_variant_get(parameters, "(u)", &view_id);

        wayfire_view view = get_view_from_view_id(view_id);
        wf::output_t* output;
        int view_below = -1;
        std::vector<wayfire_view> workspace_views;

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(i)", view_below));

            return;
        }

        while (view->parent)
        {
            view = view->parent;
        }

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(i)", view_below));

            return;
        }

        output = view->get_output();
        if (!output) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(i)", view_below));

            return;
        }

        workspace_views =
            output->workspace->get_views_in_layer(wf::MIDDLE_LAYERS);

        for (int i = 0; i < workspace_views.size() - 1; i++)
        {
            wayfire_view v = workspace_views[i];
            if (!check_view_toplevel(v)) {
                continue;
            }

            if (v == view) {
                if (i != workspace_views.size() - 1) {
                    if (check_view_toplevel(workspace_views[i + 1])) {
                        view_below = workspace_views[i + 1]->get_id();
                    }

                    break;
                }
            }

            // if (view_below != -1)
            // {
            // g_warning("Below %s  is  %s", view->get_title().c_str(),
            // get_view_from_view_id(view_below)->get_title().c_str());
            // }
            // else
            // {
            // g_warning("No view below %s", view->get_title().c_str());
            // }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(i)", view_below));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_app_id") == 0)
    {
        uint view_id;
        gchar* response = "nullptr";
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(s)", response));

            return;
        }

        response = g_strdup(view->get_app_id().c_str());
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        g_free(response);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_app_id_gtk_shell") == 0)
    {
        uint view_id;
        gchar* response = "nullptr";
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(s)", response));

            return;
        }

        response = g_strdup(get_gtk_shell_app_id(view).c_str());
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        g_free(response);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_app_id_xwayland_net_wm_name") ==
        0)
    {
        uint view_id;
        gchar* response = "nullptr";
        wayfire_view view;
        bool free_response = false;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            auto wlr_surf = view->get_wlr_surface();
            if (!wlr_surf) {
                g_dbus_method_invocation_return_value(invocation,
                                                      g_variant_new("(s)", response));

                return;
            }

            if (wlr_surface_is_xwayland_surface(wlr_surf)) {
                struct wlr_xwayland_surface* xsurf;
                xsurf = wlr_xwayland_surface_from_wlr_surface(wlr_surf);
                if (!xsurf) {
                    g_dbus_method_invocation_return_value(invocation,
                                                          g_variant_new("(s)", response));

                    return;
                }

                g_assert(xsurf != NULL);
                std::string wm_name_app_id = nonull(xsurf->instance);
                response = g_strdup_printf(wm_name_app_id.c_str());
                free_response = true;
            }
        }

        if (free_response) {
            g_free(response);
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_title") == 0)
    {
        uint view_id;
        gchar* response = "nullptr";
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!check_view_toplevel(view)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(s)", response));

            return;
        }

        response = g_strdup_printf(view->get_title().c_str());
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", response));
        g_free(response);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_attention") == 0)
    {
        uint view_id;
        bool attention = false;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);
        if (!check_view_toplevel(view)) {
            return;
        }

        if (view->has_data("view-demands-attention")) {
            attention = true;
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", attention));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_xwayland_display") == 0)
    {
        const char* xdisplay = core.get_xwayland_display().c_str();

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", xdisplay));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_xwayland_wid") == 0)
    {
        uint view_id;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!view) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(u)", 0));

            return;
        }

        if (xwayland_enabled == 1) {
            auto main_wlr_surface = view->get_main_surface()->get_wlr_surface();
            if (!main_wlr_surface) {
                g_dbus_method_invocation_return_value(invocation,
                                                      g_variant_new("(u)", 0));

                return;
            }

            if (wlr_surface_is_xwayland_surface(main_wlr_surface)) {
#ifdef DBUS_PLUGIN_DEBUG
                LOG(wf::log::LOG_LEVEL_DEBUG, "xwayland is the surface type.");
#endif
                struct wlr_xwayland_surface* main_xsurf;
                main_xsurf = wlr_xwayland_surface_from_wlr_surface(main_wlr_surface);
                g_dbus_method_invocation_return_value(
                    invocation, g_variant_new("(u)", main_xsurf->window_id));

                return;
            }
        }

        g_dbus_method_invocation_return_value(invocation, g_variant_new("(u)", 0));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_xwayland_atom_cardinal") == 0)
    {
        uint view_id;
        uint atom_value_cardinal = 0;
        gchar* atom_name;
        wayfire_view view;

        g_variant_get(parameters, "(us)", &view_id, &atom_name);
        view = get_view_from_view_id(view_id);

        if (!view) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(u)", 0));

            return;
        }

        auto main_wlr_surface = view->get_main_surface()->get_wlr_surface();
        if (!main_wlr_surface) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(u)", 0));

            return;
        }

        if ((xwayland_enabled != 1) ||
            !wlr_surface_is_xwayland_surface(main_wlr_surface)) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(u)", 0));

            return;
        }

        struct wlr_xwayland_surface* main_xsurf;
        main_xsurf = wlr_xwayland_surface_from_wlr_surface(main_wlr_surface);

        const char* xdisplay = core.get_xwayland_display().c_str();
        int screen;
        xcb_connection_t* conn = xcb_connect(xdisplay, &screen);
        xcb_intern_atom_cookie_t atom_cookie;
        xcb_atom_t atom;
        xcb_intern_atom_reply_t* reply;
        atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
        reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
        if (reply != NULL) {
            atom = reply->atom;
            free(reply);
        }
        else
        {
#ifdef DBUS_PLUGIN_DEBUG
            LOG(wf::log::LOG_LEVEL_DEBUG, "reply for querying the atom is empty.");
#endif
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(u)", atom_value_cardinal));

            return;
        }

        xcb_get_property_cookie_t reply_cookie;
        xcb_get_property_reply_t* reply_value;
        reply_cookie = xcb_get_property(conn, 0, main_xsurf->window_id, atom,
                                        XCB_ATOM_ANY, 0, 2048);
        reply_value = xcb_get_property_reply(conn, reply_cookie, NULL);
        xcb_disconnect(conn);

        if (reply_value->type == XCB_ATOM_CARDINAL) {
            uint* uvalue = (uint*)xcb_get_property_value(reply_value);
            atom_value_cardinal = *uvalue;
#ifdef DBUS_PLUGIN_DEBUG
            LOG(wf::log::LOG_LEVEL_DEBUG, "value to uint.", atom_value_cardinal);
#endif
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "requested value is not a cardinal");
        }
#endif

        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(u)", atom_value_cardinal));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_xwayland_atom_string") == 0)
    {
        uint view_id;
        gchar* atom_name;
        gchar* atom_value_string = "No atom value received.";

        g_variant_get(parameters, "(us)", &view_id, &atom_name);

        wayfire_view view = get_view_from_view_id(view_id);

        if (!view) {
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(s)", "View not found."));

            return;
        }

        auto main_wlr_surface = view->get_main_surface()->get_wlr_surface();

        if (!main_wlr_surface) {
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(s)", "main_wlr_surface not found."));

            return;
        }

        if ((xwayland_enabled != 1) ||
            !wlr_surface_is_xwayland_surface(main_wlr_surface)) {
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(s)", "Not an xwayland surface."));

            return;
        }

        struct wlr_xwayland_surface* main_xsurf;

        main_xsurf = wlr_xwayland_surface_from_wlr_surface(main_wlr_surface);

        const char* xdisplay = core.get_xwayland_display().c_str();
        int screen;
        xcb_connection_t* conn = xcb_connect(xdisplay, &screen);
        xcb_intern_atom_cookie_t atom_cookie;
        xcb_atom_t atom;
        xcb_intern_atom_reply_t* reply;
        atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
        reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
        if (reply != NULL) {
            atom = reply->atom;
            free(reply);
        }
        else
        {
            g_dbus_method_invocation_return_value(
                invocation,
                g_variant_new("(s)", "reply for querying the atom is empty."));

            return;
        }

        xcb_get_property_cookie_t reply_cookie = xcb_get_property(
            conn, 0, main_xsurf->window_id, atom, XCB_ATOM_ANY, 0, 2048);
        xcb_get_property_reply_t* reply_value =
            xcb_get_property_reply(conn, reply_cookie, NULL);

        char* value = static_cast<char*> (xcb_get_property_value(reply_value));

        xcb_disconnect(conn);

        if (reply_value->type != XCB_ATOM_CARDINAL) {
            atom_value_string = value;
#ifdef DBUS_PLUGIN_DEBUG
            LOG(wf::log::LOG_LEVEL_DEBUG, "value to char.", atom_value_string);
#endif
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(s)", atom_value_string));

            return;
        }
        else
        {
            g_dbus_method_invocation_return_value(
                invocation,
                g_variant_new("(s)", "XCB_ATOM_CARDINAL type requested."));

            return;
        }
    }
    else
    if (g_strcmp0(method_name, "query_view_credentials") == 0)
    {
        uint view_id;
        pid_t pid = 0;
        uid_t uid = 0;
        gid_t gid = 0;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!view) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(iuu)", 0, 0, 0));

            return;
        }

        if (xwayland_enabled == 1) {
            auto main_surface = view->get_main_surface()->get_wlr_surface();
            if (!main_surface) {
                g_dbus_method_invocation_return_value(invocation,
                                                      g_variant_new("(iuu)", 0, 0, 0));

                return;
            }

            if (wlr_surface_is_xwayland_surface(main_surface)) {
                struct wlr_xwayland_surface* main_xsurf;
                xcb_res_client_id_spec_t spec = {0};
                xcb_generic_error_t* err = NULL;
                xcb_res_query_client_ids_cookie_t cookie;
                xcb_res_query_client_ids_reply_t* reply;
                int screen;

                const char* xdisplay = core.get_xwayland_display().c_str();
                xcb_connection_t* conn = xcb_connect(xdisplay, &screen);

                main_xsurf = wlr_xwayland_surface_from_wlr_surface(main_surface);
                spec.client = main_xsurf->window_id;
                spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;
                cookie = xcb_res_query_client_ids(conn, 1, &spec);
                reply = xcb_res_query_client_ids_reply(conn, cookie, &err);

                if (reply == NULL) {
#ifdef DBUS_PLUGIN_DEBUG
                    LOG(wf::log::LOG_LEVEL_DEBUG,
                        "could not get pid from xserver, empty reply");
#endif
                }
                else
                {
                    xcb_res_client_id_value_iterator_t it;
                    it = xcb_res_query_client_ids_ids_iterator(reply);
                    for (; it.rem; xcb_res_client_id_value_next(&it))
                    {
                        spec = it.data->spec;
                        if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
                            pid = *xcb_res_client_id_value_value(it.data);
                            break;
                        }
                    }

                    free(reply);
                }

                xcb_disconnect(conn);

                if (pid != 0) {
                    LOG(wf::log::LOG_LEVEL_DEBUG,
                        "returning xwayland window credentials.");
                    g_dbus_method_invocation_return_value(
                        invocation, g_variant_new("(iuu)", pid, uid, gid));

                    return;
                }
            }
        }

#ifdef DBUS_PLUGIN_DEBUG
        LOG(wf::log::LOG_LEVEL_DEBUG, "returning standard credentials.");
#endif
        wl_client_get_credentials(view->get_client(), &pid, &uid, &gid);
        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(iuu)", pid, uid, gid));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_above") == 0)
    {
        wayfire_view view;
        uint view_id;
        bool above;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);
        above = false;

        if (view) {
            if (view->has_data("wm-actions-above")) {
                above = true;
            }
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_above no view");
        }
#endif
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", above));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_maximized") == 0)
    {
        wayfire_view view;
        uint view_id;
        bool response = false;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            response = (view->tiled_edges == wf::TILED_EDGES_ALL);
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_maximized no view");
        }
#endif
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", response));

        return;
    }

    else
    if (g_strcmp0(method_name, "query_view_active") == 0)
    {
        wayfire_view view;
        uint view_id;
        bool response = false;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            response = view->activated;
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_active no view");
        }
#endif
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", response));

        return;
    }

    else
    if (g_strcmp0(method_name, "query_view_minimized") == 0)
    {
        wayfire_view view;
        uint view_id;
        bool response = false;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            response = view->minimized;
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_minimized no view");
        }
#endif
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", response));

        return;
    }

    else
    if (g_strcmp0(method_name, "query_view_fullscreen") == 0)
    {
        uint view_id;
        bool response = false;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            response = view->fullscreen;
        }

#ifdef DBUS_PLUGIN_DEBUG
        else
        {
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_fullscreen no view");
        }
#endif
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(b)", response));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_output") == 0)
    {
        uint view_id;
        uint output_id = 0;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            if (view->get_output()) {
                output_id = view->get_output()->get_id();
            }

#ifdef DBUS_PLUGIN_DEBUG

            else
            {
                g_warning("No output for view.");
            }
#endif
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(u)", output_id));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_workspaces") == 0)
    {
#ifdef DBUS_PLUGIN_DEBUG
        LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_workspaces ");
#endif

        uint view_id;
        double area;
        GVariantBuilder builder;
        GVariant* value;
        wf::geometry_t workspace_relative_geometry;
        wlr_box view_relative_geometry;
        wf::geometry_t intersection;
        wf::dimensions_t workspaces;
        wf::output_t* output;

        g_variant_get(parameters, "(u)", &view_id);
        wayfire_view view = get_view_from_view_id(view_id);

        if (!check_view_toplevel(view)) {
#ifdef DBUS_PLUGIN_DEBUG

            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_workspaces no view");
#endif
            g_dbus_method_invocation_return_value(invocation, NULL);

            return;
        }

        workspaces = core.get_active_output()->workspace->get_workspace_grid_size();

        view_relative_geometry = view->get_bounding_box();
        output = view->get_output();

        g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ii)"));

        for (int horizontal_workspace = 0; horizontal_workspace < workspaces.width;
             horizontal_workspace++)
        {
            for (int vertical_workspace = 0; vertical_workspace < workspaces.height;
                 vertical_workspace++)
            {
                wf::point_t ws = {horizontal_workspace, vertical_workspace};
                if (output->workspace->view_visible_on(view, ws)) {
                    workspace_relative_geometry = output->render->get_ws_box(ws);
                    intersection = wf::geometry_intersection(view_relative_geometry,
                                                             workspace_relative_geometry);
                    area = 1.0 * intersection.width * intersection.height;
                    area /= 1.0 * view_relative_geometry.width *
                        view_relative_geometry.height;

                    if (area > 0.1) {
                        g_variant_builder_add(&builder, "(ii)", horizontal_workspace,
                                              vertical_workspace);
                    }
                }
            }
        }

        value = g_variant_new("(a(ii))", &builder);
        g_dbus_method_invocation_return_value(invocation, value);

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_group_leader") == 0)
    {
        uint view_id;
        uint group_leader_view_id;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        group_leader_view_id = view_id;
        view = get_view_from_view_id(view_id);

        if (view) {
            while (view->parent)
            {
                view = view->parent;
            }

            group_leader_view_id = view->get_id();
#ifdef DBUS_PLUGIN_DEBUG
            LOG(wf::log::LOG_LEVEL_DEBUG, "query_view_group_leader found returning");
#endif
        }

        g_dbus_method_invocation_return_value(
            invocation, g_variant_new("(u)", group_leader_view_id));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_role") == 0)
    {
        uint view_id;
        uint response = 0;
        bool is_modal_dialog;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (view) {
            if (view->is_mapped()) {
                is_modal_dialog = view->has_data("gtk-shell-modal");

                if ((view->role == wf::VIEW_ROLE_TOPLEVEL) && !is_modal_dialog) {
                    response = 1;
                }

                else
                if (view->role == wf::VIEW_ROLE_DESKTOP_ENVIRONMENT)
                {
                    response = 2;
                }

                else
                if (view->role == wf::VIEW_ROLE_UNMANAGED)
                {
                    response = 3;
                }
            }
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(u)", response));

        return;
    }
    else
    if (g_strcmp0(method_name, "query_view_test_data") == 0)
    {
        uint view_id;
        wayfire_view view;

        g_variant_get(parameters, "(u)", &view_id);
        view = get_view_from_view_id(view_id);

        if (!check_view_toplevel(view))
        {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(uu)", 0, 0));

            return;
        }

        auto wlr_surf = view->get_wlr_surface();
        if (!wlr_surf) {
            g_dbus_method_invocation_return_value(invocation,
                                                  g_variant_new("(uu)", 0, 0));

            return;
        }

        if (wlr_surface_is_xwayland_surface(wlr_surf)) {
            struct wlr_xwayland_surface* xsurf;
            xsurf = wlr_xwayland_surface_from_wlr_surface(wlr_surf);
            g_dbus_method_invocation_return_value(
                invocation, g_variant_new("(uu)", xsurf->width, xsurf->height));
        }

        return;
    }

    /*************** Other Actions ****************/
    else
    if (g_strcmp0(method_name, "inhibit_output_start") == 0)
    {
        g_variant_unref(parameters);
    }
    else
    if (g_strcmp0(method_name, "inhibit_output_stop") == 0)
    {
        g_variant_unref(parameters);
    }
    else
    if (g_strcmp0(method_name, "trigger_show_desktop") == 0)
    {
        g_variant_unref(parameters);
    }
    else
    if (g_strcmp0(method_name, "trigger_show_overview") == 0)
    {
        g_variant_unref(parameters);
    }
}

static GVariant*
handle_get_property (GDBusConnection* connection,
                     const gchar* sender,
                     const gchar* object_path,
                     const gchar* interface_name,
                     const gchar* property_name, GError** error,
                     gpointer user_data)
{
    // returning nullptr would crash compositor
    GVariant* ret = g_variant_new_string("nullptr");

    // kept as an example
    // if (g_strcmp0(property_name, "view_vector_ids") == 0)
    // {
    // std::vector<nonstd::observer_ptr<wf::view_interface_t>> view_vector =
    // core.get_all_views();

    // GVariantBuilder builder;
    // g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));
    // for (auto it = begin(view_vector); it != end(view_vector); ++it)
    // g_variant_builder_add(&builder, "u", it->get()->get_id());
    // ret = g_variant_builder_end(&builder);
    // }

    /* unused */
    return ret;
}

static gboolean
handle_set_property (GDBusConnection* connection,
                     const gchar* sender,
                     const gchar* object_path,
                     const gchar* interface_name,
                     const gchar* property_name, GVariant* value,
                     GError** error, gpointer user_data)
{
    /* unused */
    return false;
}

static const GDBusInterfaceVTable interface_vtable = {
    handle_method_call, handle_get_property, handle_set_property, {0}
};

static gboolean
bus_emit_signal (gchar* signal_name, GVariant* signal_data)
{
    GError* local_error = NULL;
    if (!dbus_connection) {
        if (signal_data != nullptr) {
            g_variant_unref(signal_data);
        }

        return true;
    }

    g_dbus_connection_emit_signal(
        dbus_connection, nullptr, "/org/wayland/compositor",
        "org.wayland.compositor", signal_name, signal_data, &local_error);
    g_assert_no_error(local_error);

    if (signal_data != nullptr) {
        g_variant_unref(signal_data);
    }

    return true;
}

static void
on_bus_acquired (GDBusConnection* connection, const gchar* name,
                 gpointer user_data)
{
    uint registration_id;

    dbus_connection = connection;
    registration_id = g_dbus_connection_register_object(
        connection, "/org/wayland/compositor", introspection_data->interfaces[0],
        &interface_vtable, nullptr, nullptr, nullptr);
#ifdef DBUS_PLUGIN_DEBUG
    LOG(wf::log::LOG_LEVEL_DEBUG, "Acquired the Bus");
#endif
}

static void
on_name_acquired (GDBusConnection* connection, const gchar* name,
                  gpointer user_data)
{
#ifdef DBUS_PLUGIN_DEBUG
    LOG(wf::log::LOG_LEVEL_DEBUG,
        "Acquired the name " + std::string(name) + "on the session bus\n");
#endif
}

static void
on_name_lost (GDBusConnection* connection, const gchar* name,
              gpointer user_data)
{
#ifdef DBUS_PLUGIN_DEBUG
    LOG(wf::log::LOG_LEVEL_DEBUG,
        "Lost the name " + std::string(name) + "on the session bus");
#endif
}

static void
acquire_bus ()
{
    GBusNameOwnerFlags flags;
// flags = G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE;
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, nullptr);

    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, "org.wayland.compositor", flags,
                              on_bus_acquired, on_name_acquired, on_name_lost,
                              nullptr, nullptr);
}
