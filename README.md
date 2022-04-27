# D-Bus, `wf-msg` and `wf-utils` for [Wayfire](https://github.com/WayfireWM/wayfire)
This allows you to have very fine-grained control of Wayfire from the CLI. At its most basic it can manipulate windows eg; move, minimize, resize and close them. However it can also query current properties, or subscribe to changes, about; windows, workspaces and outputs. Together these tools can be used to create some powerful functionality.


### D-Bus
The [D-Bus plugin](https://github.com/damianatorrpm/wayfire-plugin_dbus_interface) is here for now because at the time it wasn't building against current Wayfire and needs an unmerged PR to get it working. I'll remove the D-Bus code once its working upstream again.

### `wf-msg`
Inspired by SwayWM's [swaymsg](https://github.com/swaywm/sway/blob/master/swaymsg/swaymsg.1.scd). Talks to the D-Bus plugin.

### `wf-utils`
A collection of ready-made utilities that make use of `wf-msg`


I'd hope that `wf-msg` and `wf-utils` could find a home in either the [D-Bus plugin](https://github.com/damianatorrpm/wayfire-plugin_dbus_interface) repo or even Wayfire's own [wayfire-plugins-extra](https://github.com/WayfireWM/wayfire-plugins-extra) repo


## Requirements

	* [wayfire-plugins-extra](https://github.com/WayfireWM/wayfire-plugins-extra)
  * `jq` https://stedolan.github.io/jq/download


## Install

  * AUR package TBD
	* `make && make install`


## `wf-utils` usage
TBC

## `wf-msg` usage
TBC
