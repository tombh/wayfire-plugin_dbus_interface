# shellcheck shell=bash disable=2120

function get_all_window_ids {
	local _
	declare -A _=(
		[summary]="Get the IDs of all the current windows"
	)
	__BAPt_parse_arguments _ "$@"
	wf-call query_view_vector_taskman_ids | _o_numbers
}

function get_window_title {
	local args
	declare -A args=(
		[summary]="Get the title of the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call query_view_title "${args[window_id]}" | _o_string
}

function get_window_app {
	local args
	declare -A args=(
		[summary]="Get the application that launched the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call query_view_app_id "${args[window_id]}" | _o_string
}

function get_current_output {
	local _
	declare -A _=(
		[summary]="Get the ID of the current output (usually a physical monitor)"
	)
	__BAPt_parse_arguments _ "$@"
	wf-call query_active_output | _o_numbers
}

function get_current_workspace {
	local _
	declare -A _=(
		[summary]="Get the X,Y coords of the current workspace on the current output"
	)
	__BAPt_parse_arguments _ "$@"
	wf-call query_output_workspace "$(get_current_output)" | _o_numbers
}

function get_window_workspace {
	local args
	declare -A args=(
		[summary]="Get the X,Y coords of the given window"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call query_view_workspaces "${args[window_id]}" | _o_numbers
}

function is_window_active {
	local args
	declare -A args=(
		[summary]="Is the given window ID active? (I think that means focussed)"
		[0:window_id]="$__WF_MSG_WINDOW_ID_DESCRIPTION"
	)
	__BAPt_parse_arguments args "$@"
	wf-call query_view_active "${args[window_id]}" | _o_boolean
}

# TODO: This is slow because each window generates multiple extra calls to get its details
function get_all_windows {
	local _
	declare -A _=(
		[summary]="Get the full details of all current windows"
	)
	__BAPt_parse_arguments _ "$@"

	_json_array "$(
		read -ra wids <<<"$(get_all_window_ids)"
		for wid in "${wids[@]}"; do
			_json_object "$(
				_json_kv 'id' "$wid"
				_json_kv 'app' "$(get_window_app "$wid")"
				_json_kv 'title' "$(get_window_title "$wid")"
			)"
		done
	)" | _json_rm_trailing_comma
}
