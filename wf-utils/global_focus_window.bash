# shellcheck shell=bash

function global_focus_window {
	: 'Move and focus a window to the current workspace'
	: 'Program name'
	local app="$1"
	: 'Window title'
	local titlish="$2"

	local window
	local is_active
	local x
	local y
	window=$(find_window_by "$app" "$titlish")
	is_active=$(_wf-msg is_window_active "$window")

	if [[ "$is_active" == "false" ]]; then
		_debug "window $window is not active. focussing ..."
		current_workspace="$(_wf-msg get_current_workspace)"
		x=$(_get_x "$current_workspace")
		y=$(_get_y "$current_workspace")
		_wf-msg move_window_to_workspace "$window" "$x" "$y"
		_wf-msg focus_window "$window"
		hide_window_when_done "$window"
	else
		_debug "window $window is active. hiding ..."
		_wf-msg minimize_window "$window"
	fi
}
