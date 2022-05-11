# `wf-msg` and `wf-utils` for [Wayfire](https://github.com/WayfireWM/wayfire)
This allows you to have very fine-grained control of Wayfire from the CLI. At its most basic it can manipulate windows eg; move, minimize, resize and close them. However it can also query current properties, or subscribe to changes, about; windows, workspaces and outputs. Together these tools can be used to create some powerful functionality.

### `wf-msg`
Inspired by SwayWM's [swaymsg](https://github.com/swaywm/sway/blob/master/swaymsg/swaymsg.1.scd). Talks to the D-Bus plugin.

```
Usage: wf-msg [SUBCOMMAND] [--help]

Control Wayfire from the CLI

Subcommands:
  focus_window             Focus the given window
  get_all_window_ids       Get the IDs of all the current windows
  get_all_windows          Get the full details of all current windows
  get_current_output       Get the ID of the current output (usually a physical monitor)
  get_current_workspace    Get the X,Y coords of the current workspace on the current output
  get_window_app           Get the application that launched the given window
  get_window_title         Get the title of the given window
  get_window_workspace     Get the X,Y coords of the given window
  is_window_active         Is the given window ID active? (I think that means focussed)
  maximize_window          Maximize the given window
  minimize_window          Minimize the given window
  move_window_to_workspace Move the given window to the given workspace
  unfocus_window           Return window to focus state before previous focus
  unmaximize_window        Reize window to size before previous maximization
  unminimize_window        Resize window to size before previous minimization
  wf-call                  Make a call to Wayfire
  wf-dbus-introspect       Returns XML of all the available Wayfire D-Bus methods and signals

Options:
  --help                   Show this help
```

### `wf-utils`
```
Usage: wf-utils [SUBCOMMAND] [--help]

Collection of general utilities for controling Wayfire

Subcommands:
  find_titled_window               Find a window by searching for its name and title. Returns window ID
  minimize_window_on_unfocus       Minimise a window once it becomes unfoccused
  move_titled_window_to_workspace  Find a window by its app and title then move it to a workspace
  move_window_to_current_workspace Move window to current workspace
  peek_titled_window               Find a window and bring it to the current workspace
  wait_for_window_creation         Wait for a window with the given title to come into existence
  wait_for_window_title_change     Wait until a window changes its title to that provided

Options:
  --help                           Show this help
```

I'd hope that `wf-msg` and `wf-utils` could find a home in either the [D-Bus plugin](https://github.com/damianatorrpm/wayfire-plugin_dbus_interface) repo or even Wayfire's own [wayfire-plugins-extra](https://github.com/WayfireWM/wayfire-plugins-extra) repo

### Examples

Hotkey for specfic window:
```ini
# Unconditionally focus a Firefox window with a translation tab.
# It doesn't create the Firefox window, just focusses a pre-existing one.
binding_toggle_translator = <super> KEY_T
command_toggle_translator = wf-utils peek_titled_window firefox 'DeepL Translate'
```

Autostart windows to specific layout:
```sh
# When a browser window contains multiple tabs you need an extension to insert a consistent,
# searchable string token in the title in order for `wf-utils` to find it. For Firefox there is:
# https://addons.mozilla.org/en-US/firefox/addon/window-titler
# The `--wait-for-title` flag is needed because it can take a few moments for Firefox to update
# its titles.
wf-utils move_titled_window_to_workspace firefox 'Projects' 0 0 --wait-for-title --timeout 60 &
wf-utils move_titled_window_to_workspace firefox 'Email etc' 0 1 --wait-for-title --timeout 60 &
wf-utils move_titled_window_to_workspace firefox 'System' 0 2 --wait-for-title --timeout 60 &
wf-utils move_titled_window_to_workspace firefox 'Entertainment' 2 1 --wait-for-title --timeout 60 &
wf-utils move_titled_window_to_workspace firefox 'Remote' 2 2 --wait-for-title --timeout 60 &
firefox &
disown
firefox --private-window https://www.deepl.com/translator &
disown
```




## Requirements
* @soppelmann's fork (it fixes damianatorrpm#46) of the [Wayfire D-Bus plugin](https://github.com/soppelmann/wayfire-plugin_dbus_interface)
* [wayfire-plugins-extra](https://github.com/WayfireWM/wayfire-plugins-extra)
* `wf-utils` requires [`jq`](https://stedolan.github.io/jq/download)


## Install
From project root:
* `ln -s $(pwd)/wf-msg/wf-msg /usr/local/bin/wf-msg`
* `ln -s $(pwd)/wf-utils/wf-utils /usr/local/bin/wf-utils`

