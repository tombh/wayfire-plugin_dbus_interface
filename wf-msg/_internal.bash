# shellcheck shell=bash

_LOG_IDENTIFIER="WF-MSG  "
_PROGRAM_DESCRIPTION="Control Wayfire from the CLI"
__WF_MSG_WINDOW_ID_DESCRIPTION="Window ID. See something like 'get_all_window_ids'"

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
