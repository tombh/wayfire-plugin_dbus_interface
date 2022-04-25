# shellcheck shell=bash

function maximize_window {
	local window_id=$1
	wf-call maximize_view "$window_id" 1 | _o_none
}

function unmaximize_window {
	local window_id=$1
	wf-call maximize_view "$window_id" 0 | _o_none
}

function minimize_window {
	local window_id=$1
	wf-call minimize_view "$window_id" 1 | _o_none
}

function unminimize_window {
	local window_id=$1
	wf-call minimize_view "$window_id" 0 | _o_none
}

function focus_window {
	local window_id=$1
	wf-call focus_view "$window_id" 1 | _o_none
}

function unfocus_window {
	local window_id=$1
	wf-call focus_view "$window_id" 0 | _o_none
}

function move_window_to_workspace {
	local window_id=$1
	local workspace_x="$2"
	local workspace_y="$3"
	wf-call change_workspace_view "$window_id" "$workspace_x" "$workspace_y" | _o_none
}
