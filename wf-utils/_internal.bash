# shellcheck shell=bash

export _LOG_IDENTIFIER="WF-UTILS"
export _PROGRAM_DESCRIPTION="Collection of general utilities for controling Wayfire"
export DEFAULT_TIMEOUT=15

function _wf-msg {
	local args=("$@")

	while read -r line; do
		if ! _is_empty_whitespace "$line"; then
			echo "$line"
		fi
	done < <(wf-msg "${args[@]}")
	test $? -eq 0 || _error "Error calling \`wf-msg\` ${args[*]}"
}

function _jq {
	local query="$1"
	shift

	if ! command -v jq &>/dev/null; then
		_error "The command \`jq\` is needed: https://stedolan.github.io/jq/download"
	fi
	jq "$query" --raw-output "$@"
}

function _get_x {
	local coords="$1"
	echo "$coords" | cut -d ' ' -f1
}

function _get_y {
	local coords="$1"
	echo "$coords" | cut -d ' ' -f2
}
