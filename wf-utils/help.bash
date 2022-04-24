# shellcheck shell=bash

function help {
	: 'Display help'

	local cmds
	cmds=$(compgen -A function | grep -v -e '^_')

	for cmd in "${cmds[@]}"; do
		body=$(declare -f "$cmd" | sed '1,2d;$d')
		summary=$(_summary "$body")
		echo "$cmd $summary"
	done
}

function _summary {
	local body="$1"
	local summary
	summary=$(
		echo "$body" |
			head -n1 |
			sed -e "s/^ *: '//" |
			sed -e "s/';\$//"
	)
	echo "$summary"
}
