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
#   __BAPt_parse args "$@"
#   echo "foo is ${args[foo]}. bar is ${args[foo]}. foobar is ${args[foobar]}"
# ```

__BAPt_ERROR_PREFIX='Error parsing arguments'
__BAPt_SCRIPT_NAME=$(basename "$0")

function __BAPt_parse_arguments {
	local -n _arg_defs="$1"
	shift
	local parent_args=("$@")
	local usage option_definitions parsed parts positionals options
	_arg_defs+=([--help:flag]="Show help")

	# Why build $usage now rather than as and when it's needed?
	# $_arg_defs, which is needded to build the usage string, is an associative array
	# which are hard to copy. So it's easier to maintain a copy of the prebuilt usage
	# rather than a copy of $_arg_defs.
	usage=$(__BAPt_build_usage _arg_defs)

	option_definitions=$(__BAPt_get_option_definitions _arg_defs)
	if ! parsed=$(
		getopt \
			-n "$__BAPt_ERROR_PREFIX" \
			--longoptions "$option_definitions" \
			-- _ "${parent_args[@]}"
	); then
		echo
		__BAPt_show_usage "$usage" 1
	fi

	parts="$(echo "$parsed" | sed 's/ -- /\n/' | sed 's/ --$/\n/')"
	parts1=$(echo "$parts" | sed -n 1p)
	parts2=$(echo "$parts" | sed -n 2p)
	[[ $parts2 = "''" ]] && parts2=""

	if [[ $(echo "$parts1" | tr -d ' ') = "--help" ]]; then
		__BAPt_show_usage "$usage" 0
	fi

	if ! __BAPt_parse_positional_args _arg_defs "$parts2"; then
		__BAPt_show_usage "$usage" 1
	fi

	if ! __BAPt_parse_options _arg_defs "$parts1"; then
		__BAPt_show_usage "$usage" 1
	fi
}

function __BAPt_show_usage {
	local usage=$1
	local exit_code=$2
	if [[ $exit_code -gt 0 ]]; then
		echo "$usage" >&2
		exit "$exit_code"
	else
		echo "$usage"
		exit 0
	fi
}

function __BAPt_build_usage {
	local -n arg_defs_u="$1"
	local calling_function=${FUNCNAME[2]}
	local widest usage positionals=() options=() description line arg_list=()

	widest=$(__BAPt_find_widest arg_defs_u)

	for key in "${!arg_defs_u[@]}"; do
		description="${arg_defs_u[$key]}"
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
		if [[ $key =~ ^-- ]]; then
			options+=("$line")
		fi
	done

	if [[ -n $calling_function ]]; then
		command_name=$(basename "$calling_function")
	else
		command_name=$(basename "$__BAPt_SCRIPT_NAME")
	fi

	echo "Usage: $command_name ${arg_list[*]} [OPTIONS]"

	if [[ -n ${arg_defs_u[summary]} ]]; then
		echo
		echo "${arg_defs_u[summary]}"
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

	if [[ -n ${arg_defs_u[details]} ]]; then
		echo
		echo "${arg_defs_u[details]}"
	fi
}

function __BAPt_find_widest {
	local -n arg_defs_fw="$1"

	local widest=0
	for key in "${!arg_defs_fw[@]}"; do
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
	local -n arg_defs_gop="$1"
	local value option_defs_string option_defs_array=()

	for key in "${!arg_defs_gop[@]}"; do
		if [[ ! $key =~ ^-- ]]; then
			continue
		fi
		value="${arg_defs_gop[$key]}"
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
	local -n arg_defs_ppa="$1"
	local parsed=$2
	declare -a "positionals=($parsed)"

	local index name arity=0
	for key in "${!arg_defs_ppa[@]}"; do
		if [[ $key =~ ^[0-9]: ]]; then
			index=${key//:*/}
			name=${key/*:/}
			arg_defs_ppa["$name"]="${positionals[index]}"
			unset 'arg_defs_ppa['"$key"']'
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
	local -n arg_defs_po="$1"
	local parsed=$2
	local value name
	declare -a "options=($parsed)"

	local index=0
	for item in "${options[@]}"; do
		if [[ $item =~ ^-- ]]; then
			name=${item/*--/}
			if [[ -n ${arg_defs_po[$item]} ]]; then
				value=${options[(($index + 1))]}
			else
				value=true
			fi
			arg_defs_po["$name"]="$value"
			unset 'arg_defs_po['"$item"']'
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
