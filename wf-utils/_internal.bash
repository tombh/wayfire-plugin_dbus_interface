# shellcheck shell=bash

function _timestamp {
	date "+%Y-%m-%d %H:%M:%S"
}

function _debug {
	[[ "$DEBUG" != "1" ]] && return 0
	local message="$1"
	local caller=${FUNCNAME[1]}
	local prefix
	prefix="$(_timestamp) WF-UTILS|DEBUG: $caller()"
	# shellcheck disable=2001
	echo "$message" | sed "s/.*/$prefix &/" 1>&2
}

function _error {
	local message="$1"
	local caller=${FUNCNAME[1]}
	echo "$(_timestamp) WF-UTILS|ERROR: $caller() $message" >&2
	kill -s TERM "$TOP_PID"
	exit 1 # TODO: Why doesn't the `kill` above do this?
}

function _join_by {
	local d=${1-} f=${2-}
	if shift 2; then
		printf %s "$f" "${@/#/$d}"
	fi
}

function _wf-msg {
	local args=("$@")

	local result
	result=$(wf-msg "${args[@]}")
	test $? -eq 0 || _error "Error calling \`wf-msg\` ${args[*]}"
	echo -n "$result"
}

function _trim {
	sed -e 's/\n//g' | awk '{$1=$1};1'
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
