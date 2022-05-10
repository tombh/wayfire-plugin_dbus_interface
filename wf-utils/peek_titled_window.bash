# shellcheck shell=bash

function peek_titled_window {
	local args
	declare -A args=(
		[summary]="Find a window and bring it to the current workspace"
		[details]="$(cat <<-EOM
			Note that the window will be automatically minimised once
			it's unfocussed. So this functionality is for when you want
			something like a "popup" window, say a terminal to enter a
			quick command.

			Also, calling this function when the window is already
			focussed will minimise it.
		EOM
		)"
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]="Regex to match title of window"
	)
	BAPt_parse_arguments args "$@"

	local window is_active
	window=$(find_titled_window "${args[app]}" "${args[titlish]}")
	move_window_to_current_workspace "$window" &
	is_active=$(_wf-msg is_window_active "$window")

	if [[ "$is_active" == "false" ]]; then
		_debug "window $window is not active. focussing ..."
		_wf-msg unminimize_window "$window"
		minimize_window_on_unfocus "$window"
	else
		_debug "window $window is active. hiding ..."
		_wf-msg minimize_window "$window"
	fi
}
