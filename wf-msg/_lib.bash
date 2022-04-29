# shellcheck shell=bash

function _includes_path {
	dirname "$(readlink -f "$0")"
}

function _load_includes {
	for file in "$(_includes_path)"/*.bash; do
		# shellcheck disable=1090
		source "$file"
	done
}

function _is_function {
	local suspect=$1 cleaned
	# shellcheck disable=2001
	cleaned=$(echo "$suspect" | sed 's/^-*//')
	if [[ $(type -t "$cleaned") != function ]]; then
		return 1
	fi
}

function _init {
	local subcommand=$1 start_time elapsed
	shift

	local args=("$@")

	_load_includes

	if [[ "$subcommand" = "--help" ]]; then
		_help
		exit 0
	fi

	if ! _is_function "$subcommand"; then
		# shellcheck disable=2154
		echo "$__BAPt_ERROR_PREFIX: Unknown subcommand" 2>&1
		echo 2>&1
		_help
		exit 1
	fi

	start_time=$(date +%s%N)
	"$subcommand" "${args[@]}"
	elapsed=$((($(date +%s%N) - start_time) / 1000000))
	_debug "\`$subcommand ${args[*]}\` took ${elapsed}ms"
}

function _timestamp {
	date +"%H:%M:%S.%3N"
}

function _error {
	local message="$1"
	local caller=${FUNCNAME[1]}
	local prefix=""

	if [[ $DEBUG = 1 ]]; then
		prefix="$(_timestamp) $_LOG_IDENTIFIER  |ERROR: $caller() "
	fi
	# shellcheck disable=2001
	echo "$message" | sed "s/.*/$prefix&/" 1>&2
	kill -s TERM "$_TOP_PID"
	exit 1
}

function _debug {
	[[ "$DEBUG" != "1" ]] && return 0
	local message="$1"
	local caller=${FUNCNAME[1]}
	local prefix
	prefix="$(_timestamp) $_LOG_IDENTIFIER  |DEBUG: $caller()"
	# shellcheck disable=2001
	echo "$message" | sed "s/.*/$prefix &/" 1>&2
}

function _join_by {
	local d=${1-} f=${2-}
	if shift 2; then
		printf %s "$f" "${@/#/$d}"
	fi
}

function _trim {
	sed -e 's/\n//g' | awk '{$1=$1};1'
}
