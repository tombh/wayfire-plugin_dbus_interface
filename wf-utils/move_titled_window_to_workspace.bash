# shellcheck shell=bash

function move_titled_window_to_workspace {
	local args
	declare -A args=(
		[summary]="Find a window by its app and title then move it to a workspace"
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]="Regex to match title of window"
		[2:x]="x-coord of destination workspace"
		[3:y]="y-coord of destination workspace"
		[--timeout]="How many seconds to wait if using a waiting flag, default: $default_timeout"
		[--on-creation:flag]="Whether to first wait for the window to be created"
		[--wait-for-title:flag]="Whether to wait for the title to change"
	)
	BAPt_parse_arguments args "$@"

	local window timeout="${args[timeout]:-$DEFAULT_TIMEOUT}"
	window=$(find_titled_window "${args[app]}" "${args[titlish]}" --allow-absence)

	if [[ -z $window && -n ${args[wait-for-title]} ]]; then
		window=$(
			wait_for_window_title_change \
				"${args[app]}" \
				"${args[titlish]}" \
				--timeout "$timeout"
		)
	fi

	if [[ -z $window && -n ${args[on-creation]} ]]; then
		window="$(
			wait_for_window_creation \
				"${args[app]}" \
				"${args[titlish]}" \
				--timeout "$timeout"
		)"
	fi

	if [[ -n $window ]]; then
		_wf-msg move_window_to_workspace "$window" "${args[x]}" "${args[y]}"
	else
		_error "Window not found"
	fi

}
