# shellcheck shell=bash

function move_titled_window_to_workspace {
	local args
	declare -A args=(
		[summary]="Find a window by its app and title then move it to a workspace"
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]="Regex to match title of window"
		[2:x]="x-coord of destination workspace"
		[3:y]="y-coord of destination workspace"
		[--wait:flag]="Whether to wait for the title to change"
	)
	__BAPt_parse_arguments args "$@"

	window=$(find_titled_window "${args[app]}" "${args[titlish]}")
	_wf-msg move_window_to_workspace "$window" "${args[x]}" "${args[y]}"
}
