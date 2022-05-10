# shellcheck shell=bash

function minimize_window_on_unfocus {
	local args
	declare -A args=(
		[summary]="Minimise a window once it becomes unfoccused"
		[0:window_id]="Window ID. See something like \`find_titled_window\`"
	)
	BAPt_parse_arguments args "$@"

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
		[--allow-absence:flag]="Don't error if window not found"
	)
	BAPt_parse_arguments args "$@"

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

	if [[ -z $match && -z ${args[allow-absence]} ]]; then
		_error "Couldn't find a window matching '${args[app]}', '${args[titlish]}'"
	fi

	_debug "matched '${args[app]}', '${args[titlish]}': window $match"
	echo "$match"
}

function _double_check_window_title {
	local window_id=$1 titlish=$2 actual_title
	if [[ -z $window_id	]]; then
		return 1
	fi

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
	BAPt_parse_arguments args "$@"

	local current_workspace x y
	current_workspace="$(_wf-msg get_current_workspace)"
	x=$(_get_x "$current_workspace")
	y=$(_get_y "$current_workspace")
	_wf-msg move_window_to_workspace "${args[window_id]}" "$x" "$y"
}

function wait_for_window_title_change {
	local args
	declare -A args=(
		[summary]='Wait until a window changes its title to that provided'
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]='Regex to match title of window'
		[--timeout]="How long to wait until quitting, default 15 seconds"
	)
	BAPt_parse_arguments args "$@"

	local app title window_id timeout="${args[timeout]:-$DEFAULT_TIMEOUT}"

	_debug "Waiting for window title (${args[app]}, ${args[titlish]})..."
	while read -r line ; do
		window_id=$(echo "$line" | sed -n 's/^\([0-9]*\),.*/\1/p')
		app=$(_wf-msg get_window_app "$window_id")
		[[ $app != "${args[app]}" ]] && continue
		title=$(_wf-msg get_window_title "$window_id")
		if [[ $title =~ ${args[titlish]} ]]; then
			echo "$window_id"
			_debug "Matched window ($window_id) title change: (${args[app]}, ${args[titlish]})"
			break
		fi
	done < <(_wf-msg dbus-signal view_title_changed --timeout "$timeout")
}

function wait_for_window_creation {
	local args
	declare -A args=(
		[summary]='Wait for a window with the given title to come into existence'
		[0:app]="Application ID, eg; 'firefox'"
		[1:titlish]='Regex to match title of window'
		[--timeout]="How long to wait until quitting, default 15 seconds"
	)
	BAPt_parse_arguments args "$@"

	local app title window_id timeout="${args[timeout]:-$DEFAULT_TIMEOUT}"

	_debug "Waiting for window (${args[app]}, ${args[titlish]}) to be created..."
	while read -r window_id; do
		app=$(_wf-msg get_window_app "$window_id")
		[[ $app != "${args[app]}" ]] && continue
		title=$(_wf-msg get_window_title "$window_id")
		if [[ $title =~ ${args[titlish]} ]]; then
			echo "$window_id"
			_debug "Matched window creation: (${args[app]}, ${args[titlish]})"
			break
		fi
	done < <(_wf-msg dbus-signal view_added --timeout "$timeout")
	if [[ -n $window_id ]]; then
		return 0
	else
		return 1
	fi
}
