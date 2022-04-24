# shellcheck shell=bash

function _timestamp {
	date "+%Y-%m-%d %H:%M:%S"
}

function _debug {
	[[ "$DEBUG" != "1" ]] && return 0
	local caller=${FUNCNAME[1]}
	echo "$(_timestamp) WF-UTILS|DEBUG: $caller() $1" >&2
}

function _error {
	local message="$1"
	local caller=${FUNCNAME[1]}
	echo "$(_timestamp) WF-UTILS|ERROR: $caller() $message" >&2
	kill -s TERM "$TOP_PID"
}

function _wf-msg {
	local args=("$@")

	local result
	result=$(wf-msg "${args[@]}")
	test $? -eq 0 || _error "Error calling \`wf-msg\` ${args[*]}"
	echo "$result"
}

function _jq {
	local query="$1"
	shift

	if ! command -v jq &>/dev/null; then
		# shellcheck disable=2016
		_error 'The command `jq` is needed: https://stedolan.github.io/jq/download'
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
