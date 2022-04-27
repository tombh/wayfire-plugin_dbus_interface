# shellcheck shell=bash

function minimize_window_on_unfocus {
	local args
	# shellcheck disable=2016
	declare -A args=(
		[summary]="Minimise a window once it becomes unfoccused"
		[0:window_id]='Window ID. See something like `find_titled_window`'
	)
	__BAPt_parse_arguments args "$@"

	while [[ "$(_wf-msg is_window_active "${args[window_id]}")" == "false" ]]; do
		sleep 0.5
	done
	while [[ "$(_wf-msg is_window_active "${args[window_id]}")" == "true" ]]; do
		sleep 0.5
	done
	_wf-msg minimize_window "${args[window_id]}"
}

function find_titled_window {
	local args
	declare -A args=(
		[summary]='Find a window by searching for its name and title. Returns window ID'
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]='Regex to match title of window'
	)
	__BAPt_parse_arguments args "$@"

	local query=".[] |
		select(
			(.app==\"${args[app]}\") and (.title|test(\"${args[titlish]}\"))
		) | .id
	"

	_debug "query: $(echo "$query" | _trim)"
	match=$(_wf-msg get_all_windows | _jq "$query" | head -n1)

	if [[ -z $match ]]; then
		_error "Couldn't find a window matching '${args[app]}', '${args[titlish]}'"
	fi

	_debug "matched '${args[app]}', '${args[titlish]}': window $match"
	echo "$match"
}
