# shellcheck shell=bash

__WF_MSG_WINDOW_ID_DESCRIPTION="Window ID. See something like 'get_all_window_ids'"

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
	local subcommand=$1
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

	"$subcommand" "${args[@]}"
}

function _timestamp {
	date "+%Y-%m-%d %H:%M:%S"
}

function _error {
	local message="$1"
	local caller=${FUNCNAME[1]}
	local prefix
	prefix="$(_timestamp) WF-MSG  |ERROR: $caller()"
	# shellcheck disable=2001
	echo "$message" | sed "s/.*/$prefix &/" 1>&2
	kill -s TERM "$_TOP_PID"
}

function _debug {
	[[ "$DEBUG" != "1" ]] && return 0
	local message="$1"
	local caller=${FUNCNAME[1]}
	local prefix
	prefix="$(_timestamp) WF-MSG  |DEBUG: $caller()"
	# shellcheck disable=2001
	echo "$message" | sed "s/.*/$prefix &/" 1>&2
}

function _join_by {
	local d=${1-} f=${2-}
	if shift 2; then
		printf %s "$f" "${@/#/$d}"
	fi
}

function _extract_numbers {
	sed \
		-e 's/uint32//g' \
		-e 's/[^0-9]/ /g'
}

function _clean {
	sed \
		-e "s/^()$//g" \
		-e "s/^(['\"]//g" \
		-e "s/['\"],)$//g" \
		-e "s/^(//g" \
		-e "s/,)$//g"
}

function _trim {
	sed -e 's/\n//g' | awk '{$1=$1};1'
}

function _escape_double_quotes {
	local value=$1
	echo -n "${value//\"/\\\"}"
}

function _o_numbers {
	_extract_numbers | _trim
}

function _o_string {
	_clean | _trim
}

function _o_boolean {
	_clean | _trim
}

function _o_none {
	_clean | _trim
}

function _json_rm_trailing_comma {
	sed 's/,$//'
}

function _json_kv {
	local key="$1"
	local value="$2"

	value=$(_escape_double_quotes "$value")
	echo -n '"'"$key"'":"'"$value"'",'
}

function _json_object {
	local kvs="$1"
	echo -n "$kvs" | _json_rm_trailing_comma | sed 's/.*/{&},/'
}

function _json_array {
	local objects="$1"
	echo -n "$objects" | _json_rm_trailing_comma | sed 's/.*/[&],/'
}
