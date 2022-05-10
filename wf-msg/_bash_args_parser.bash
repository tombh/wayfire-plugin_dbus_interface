# shellcheck shell=bash

# Bash Arguments Parser by tombh (BAPt)
#
# Usage:
# ```
#   declare -A args=(
#   	[summary]="A short description of what this command or function does"
#   	[0:foo]="A foo argument, it's positional and required"
#   	[--bar:flag]="A boolean option that doesn't take a value"
#   	[--foobar]="An option that requires a value"
#   )
#   BAPt_parse_arguments args "$@"
#
#   echo "foo is ${args[foo]}. bar is ${args[foo]}. foobar is ${args[foobar]}"
# ```

__BAPt_ERROR_PREFIX='Error parsing arguments'
__BAPt_SCRIPT_NAME=$(basename "$0")

function BAPt_parse_arguments {
	local -n arg_defs="$1"
	shift
	local parent_args=("$@")
	local usage option_definitions parsed parts positionals options
	arg_defs+=([--help:flag]="Show help")

	# Why build $usage now rather than as and when it's needed?
	# $arg_defs, which is needded to build the usage string, is an associative array
	# which are hard to copy. So it's easier to maintain a copy of the prebuilt usage
	# rather than a copy of $arg_defs.
	usage=$(__BAPt_build_usage arg_defs)

	if [[ ${parent_args[*]} = "--help" ]]; then
		__BAPt_show_usage 0
	fi

	if [[ -n ${arg_defs[any]} ]]; then
		if [[ -z ${parent_args[*]} ]]; then
			echo "$__BAPt_ERROR_PREFIX: arguments expected" 1>&2
			echo 1>&2
			__BAPt_show_usage 1
		fi
		return 0
	fi

	__BAPt_parse
}

function __BAPt_parse {
	option_definitions=$(__BAPt_get_option_definitions)
	if ! parsed=$(
		getopt \
			-n "$__BAPt_ERROR_PREFIX" \
			--longoptions "$option_definitions" \
			-- _ "${parent_args[@]}"
	); then
		echo
		__BAPt_show_usage 1
	fi

	parts="$(echo "$parsed" | sed 's/ -- /\n/' | sed 's/ --$/\n/')"
	parts1=$(echo "$parts" | sed -n 1p)
	parts2=$(echo "$parts" | sed -n 2p)
	[[ $parts2 = "''" ]] && parts2=""

	if ! __BAPt_parse_positional_args "$parts2"; then
		__BAPt_show_usage 1
	fi

	if ! __BAPt_parse_options "$parts1"; then
		__BAPt_show_usage 1
	fi
}

function __BAPt_show_usage {
	local exit_code=$1
	if [[ $exit_code -gt 0 ]]; then
		echo "$usage" >&2
		exit "$exit_code"
	else
		echo "$usage"
		exit 0
	fi
}

function __BAPt_build_usage {
	local calling_function=${FUNCNAME[2]}
	local widest usage command_name positionals=() options=() description line arg_list=()

	widest=$(__BAPt_find_widest)

	__BAPt_extract_positionals_and_options

	if [[ -n $calling_function ]]; then
		command_name=$(basename "$calling_function")
	else
		command_name=$(basename "$__BAPt_SCRIPT_NAME")
	fi

	echo "Usage: $command_name ${arg_list[*]} [OPTIONS]"

	if [[ -n ${arg_defs[summary]} ]]; then
		echo
		echo "${arg_defs[summary]}"
	fi

	if [[ ${#positionals} -gt 0 ]]; then
		echo
		echo "Arguments:"
		for line in "${positionals[@]}"; do
			echo "$line"
		done
	fi

	if [[ ${#options} -gt 0 ]]; then
		echo
		echo "Options:"
		for line in "${options[@]}"; do
			echo "$line"
		done
	fi

	if [[ -n ${arg_defs[details]} ]]; then
		echo
		echo "${arg_defs[details]}"
	fi
}

function __BAPt_extract_positionals_and_options {
	for key in "${!arg_defs[@]}"; do
		description="${arg_defs[$key]}"
		if [[ $key =~ ^[0-9]: ]]; then
			name=${key/*:/}
			index=${key//:*/}
		else
			name=${key//:flag/}
		fi
		line=$(printf "%-${widest}s %s\n" "  $name" "$description")
		if [[ $key =~ ^[0-9]: ]]; then
			arg_list[$index]="$name"
			positionals[$index]=$line
		fi
		if [[ $key = any ]]; then
			arg_list[0]="[ARGUMENTS]"
			positionals[0]=$line
		fi
		if [[ $key =~ ^-- ]]; then
			options+=("$line")
		fi
	done
}

function __BAPt_find_widest {
	local widest=0
	for key in "${!arg_defs[@]}"; do
		key=${key//:flag/}
		width=${#key}
		if [[ $width -gt $widest ]]; then
			widest=$width
		fi
	done
	echo "$((widest + 2))"
}

# Convert an associative array into a `getopt`-compatible options definition
# Eg, from:
#   ([--foo]="bar" [--boolme]="")
# to:
#   "foo:,boolme"
function __BAPt_get_option_definitions {
	local value option_defs_string option_defs_array=()

	for key in "${!arg_defs[@]}"; do
		if [[ ! $key =~ ^-- ]]; then
			continue
		fi
		value="${arg_defs[$key]}"
		key="${key//--/}"
		if [[ ! $key =~ ":flag" ]]; then
			key="$key:"
		else
			key="${key//:flag/}"
		fi
		option_defs_array+=("$key")
	done
	option_defs_string=$(__BAPt_join_by ',' "${option_defs_array[@]}")

	echo "$option_defs_string"
}

function __BAPt_parse_positional_args {
	local parsed=$1
	declare -a "positionals=($parsed)"

	local index name arity=0
	for key in "${!arg_defs[@]}"; do
		if [[ $key =~ ^[0-9]: ]]; then
			index=${key//:*/}
			name=${key/*:/}
			arg_defs["$name"]="${positionals[index]}"
			unset 'arg_defs['"$key"']'
			arity=$((arity + 1))
		fi
	done

	if [[ ${#positionals[@]} -ne $arity ]]; then
		echo "$__BAPt_ERROR_PREFIX: Expected $arity got ${#positionals[@]}" >&2
		echo >&2
		return 1
	fi
}

function __BAPt_parse_options {
	local parsed=$1
	local value name
	declare -a "options=($parsed)"

	local index=0
	for item in "${options[@]}"; do
		if [[ $item =~ ^-- ]]; then
			name=${item/*--/}
			if [[ -n ${arg_defs[$item]} ]]; then
				value=${options[(($index + 1))]}
			else
				value=true
			fi
			arg_defs["$name"]="$value"
			unset 'arg_defs['"$item"']'
		fi
		index=$((index + 1))
	done
}

function __BAPt_join_by {
	local delimeter=${1-} field=${2-}
	if shift 2; then
		printf %s "$field" "${@/#/$delimeter}"
	fi
}
