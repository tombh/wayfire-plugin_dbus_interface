# shellcheck shell=bash

function peek_titled_window {
	local args
	declare -A args=(
		[summary]="Find a window and bring it to the current workspace"
		[details]="$(cat <<-EOM
			Note that the window will be automatically minimised once
			it's unfoccused. So this functionality is for when you want
			something like a "popup" window, say a terminal to enter a
			quick command.

			Also, calling this function when the window is already
			focussed will minimise it.
		EOM
		)"
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]="Regex to match title of window"
	)
	__BAPt_parse_arguments args "$@"

	local window is_active x y

	window=$(find_titled_window "${args[app]}" "${args[titlish]}")
	is_active=$(_wf-msg is_window_active "$window")

	if [[ "$is_active" == "false" ]]; then
		_debug "window $window is not active. focussing ..."
		current_workspace="$(_wf-msg get_current_workspace)"
		x=$(_get_x "$current_workspace")
		y=$(_get_y "$current_workspace")
		_wf-msg move_window_to_workspace "$window" "$x" "$y"
		_wf-msg focus_window "$window"
		minimize_window_on_unfocus "$window"
	else
		_debug "window $window is active. hiding ..."
		_wf-msg minimize_window "$window"
	fi
}
