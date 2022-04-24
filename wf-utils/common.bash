# shellcheck shell=bash

function hide_window_when_done {
	local window="$1" # ID of window

	while [[ "$(_wf-msg is_window_active "$window")" == "false" ]]; do
		sleep 0.5
	done
	while [[ "$(_wf-msg is_window_active "$window")" == "true" ]]; do
		sleep 0.5
	done
	_wf-msg minimize_window "$window"
}

function find_window_by {
	: 'Find a window by searching for its name and title'
	local app="$1"     # Program name
	local titlish="$2" # Window title

	match=$(
		_wf-msg get_all_windows |
			_jq '
        .[] |
        select(.app=="'"$app"'") |
        select(.title|test("'"$titlish"'")) |
        .id
      ' | head -n1
	)

	_debug "matched '$app', '$titlish': window $match"
	echo "$match"
}
