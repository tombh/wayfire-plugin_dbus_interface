# shellcheck shell=bash

function _help {
	local _
	declare -A _=(
		[summary]="Show this help text"
	)
	__BAPt_parse_arguments _ "$@"

	local sub_commands widest
	readarray -t sub_commands <<<"$(_get_all_function_names | _exclude_private_functions)"
	widest=$(_get_widest "${sub_commands[@]}")

	_echo_usage_title
	_echo_subcommands
	_echo_options
}

function _echo_usage_title {
	# shellcheck disable=2154
	echo "Usage: $__BAPt_SCRIPT_NAME [SUBCOMMAND] [--help]"
	echo
}

function _echo_subcommands {
	echo "Subcommands:"
	for cmd in "${sub_commands[@]}"; do
		body=$(_get_contents_of_function "$cmd")
		summary=$(_summary "$body")
		line=$(printf "%-${widest}s %s\n" "  $cmd" "$summary")
		echo "$line"
	done
}

function _echo_options {
	echo
	echo "Options:"
	printf "%-${widest}s %s\n" "  --help" "Show this help"
}

function _get_all_function_names {
	compgen -A function
}

function _exclude_private_functions {
	grep -v -e '^_'
}

function _get_contents_of_function {
	local fn=$1
	declare -f "$fn"
}

function _summary {
	local body=$1
	local summary
	summary=$(
		echo "$body" | grep summary | sed -n 's/.*\[summary\]="\([^"]*\).*/\1/p'
	)
	echo "$summary"
}

function _get_widest {
	local items=("$@")
	local widest=0
	for item in "${items[@]}"; do
		width=${#item}
		if [[ $width -gt $widest ]]; then
			widest=$width
		fi
	done
	echo "$((widest + 2))"
}
