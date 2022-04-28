# shellcheck shell=bash

function minimize_window_on_unfocus {
	local args
	declare -A args=(
		[summary]="Minimise a window once it becomes unfoccused"
		[0:window_id]="Window ID. See something like \`find_titled_window\`"
	)
	__BAPt_parse_arguments args "$@"

	# TODO: use D-Bus subscribes
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
	_debug "jq query: $(echo "$query" | _trim)"

	match=$(_wf-msg get_all_windows --use-cache | _jq "$query" | head -n1)
	if ! _double_check_window_title "$match" "${args[titlish]}"; then
		match=""
	fi

	if [[ -z $match ]]; then
		match=$(_wf-msg get_all_windows | _jq "$query" | head -n1)
	fi

	if [[ -z $match ]]; then
		_error "Couldn't find a window matching '${args[app]}', '${args[titlish]}'"
	fi

	_debug "matched '${args[app]}', '${args[titlish]}': window $match"
	echo "$match"
}

function _double_check_window_title {
	local window_id=$1 titlish=$2 actual_title

	actual_title=$(wf-msg get_window_title "$window_id")
	_debug "$titlish =~ $actual_title"
	if [[ $actual_title =~ $titlish	]]; then
		return 0
	else
		return 1
	fi
}

# TODO: This is slow, ~150ms, because as well as the 2 calls in this function it also needs
# to get the current output
function move_window_to_current_workspace {
	local args
	declare -A args=(
		[summary]="Move window to current workspace"
		[0:window_id]="Window ID. See something like \`find_titled_window\`"
	)
	__BAPt_parse_arguments args "$@"

	local current_workspace x y
	current_workspace="$(_wf-msg get_current_workspace)"
	x=$(_get_x "$current_workspace")
	y=$(_get_y "$current_workspace")
	_wf-msg move_window_to_workspace "${args[window_id]}" "$x" "$y"
}
