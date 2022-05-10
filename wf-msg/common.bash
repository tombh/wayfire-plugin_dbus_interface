# shellcheck shell=bash disable=2120

function dbus-method {
	local _
	declare -A _=(
		[summary]="Make a call to Wayfire's D-Bus interface"
		[details]="$(
			cat <<-EOM
				To see all the available commands and arguments, use:

				  \`wf-msg wf-dbus-introspect\`
			EOM
		)"
		[any]="Arguments to Wayfire D-Bus interface"
	)
	__BAPt_parse_arguments _ "$@"

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

function dbus-signal {
	local args
	declare -A args=(
		[summary]="Monitor Wayfire's D-Bus interface for specific activity"
		[details]="$(
			cat <<-EOM
				To see all the available signals, use:

				  \`wf-msg dbus-introspect\`
			EOM
		)"
		[0:signal]="Name of signal to monitor"
		[--timeout]="How long to wait until quitting, default 15 seconds"
	)
	__BAPt_parse_arguments args "$@"

	local prefix timeout="${args[timeout]:-15}"
	prefix=$(
		_escape_forward_slashes \
			"/org/wayland/compositor: org.wayland.compositor.${args[signal]} "
	)

	dbus_cmd=(
		gdbus
		monitor
		--session
		--object-path /org/wayland/compositor
		--dest org.wayland.compositor
	)

	_debug "Monitoring D-Bus for ${args[signal]}"
	while read -r line; do
		echo "$line" |
			grep "${args[signal]}" |
			sed "s/$prefix//" |
			_clean
	done < <(timeout "$timeout" "${dbus_cmd[@]}")
}

function dbus-introspect {
	local _
	declare -A _=(
		[summary]="Returns XML of all the available Wayfire D-Bus methods and signals"
	)
	__BAPt_parse_arguments _ "$@"

	dbus-send \
		--session \
		--type=method_call \
		--print-reply \
		--dest=org.wayland.compositor \
		/org/wayland/compositor \
		org.freedesktop.DBus.Introspectable.Introspect
}
