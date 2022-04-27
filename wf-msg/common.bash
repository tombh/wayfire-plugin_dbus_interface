# shellcheck shell=bash disable=2120

function wf-call {
	local method=$1
	shift
	local args=("$@")

	local args_with_commas
	local result

	args_with_commas=$(_join_by , "${args[@]}")
	_debug "Calling DBUS $method($args_with_commas)"

	dbus_args=(
		call
		--session
		--dest org.wayland.compositor
		--object-path /org/wayland/compositor
		--method org.wayland.compositor."$method"
	)

	[ ${#args[@]} -gt 0 ] && dbus_args+=("${args[@]}")

	if ! result=$(gdbus "${dbus_args[@]}" 2>&1); then
		_error "$result"
	else
		echo "$result"
	fi
}
