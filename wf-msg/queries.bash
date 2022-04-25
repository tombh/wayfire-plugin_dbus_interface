# shellcheck shell=bash

function get_all_window_ids {
	wf-call query_view_vector_taskman_ids | _o_numbers
}

function get_window_title {
	local window_id=$1
	wf-call query_view_title "$window_id" | _o_string
}

function get_window_app {
	local window_id=$1
	wf-call query_view_app_id "$window_id" | _o_string
}

function get_current_output {
	wf-call query_active_output | _o_numbers
}

function get_current_workspace {
	wf-call query_output_workspace "$(get_current_output)" | _o_numbers
}

function get_window_workspace {
	local window_id=$1
	wf-call query_view_workspaces "$window_id" | _o_numbers
}

function is_window_active {
	local window_id=$1
	wf-call query_view_active "$window_id" | _o_boolean
}

function get_all_windows {
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
